// \file f9omstw/OmsRequestRunner.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestRunner_hpp__
#define __f9omstw_OmsRequestRunner_hpp__
#include "f9omstw/OmsRequestTrade.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "fon9/RevPrint.hpp"

namespace f9omstw {

/// user(session) thread 的下單要求(包含回報)物件
class OmsRequestRunner {
   fon9_NON_COPYABLE(OmsRequestRunner);
public:
   OmsRequestSP         Request_;
   fon9::RevBufferList  ExLog_;

   OmsRequestRunner() : ExLog_{256} {
   }
   OmsRequestRunner(fon9::StrView exlog) : ExLog_{static_cast<fon9::BufferNodeSize>(exlog.size() + 128)} {
      fon9::RevPrint(this->ExLog_, exlog, '\n');
   }

   OmsRequestRunner(OmsRequestRunner&&) = default;
   OmsRequestRunner& operator=(OmsRequestRunner&&) = default;

   /// \copydoc bool OmsRequestBase::ValidateInUser(OmsRequestRunner&);
   bool ValidateInUser() {
      assert(!this->Request_->IsReportIn());
      assert(dynamic_cast<OmsRequestTrade*>(this->Request_.get()) != nullptr);
      return static_cast<OmsRequestTrade*>(this->Request_.get())->ValidateInUser(*this);
   }
   OmsOrderRaw* BeforeReqInCore(OmsResource& res) {
      assert(!this->Request_->IsReportIn());
      assert(dynamic_cast<OmsRequestTrade*>(this->Request_.get()) != nullptr);
      return static_cast<OmsRequestTrade*>(this->Request_.get())->BeforeReqInCore(*this, res);
   }
   void RequestAbandon(OmsResource* res, OmsErrCode errCode, std::string reason = std::string{}) {
      this->Request_->Abandon(errCode, std::move(reason), res, &this->ExLog_);
   }
   void RequestAbandon(OmsResource* res, OmsErrCode errCode, std::nullptr_t) = delete;

   /// 檢查是否有回報補單權限, 由收到回報補單的人自主檢查.
   /// 檢查方式: IsEnumContains(pol.GetIvrAdminRights(), OmsIvRight::AllowAddReport);
   /// 若無權限, 返回前會先呼叫 this->RequestAbandon(nullptr, OmsErrCode_DenyAddReport);
   bool CheckReportRights(const OmsRequestPolicy& pol);
};

/// 如果 rpt->ReqUID_ 使用此字串開頭(含EOS), 則後續緊接著 OmsRxSNO 指定回報.
#define f9omstw_kCSTR_ForceReportReqUID  "SNO.bin"
/// 設定 rpt 的回報原始「下單要求」序號.
/// 適用於「立即等候回報」的情況, 例如: 台灣證券TMP下單協定.
static inline void OmsForceAssignReportReqUID(OmsRequestId& rpt, OmsRxSNO sno) {
   memcpy(rpt.ReqUID_.Chars_, f9omstw_kCSTR_ForceReportReqUID, sizeof(f9omstw_kCSTR_ForceReportReqUID));
   memcpy(rpt.ReqUID_.Chars_ + sizeof(f9omstw_kCSTR_ForceReportReqUID), &sno, sizeof(sno));
   static_assert(rpt.ReqUID_.size() >= sizeof(f9omstw_kCSTR_ForceReportReqUID) + sizeof(sno),
                 "OmsRequestId::ReqUID_ too small.");
}
static inline bool OmsParseForceReportReqUID(const OmsRequestId& rpt, OmsRxSNO& sno) {
   if (fon9_LIKELY(memcmp(rpt.ReqUID_.Chars_, f9omstw_kCSTR_ForceReportReqUID, sizeof(f9omstw_kCSTR_ForceReportReqUID)) != 0))
      return false;
   memcpy(&sno, rpt.ReqUID_.Chars_ + sizeof(f9omstw_kCSTR_ForceReportReqUID), sizeof(sno));
   return true;
}

//--------------------------------------------------------------------------//
/// - 禁止使用 new 的方式建立 OmsRequestRunnerInCore;
///   只能用「堆疊變數」的方式, 並在解構時自動結束更新.
/// - 變動中的委託: this->OrderRaw_
///   - this->OrderRaw_.Order_->Tail() == &this->OrderRaw_;
///   - this->OrderRaw_.Request_->LastUpdated() 尚未設定成 &this->OrderRaw_;
///     所以 this->OrderRaw_.Request_->LastUpdated() 有可能仍是 nullptr;
/// - ~OmsRequestRunnerInCore() 或 ForceFinish():
///   - this->Resource_.Backend_.OnBefore_Order_EndUpdate(*this);
///     - 將 req, this->OrderRaw_ 加入 backend;
///     - 設定 this->OrderRaw_.Request_->SetLastUpdated(&this->OrderRaw_);
///   - this->OrderRaw_.Order().EndUpdate(*this);
class OmsRequestRunnerInCore {
   fon9_NON_COPY_NON_MOVE(OmsRequestRunnerInCore);
   OmsOrderRaw*         OrderRaw_;
public:
   OmsResource&         Resource_;
   /// 例如: 儲存送給交易所的封包.
   fon9::RevBufferList  ExLogForUpd_;
   /// 例如: 儲存下單要求的原始封包.
   fon9::RevBufferList  ExLogForReq_;
   void*                BackendLocker_{nullptr};

