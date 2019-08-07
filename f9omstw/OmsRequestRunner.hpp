// \file f9omstw/OmsRequestRunner.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestRunner_hpp__
#define __f9omstw_OmsRequestRunner_hpp__
#include "f9omstw/OmsRequestTrade.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "fon9/RevPrint.hpp"

namespace f9omstw {

/// user(session) thread 的下單要求物件
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
   void RequestAbandon(OmsResource* res, OmsErrCode errCode);
   void RequestAbandon(OmsResource* res, OmsErrCode errCode, std::string reason);

   /// 檢查是否有回報權限, 由收到回報的人自主檢查.
   /// 檢查方式: IsEnumContains(pol.GetIvrAdminRights(), OmsIvRight::AllowAddReport);
   /// 若無權限, 返回前會先呼叫 this->RequestAbandon(nullptr, OmsErrCode_DenyAddReport);
   bool CheckReportRights(const OmsRequestPolicy& pol);
};
//--------------------------------------------------------------------------//
fon9_WARN_DISABLE_PADDING;
enum class OmsReportRunnerSt {
   Checking,
   /// OmsReportRunner::ReportAbandon(); 之後的狀態.
   Abandoned,
   /// 已收過的重複回報, e.g. 改量結果的 BeforeQty 相同.
   Duplicated,
   /// e.g. 已收到 Sending 回報, 再收到 Queuing 回報; 此時的 Queuing 回報狀態.
   Obsolete,
   /// e.g. 改價1(成功於09:01:01) => 改價2(成功於09:01:02);
   /// 先收到改價2回報, 再收到改價1回報; 此時的改價1回報狀態.
   Expired,
   /// 確定沒收過的回報(已經不可能是 Duplicated), 但仍需檢查是否為 Expired?
   /// 這類回報必定會在 Order 留下記錄.
   NotReceived,
};

class OmsReportRunner {
   fon9_NON_COPYABLE(OmsReportRunner);
public:
   OmsResource&         Resource_;
   OmsRequestSP         Report_;
   fon9::RevBufferList  ExLog_;
   OmsReportRunnerSt    RunnerSt_{};

   OmsReportRunner(OmsResource& res, OmsRequestRunner&& src)
      : Resource_(res)
      , Report_{std::move(src.Request_)}
      , ExLog_{std::move(src.ExLog_)} {
   }

   /// 如果傳回 nullptr, 則必定已經呼叫過 this->ReportAbandon();
   OmsOrdNoMap* GetOrdNoMap();

   /// 用 this->Report_->ReqUID_ 尋找原始下單要求.
   /// \retval !nullptr 表示找到了原本的 Request;
   ///   - this->RunnerSt_ == OmsReportRunnerSt::NotReceived; 找到了 Req, 但此筆 Rpt 確定沒收到過.
   ///   - this->RunnerSt_ 與呼叫前相同(預設為 OmsReportRunnerSt::Checking)
   /// \retval nullptr  表示沒找到原本的 Request;
   ///   - this->RunnerSt_ == OmsReportRunnerSt::Abandoned;
   ///   - this->RunnerSt_ 與呼叫前相同(預設為 OmsReportRunnerSt::Checking)
   const OmsRequestBase* SearchOrigRequestId();

   /// 拋棄此次回報, 僅使用 ExInfo 方式記錄 log.
   void ReportAbandon(fon9::StrView reason);
};
fon9_WARN_POP;
//--------------------------------------------------------------------------//
/// - 禁止使用 new 的方式建立 OmsRequestRunnerInCore;
///   只能用「堆疊變數」的方式, 並在解構時自動結束更新.
/// - 變動中的委託: this->OrderRaw_
///   - this->OrderRaw_.Order_->Tail() == &this->OrderRaw_;
///   - this->OrderRaw_.Request_->LastUpdated() 尚未設定成 &this->OrderRaw_;
///     所以 this->OrderRaw_.Request_->LastUpdated() 有可能仍是 nullptr;
/// - 解構時: ~OmsRequestRunnerInCore()
///   - this->Resource_.Backend_.OnAfterOrderUpdated(*this);
///     - 將 req, this->OrderRaw_ 加入 backend;
///     - 設定 this->OrderRaw_.Request_->SetLastUpdated(&this->OrderRaw_);
///   - this->OrderRaw_.Order().EndUpdate(this->OrderRaw_, &this->Resource_);
class OmsRequestRunnerInCore {
   fon9_NON_COPY_NON_MOVE(OmsRequestRunnerInCore);
public:
   OmsResource&         Resource_;
   OmsOrderRaw&         OrderRaw_;
   /// 例如: 儲存送給交易所的封包.
   fon9::RevBufferList  ExLogForUpd_;
   /// 例如: 儲存下單要求的原始封包.
   fon9::RevBufferList  ExLogForReq_;

