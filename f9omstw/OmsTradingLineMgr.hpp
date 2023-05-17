// \file f9omstw/OmsTradingLineMgr.hpp
// \author fonwinz@gmail.com
#ifndef f9omstw_OmsTradingLineMgr_hpp__
#define f9omstw_OmsTradingLineMgr_hpp__
#include "fon9/fmkt/TradingLine.hpp"
#include "fon9/framework/IoManager.hpp"
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {
namespace f9fmkt = fon9::fmkt;

enum class TradingLineMgrState : uint8_t {
   /// 在 UnsendableReject 之後, 若為瞬斷, 應會在短時間內重新連線, 進入 Ready 狀態.
   /// 若一段時間內無法進入 Ready 狀態, 則會進入 UnsendableBroken 狀態.
   /// 需由外部主動呼叫 OmsTradingLineMgrBase::CheckUnsendableBroken(); 才會進入此狀態.
   /// 若沒有呼叫 OmsTradingLineMgrBase::CheckUnsendableBroken(); 則在斷線時, 僅會進入 UnsendableReject 狀態.
   UnsendableBroken = 0,
   /// 由 UnsendableReject 進入 UnsendableBroken 之前的暫時狀態.
   UnsendableBroken1 = 1,
   UnsendableReject = 2,
   Ready = 3,
};

class OmsTradingLineMgrBase {
   fon9_NON_COPY_NON_MOVE(OmsTradingLineMgrBase);

   TradingLineMgrState  State_{TradingLineMgrState::UnsendableBroken};
   char                 Padding____[3];
   OmsOrdTeamGroupId    OrdTeamGroupId_{};
   fon9::CharVector     OrdTeamConfig_;

   using Locker = f9fmkt::TradingLineManager::Locker;
   void SetOrdTeamGroupId(OmsResource& coreResource, const Locker&);
   OmsCoreSP IsNeedsUpdateOrdTeamConfig(fon9::CharVector ordTeamConfig, const Locker&);

   /// CurrRunner_ 可能為 Request 或 Report(因為可能會從 RerunRequest 而來)
   OmsRequestRunnerInCore* CurrRunner_{};
   OmsResource*            CurrOmsResource_{};
   OmsCoreSP               OmsCore_;

protected:
   void OnBeforeDestroy() {
      this->OmsCore_.reset();
   }
   bool SetOmsCore(OmsResource& coreResource, Locker&&);
   void SetState(TradingLineMgrState st, const Locker&) {
      this->State_ = st;
   }
   f9fmkt::SendRequestResult OnAskFor_SendRequest_FromLocal(f9fmkt::TradingRequest& req, const Locker& tsvrSelf, OmsTradingLineMgrBase& askerFrom);

   virtual void AddRef() const = 0;
   virtual void Release() const = 0;
   inline friend void intrusive_ptr_add_ref(const OmsTradingLineMgrBase* px) { px->AddRef(); }
   inline friend void intrusive_ptr_release(const OmsTradingLineMgrBase* px) { px->Release(); }
   using ThisSP = fon9::intrusive_ptr<OmsTradingLineMgrBase>;

   /// 將 SendReqQueue() 的要求移到 OmsCore;
   /// 到達 OmsCore 時, 執行: tsvr->SendReqQueue(&tsvr);
   void ToCore_SendReqQueue(Locker&& tsvr);

   /// 將 tsvr->ReqQueue_ 移出: ReqQueueMoveTo(reqs);
   /// 然後到 OmsCore 執行:拒絕排隊中的下單要求(f9fmkt_TradingRequestSt_QueuingCanceled/OmsErrCode_NoReadyLine).
   void ClearReqQueue(Locker&& tsvr, fon9::StrView cause, OmsCoreSP core);

   f9fmkt::SendRequestResult NoReadyLineReject(f9fmkt::TradingRequest& req, fon9::StrView cause);

public:
   const fon9::CharVector        StrQueuingIn_;
   f9fmkt::TradingLineManager&   ThisLineMgr_;

   /// 通常 name = ioargs.Name_; 用來建立排隊訊息: 「fon9_kCSTR_QueuingIn + name」.
   OmsTradingLineMgrBase(fon9::StrView name, f9fmkt::TradingLineManager& thisLineMgr);
   virtual ~OmsTradingLineMgrBase();

   #define fon9_kCSTR_QueuingIn    "Queuing in "
   fon9::StrView GetName() const {
      return fon9::StrView(this->StrQueuingIn_.begin() + sizeof(fon9_kCSTR_QueuingIn) - 1,
                           this->StrQueuingIn_.end());
   }

   TradingLineMgrState State() const {
      return this->State_;
   }
   Locker LockLineMgr() {
      return this->ThisLineMgr_.Lock();
   }
   /// 由於線路斷線有可能是瞬斷, 可以立即重新連線.
   /// 所以如果有需要判斷是否有線路可傳送, 應使用定時檢查機制, 避免瞬斷造成誤判.
   /// 例: 策略條件單正在等候, 瞬斷時不需要拒絕, 等確定無法連線, 才拒絕等候中的條件單.
   /// \retval true  由 [非 UnsendableBroken] 進入 [UnsendableBroken] 狀態.
   /// \retval false 檢查前已經是 UnsendableBroken 狀態, 或最後狀態沒有變成 UnsendableBroken;
   bool CheckUnsendableBroken();

   void OnOrdTeamConfigChanged(fon9::CharVector ordTeamConfig);
   
   OmsCoreSP OmsCore() const {
      return this->OmsCore_;
   }
   /// 您必須自行注意: 只有在 Running in OmsCore 時才能安全的取得 CurrRunner_;
   OmsRequestRunnerInCore* CurrRunner() const {
      return this->CurrRunner_;
   }
   /// 您必須自行注意: 只有在 Running in OmsCore 時才能安全的取得 CurrOmsResource_;
   /// 通常是在 OmsCore 裡面, 處理 ReqQueue 時會設定此值.
   OmsResource* CurrOmsResource() const {
      return this->CurrOmsResource_;
   }
   OmsRequestRunnerInCore* MakeRunner(fon9::DyObj<OmsInternalRunnerInCore>& tmpRunner,
                                      const OmsRequestBase& req,
                                      unsigned exLogForUpdArg) {
      if (fon9_LIKELY(this->CurrRunner_)) {
         // 從上一個步驟(例:風控檢查)而來的, 會來到這, 使用相同的 runner 繼續處理.
         assert(&this->CurrRunner_->OrderRaw().Request() == &req);
         return this->CurrRunner_;
      }
      // 來到這裡, 建立新的 runner:
      // 在 SendReqQueue() 進入 OmsCore 之後, 從 Queue 取出送單.
      assert(tmpRunner.get() == nullptr && this->CurrOmsResource_ != nullptr);
      return tmpRunner.emplace(*this->CurrOmsResource_,
                               *req.LastUpdated()->Order().BeginUpdate(req),
                               exLogForUpdArg);
   }
   void RunRequest(OmsRequestRunnerInCore&& runner, const Locker& tsvr);
   void RunRequest(OmsRequestRunnerInCore&& runner) {
      this->RunRequest(std::move(runner), this->LockLineMgr());
   }

   /// \retval true  已填妥 OrdNo.
   /// \retval false 已填妥了拒絕狀態.
   bool AllocOrdNo(OmsRequestRunnerInCore& runner, const OmsRequestTrade& iniReq) {
      return runner.AllocOrdNo_IniOrTgid(this->OrdTeamGroupId_, iniReq);
   }

   template <class OrderRawT, class RequestIniT, class RequestUptT>
   f9fmkt::TradingRequest::OpQueuingRequestResult OpQueuingRequest(RequestUptT& uReq, f9fmkt::TradingRequest& queuingRequest) {
      OmsRequestRunnerInCore* currRunner = this->CurrRunner_;
      if (!currRunner)
         return f9fmkt::TradingRequest::Op_NotSupported;
      if (queuingRequest.RxKind() != f9fmkt_RxKind_RequestNew || uReq.IniSNO() != queuingRequest.RxSNO())
         return f9fmkt::TradingRequest::Op_NotTarget;
      switch (uReq.RxKind()) {
      default:
      case f9fmkt_RxKind_Unknown:
      case f9fmkt_RxKind_RequestNew:
      case f9fmkt_RxKind_Filled:
      case f9fmkt_RxKind_Order:
      case f9fmkt_RxKind_Event:
      case f9fmkt_RxKind_RequestChgCond:
      case f9fmkt_RxKind_RequestRerun:
         return f9fmkt::TradingRequest::Op_NotSupported;

      case f9fmkt_RxKind_RequestDelete:
      case f9fmkt_RxKind_RequestChgQty:
      case f9fmkt_RxKind_RequestChgPri:
      case f9fmkt_RxKind_RequestQuery:
         break;
      }
      // 是否要針對 iniReq 建立一個 ordraw 呢?
      // auto* iniReq = dynamic_cast<RequestIniT*>(&queuingRequest);
      // if (iniReq == nullptr)
      //    return f9fmkt::TradingRequest::Op_NotTarget;
      // OmsRequestRunnerInCore iniRunner{currRunner->Resource_, *iniReq->LastUpdated()->Order().BeginUpdate(*iniReq)};
      // ...
      // iniRunner.Update(f9fmkt_TradingRequestSt_QueuingCanceled);
      // -----
      auto& ordraw = currRunner->OrderRawT<OrderRawT>();
      if (uReq.RxKind() == f9fmkt_RxKind_RequestQuery) {
         // 查詢, 不改變任何狀態: 讓回報機制直接回覆最後的 OrderRaw 即可.
         currRunner->Update(f9fmkt_TradingRequestSt_Done);
         return f9fmkt::TradingRequest::Op_ThisDone;
      }
      if (uReq.RxKind() == f9fmkt_RxKind_RequestChgPri) {
         ordraw.LastPri_ = uReq.Pri_;
         ordraw.LastPriType_ = uReq.PriType_;
         currRunner->Update(f9fmkt_TradingRequestSt_Done);
         return f9fmkt::TradingRequest::Op_ThisDone;
      }
      assert(uReq.RxKind() == f9fmkt_RxKind_RequestChgQty || uReq.RxKind() == f9fmkt_RxKind_RequestDelete);
      CalcInternalChgQty(ordraw, uReq.Qty_);
      if (ordraw.AfterQty_ == 0) {
         ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_NewQueuingCanceled;
         currRunner->Update(f9fmkt_TradingRequestSt_QueuingCanceled);
         return f9fmkt::TradingRequest::Op_AllDone;
      }
      currRunner->Update(f9fmkt_TradingRequestSt_Done);
      return f9fmkt::TradingRequest::Op_ThisDone;
   }

   template <typename DstT, typename SrcQtyT>
   static void CalcInternalChgQty(DstT& dst, SrcQtyT src) {
      if (src >= 0) {
         dst.AfterQty_ = dst.LeavesQty_ = fon9::unsigned_cast(src);
      }
      else {
         dst.AfterQty_ = dst.LeavesQty_ = static_cast<decltype(dst.LeavesQty_)>(dst.LeavesQty_ + src);
         if (fon9::signed_cast(dst.AfterQty_) < 0) {
            dst.AfterQty_ = dst.LeavesQty_ = 0;
         }
      }
   }

   /// 通常支援本地 Asker SendReqQueue 的時候.
   /// assert(req.LastUpdated() != nullptr);
   f9fmkt::SendRequestResult OnAskFor_SendRequest_InCore(f9fmkt::TradingRequest& req, OmsResource& resource);
   f9fmkt::SendRequestResult OnAskFor_SendRequest_ByRunner(OmsInternalRunnerInCore& runner);
   void InCore_SendReqQueue(OmsResource& resource);
   void OnOfferSendable(OmsResource& resource, f9fmkt::TradingLineManager::FnHelpOffer&& fnOffer);
   void OnHelperBroken(OmsResource& resource);
};

/// TradingLineMgrBaseT 必定衍生自 fon9::fmkt::TradingLineManager;
template <class TradingLineMgrBaseT>
class OmsTradingLineMgrT : public TradingLineMgrBaseT
                         , public OmsTradingLineMgrBase {
   fon9_NON_COPY_NON_MOVE(OmsTradingLineMgrT);
   using base = TradingLineMgrBaseT;
   using baseOmsTradingLineMgr = OmsTradingLineMgrBase;

protected:
   inline friend void intrusive_ptr_add_ref(const OmsTradingLineMgrT* px) { intrusive_ptr_add_ref(static_cast<const base*>(px)); }
   inline friend void intrusive_ptr_release(const OmsTradingLineMgrT* px) { intrusive_ptr_release(static_cast<const base*>(px)); }
   void AddRef() const override { intrusive_ptr_add_ref(this); }
   void Release() const override { intrusive_ptr_release(this); }

   using Locker = typename base::Locker;
   
   void OnBeforeDestroy() override {
      base::OnBeforeDestroy();
      baseOmsTradingLineMgr::OnBeforeDestroy();
   }
   f9fmkt::SendRequestResult NoReadyLineReject(f9fmkt::TradingRequest& req, fon9::StrView cause) override {
      return this->baseOmsTradingLineMgr::NoReadyLineReject(req, cause);
   }
   void ClearReqQueue(Locker&& tsvr, fon9::StrView cause) override {
      assert(this->use_count() != 0); // 必定要在解構前呼叫 ClearReqQueue();
      this->SetState(TradingLineMgrState::UnsendableReject, tsvr);
      this->baseOmsTradingLineMgr::ClearReqQueue(std::move(tsvr), cause, this->OmsCore());
   }
   // 把 tsvr.ReqQueue_ 的傳送需求轉移到 OmsCore 執行.
   void SendReqQueue(Locker&& tsvr) override {
      this->ToCore_SendReqQueue(std::move(tsvr));
   }
public:
   fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list
   template <class... ArgsT>
   OmsTradingLineMgrT(const fon9::IoManagerArgs& ioargs, ArgsT&&... args)
      : base(ioargs, std::forward<ArgsT>(args)...)
      , baseOmsTradingLineMgr(&ioargs.Name_, *this) {
   }
   fon9_MSC_WARN_POP;

   /// 通常由 LineGroup 接收 OmsCore 的事件, 再分配到各個 LineMgr, 然後呼叫此處.
   void OnOmsCoreChanged(OmsResource& coreResource) {
      if (this->SetOmsCore(coreResource, Locker{this->TradingSvr_})) {
         // 交易日改變了, 交易線路應該要刪除後重建, 因為大部分的交易線路 log 必須換日.
         this->OnTDayChanged(coreResource.Core_.TDay(), "OmsCore.TDayChanged");
         // 如果在 tsvr unlock 之後, IoManagerTree::DisposeAndReopen() 之前.
         // IoManagerTree 進入 EmitOnTimer() 開啟了 devices;
         // 在進入 IoManagerTree::DisposeAndReopen() 時, 會立即關閉!
         // => 程式啟動初期, 先不啟動 Timer, 等首次設定 OmsCore 時啟動.
         // => 程式運行後的換日, 如果真的發生上述情況, 那就讓 device 關閉後再開吧.
      }
   }
   void OnNewTradingLineReady(f9fmkt::TradingLine* src, Locker&& tsvr) override {
      this->SetState(TradingLineMgrState::Ready, tsvr);
      base::OnNewTradingLineReady(src, std::move(tsvr));
   }
   void OnHelperReady(Locker&& tsvr) override {
      this->SetState(TradingLineMgrState::Ready, tsvr);
      base::OnHelperReady(std::move(tsvr));
   }
   f9fmkt::SendRequestResult OnAskFor_SendRequest_FromLocal(f9fmkt::TradingRequest& req, const Locker& tsvrSelf, const Locker& tsvrAsker) override {
      assert(dynamic_cast<OmsTradingLineMgrT*>(&tsvrAsker->GetOwner()) != nullptr);
      auto* askerFrom = static_cast<OmsTradingLineMgrT*>(&tsvrAsker->GetOwner());
      return baseOmsTradingLineMgr::OnAskFor_SendRequest_FromLocal(req, tsvrSelf, *askerFrom);
   }
};

} // namespaces
#endif//__f9omstw_OmsTradingLineMgr_hpp__