   /// 收到下單要求: 準備進行下單流程.
   OmsRequestRunnerInCore(OmsResource& resource, OmsOrderRaw& ordRaw, fon9::RevBufferList&& exLogForReq, fon9::BufferNodeSize updExLogSize)
      : OrderRaw_(&ordRaw)
      , Resource_(resource)
      , ExLogForUpd_{updExLogSize}
      , ExLogForReq_(std::move(exLogForReq)) {
   }

   /// 收到交易所回報時: 找回下單要求, 建立委託更新物件.
   template <class ExLogForUpdArg>
   OmsRequestRunnerInCore(OmsResource& resource, OmsOrderRaw& ordRaw, ExLogForUpdArg&& exLogForUpdArg)
      : OrderRaw_(&ordRaw)
      , Resource_(resource)
      , ExLogForUpd_{std::forward<ExLogForUpdArg>(exLogForUpdArg)}
      , ExLogForReq_{0} {
   }
   OmsRequestRunnerInCore(OmsResource& resource, OmsOrderRaw& ordRaw)
      : OrderRaw_(&ordRaw)
      , Resource_(resource)
      , ExLogForUpd_{0}
      , ExLogForReq_{0} {
   }
   /// 接續前一次執行完畢後, 的繼續執行.
   /// prevRunner 可能是同一個 order, 也可能是 child 或 parent;
   OmsRequestRunnerInCore(const OmsRequestRunnerInCore& prevRunner, OmsOrderRaw& ordRaw)
      : OrderRaw_(&ordRaw)
      , Resource_(prevRunner.Resource_)
      , ExLogForUpd_{0}
      , ExLogForReq_{0}
      , BackendLocker_{prevRunner.BackendLocker_} {
   }
   OmsRequestRunnerInCore(OmsResource& resource, OmsOrderRaw& ordRaw, const OmsRequestRunnerInCore* prevRunner)
      : OrderRaw_(&ordRaw)
      , Resource_(resource)
      , ExLogForUpd_{0}
      , ExLogForReq_{0}
      , BackendLocker_{prevRunner ? prevRunner->BackendLocker_ : nullptr} {
   }

   ~OmsRequestRunnerInCore();

   /// 強制流程結束, 並強制結束 this->OrderRaw_ 的更新.
   /// - 在 ForceFinish() 之後, this->OrderRaw_ 會變成 nullptr;
   ///   所以在此之後 this 就只能解構, 不可再使用 this 的任何 members, 否則 crash!
   /// - 使用 ForceFinish() 的時機:
   ///   - 讓 this->OrderRaw_->RxSNO() 有效.
   ///   - 讓 req.LastUpdated() 設定為 this->OrderRaw_;
   ///   - 且不再需要 this;
   void ForceFinish();