   /// 收到下單要求: 準備進行下單流程.
   OmsRequestRunnerInCore(OmsResource& resource, OmsOrderRaw& ordRaw, fon9::RevBufferList&& exLogForReq, fon9::BufferNodeSize updExLogSize)
      : Resource_(resource)
      , OrderRaw_(ordRaw)
      , ExLogForUpd_{updExLogSize}
      , ExLogForReq_(std::move(exLogForReq)) {
   }

   /// 收到交易所回報時: 找回下單要求, 建立委託更新物件.
   template <class ExLogForUpdArg>
   OmsRequestRunnerInCore(OmsResource& resource, OmsOrderRaw& ordRaw, ExLogForUpdArg&& exLogForUpdArg)
      : Resource_(resource)
      , OrderRaw_(ordRaw)
      , ExLogForUpd_{std::forward<ExLogForUpdArg>(exLogForUpdArg)}
      , ExLogForReq_{0} {
   }
   OmsRequestRunnerInCore(OmsReportRunner&& src, OmsOrderRaw& ordRaw)
      : Resource_(src.Resource_)
      , OrderRaw_(ordRaw)
      , ExLogForUpd_{std::move(src.ExLog_)}
      , ExLogForReq_{0} {
      if (src.Report_.get() == &ordRaw.Request())
         this->ExLogForReq_ = std::move(this->ExLogForUpd_);
   }
   OmsRequestRunnerInCore(OmsResource& resource, OmsOrderRaw& ordRaw)
      : Resource_(resource)
      , OrderRaw_(ordRaw)
      , ExLogForUpd_{0}
      , ExLogForReq_{0} {
   }

   ~OmsRequestRunnerInCore();

   void Update(f9fmkt_TradingRequestSt reqst);
   void Update(f9fmkt_TradingRequestSt reqst, fon9::StrView cause) {
      this->OrderRaw_.Message_.assign(cause);
      this->Update(reqst);
   }
   void Reject(f9fmkt_TradingRequestSt reqst, OmsErrCode ec, fon9::StrView cause) {
      this->OrderRaw_.ErrCode_ = ec;
      this->Update(reqst, cause);
   }

   /// 使用 this->OrderRaw_.Order_->Initiator_->Policy()->OrdTeamGroupId(); 的櫃號設定.
   ///
   /// \retval true  已填妥 OrdNo.
   /// \retval false 已填妥了拒絕狀態.
   bool AllocOrdNo(OmsOrdNo reqOrdNo);
   /// 使用 tgId 的櫃號設定, 不考慮 Request policy.
   ///
   /// \retval true  已填妥 OrdNo.
   /// \retval false 已填妥了拒絕狀態.
   bool AllocOrdNo(OmsOrdTeamGroupId tgId);

   /// 在 this->OrderRaw_.OrdNo_.empty1st() 時, 分配委託書號.
   /// - 如果 !this->OrderRaw_.Order_->Initiator_->OrdNo_.empty1st()
   ///     || this->OrderRaw_.Order_->Initiator_->Policy()->OrdTeamGroupId() != 0;
   ///   也就是下單時有指定櫃號, 或 Policy 有指定櫃號群組.
   ///   則使用 this->AllocOrdNo(this->OrderRaw_.Order_->Initiator_->OrdNo_).
   /// - 否則使用 this->AllocOrdNo(tgId);
   ///
   /// \retval true  已填妥 OrdNo.
   /// \retval false 已填妥了拒絕狀態.
   bool AllocOrdNo_IniOrTgid(OmsOrdTeamGroupId tgId) {
      if (!this->OrderRaw_.OrdNo_.empty1st()) // 已編號.
         return true;
      assert(this->OrderRaw_.Request().RxKind() == f9fmkt_RxKind_RequestNew);
      assert(this->OrderRaw_.Order().Initiator() == &this->OrderRaw_.Request());
      auto iniReq = this->OrderRaw_.Order().Initiator();
      if (!iniReq->OrdNo_.empty1st() || iniReq->Policy()->OrdTeamGroupId() != 0)
         return this->AllocOrdNo(iniReq->OrdNo_);
      return this->AllocOrdNo(tgId);
   }
};
//--------------------------------------------------------------------------//
class OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(OmsRequestRunStep);
   OmsRequestRunStepSP  NextStep_;
protected:
   void ToNextStep(OmsRequestRunnerInCore&& runner) {
      if (this->NextStep_)
         this->NextStep_->RunRequest(std::move(runner));
   }

public:
   OmsRequestRunStep() = default;
   OmsRequestRunStep(OmsRequestRunStepSP next)
      : NextStep_{std::move(next)} {
   }
   virtual ~OmsRequestRunStep();

   virtual void RunRequest(OmsRequestRunnerInCore&&) = 0;
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