   OmsOrderRaw& OrderRaw() {
      assert(this->OrderRaw_ != nullptr);
      return *this->OrderRaw_;
   }
   const OmsOrderRaw& OrderRaw() const {
      assert(this->OrderRaw_ != nullptr);
      return *this->OrderRaw_;
   }
   template <class RawT>
   RawT& OrderRawT() {
      assert(dynamic_cast<RawT*>(this->OrderRaw_) != nullptr);
      return *static_cast<RawT*>(this->OrderRaw_);
   }
   template <class RawT>
   const RawT& OrderRawT() const {
      assert(dynamic_cast<const RawT*>(this->OrderRaw_) != nullptr);
      return *static_cast<const RawT*>(this->OrderRaw_);
   }

   /// - 更新 this->OrderRaw_.RequestSt_ = reqst;
   /// - 如果是新單要求的異動
   ///   - 若有必要, 會根據 reqst 更新 this->OrderRaw_.UpdateOrderSt_;
   ///   - 若為新單拒絕, 則會呼叫 this->OrderRaw_.OnOrderReject();
   void Update(f9fmkt_TradingRequestSt reqst);
   void Update(f9fmkt_TradingRequestSt reqst, fon9::StrView cause) {
      this->OrderRaw_->Message_.assign(cause);
      this->Update(reqst);
   }
   void Reject(f9fmkt_TradingRequestSt reqst, OmsErrCode ec, fon9::StrView cause) {
      this->OrderRaw_->ErrCode_ = ec;
      this->Update(reqst, cause);
   }
   void UpdateSt(f9fmkt_OrderSt ordst, f9fmkt_TradingRequestSt reqst) {
      this->OrderRaw_->UpdateOrderSt_ = ordst;
      this->Update(reqst);
   }

   /// 使用 this->OrderRaw_.Order_->Initiator_->Policy()->OrdTeamGroupId(); 的櫃號設定.
   /// \retval true  已填妥 OrdNo.
   /// \retval false 已填妥了拒絕狀態.
   bool AllocOrdNo(OmsOrdNo reqOrdNo);
   /// 使用 tgId 的櫃號設定, 不考慮 Request policy.
   /// \retval true  已填妥 OrdNo.
   /// \retval false 已填妥了拒絕狀態.
   bool AllocOrdNo(OmsOrdTeamGroupId tgId);

   /// 在 OmsIsOrdNoEmpty(this->OrderRaw_.OrdNo_) 時, 分配委託書號.
   /// - 如果 !OmsIsOrdNoEmpty(this->OrderRaw_.Order_->Initiator_->OrdNo_)
   ///     || this->OrderRaw_.Order_->Initiator_->Policy()->OrdTeamGroupId() != 0;
   ///   也就是下單時有指定櫃號, 或 Policy 有指定櫃號群組.
   ///   則使用 this->AllocOrdNo(this->OrderRaw_.Order_->Initiator_->OrdNo_).
   /// - 否則使用 this->AllocOrdNo(tgId);
   ///
   /// \retval true  已填妥 OrdNo.
   /// \retval false 已填妥了拒絕狀態.
   bool AllocOrdNo_IniOrTgid(OmsOrdTeamGroupId tgId) {
      if (!OmsIsOrdNoEmpty(this->OrderRaw_->OrdNo_)) // 已編號.
         return true;
      assert(this->OrderRaw_->Request().RxKind() == f9fmkt_RxKind_RequestNew);
      assert(this->OrderRaw_->Order().Initiator() == &this->OrderRaw_->Request());
      auto iniReq = this->OrderRaw_->Order().Initiator();
      if (!OmsIsOrdNoEmpty(iniReq->OrdNo_) || iniReq->Policy()->OrdTeamGroupId() != 0)
         return this->AllocOrdNo(iniReq->OrdNo_);
      return this->AllocOrdNo(tgId);
   }

   /// - retval=0 刪單.
   /// - retval<0 期望的改後數量有誤: 不可增量, 或期望的改後數量與現在(LeavesQty+CumQty)相同.
   /// - retval>0 isWantToKill==true:要刪除的數量; isWantToKill==false:改後的數量;
   template <typename QtyU, typename QtyS = fon9::make_signed_t<QtyU>>
   QtyS GetRequestChgQty(QtyS reqQty, bool isWantToKill, QtyU leavesQty, QtyU cumQty) {
      fon9_GCC_WARN_DISABLE("-Wconversion");
      if (reqQty < 0) {
         if (fon9_LIKELY(isWantToKill)) {
            if (fon9::unsigned_cast(reqQty = -reqQty) >= leavesQty)
               return 0;
            return reqQty;
         }
         if ((reqQty += fon9::signed_cast(leavesQty)) <= 0)
            return 0;
         return reqQty;
      }
      if (reqQty > 0) {
         if (fon9_LIKELY(isWantToKill)) {
            if ((reqQty = fon9::signed_cast(leavesQty + cumQty) - reqQty) <= 0) {
               this->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_ChgQty, nullptr);
               return -1;
            }
         }
         return reqQty;
      }
      fon9_GCC_WARN_POP;
      // 改量時 reqQty = 0 = 刪單.
      return 0;
   }
};

class OmsInternalRunnerInCore_Ctor {
   OmsInternalRunnerInCore_Ctor();
   friend class OmsInternalRunnerInCore;
};
/// - 使用 OmsInternalRunnerInCore 必定為內部更新, 所以建構時 OrderRaw().SetForceInternal();
class OmsInternalRunnerInCore : public OmsRequestRunnerInCore, public OmsInternalRunnerInCore_Ctor {
   fon9_NON_COPY_NON_MOVE(OmsInternalRunnerInCore);
   using base = OmsRequestRunnerInCore;
public:
   using base::base;

};
inline OmsInternalRunnerInCore_Ctor::OmsInternalRunnerInCore_Ctor() {
   static_cast<OmsInternalRunnerInCore*>(this)->OrderRaw().SetForceInternal();
}
//--------------------------------------------------------------------------//
class OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(OmsRequestRunStep);
   OmsRequestRunStepSP  NextStep_;
protected:
   void ToNextStep(OmsRequestRunnerInCore&& runner) {
      if (this->NextStep_)
         this->NextStep_->RunRequest(std::move(runner));
   }
   /// 當換日時, 由 OmsCoreMgr 主動通知,
   /// 在 OmsCoreMgr.TDayChangedEvent_ 之前呼叫.
   /// 預設: do nothing;
   virtual void OnCurrentCoreChangedImpl(OmsCore&);

public:
   OmsRequestRunStep() = default;
   OmsRequestRunStep(OmsRequestRunStepSP next)
      : NextStep_{std::move(next)} {
   }
   virtual ~OmsRequestRunStep();

   virtual void RunRequest(OmsRequestRunnerInCore&&) = 0;
   /// 當換日時, 由 OmsCoreMgr 主動通知.
   /// 衍生者可透過 void OnCurrentCoreChangedImpl(OmsCore&) override; 處理.
   /// 在這裡會繼續呼叫下一步驟的 OnCurrentCoreChanged();
   void OnCurrentCoreChanged(OmsCore& core) {
      this->OnCurrentCoreChangedImpl(core);
      if (this->NextStep_)
         this->NextStep_->OnCurrentCoreChanged(core);
   }

   /// 透過 OmsErrCodeAct.cfg 設定某些錯誤碼在特定條件下,
   /// 必須重跑一次流程, 透過這處理.
   /// 預設: `if (this->NextStep_) this->NextStep_->RerunRequest(std::move(runner));`
   virtual void RerunRequest(OmsReportRunnerInCore&& runner);
};
//--------------------------------------------------------------------------//
inline bool OmsRequestIni::BeforeReq_CheckIvRight(OmsRequestRunner& runner, OmsResource& res) const {
   assert(runner.Request_.get() != this);
   assert(this->LastUpdated() != nullptr);
   return this->CheckIvRight(runner, res, this->LastUpdated()->Order().ScResource())
      != OmsIvRight::DenyAll;
}

} // namespaces
#endif//__f9omstw_OmsRequestRunner_hpp__
