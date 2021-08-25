// \file f9omstw/OmsTradingLineMgr.hpp
// \author fonwinz@gmail.com
#ifndef f9omstw_OmsTradingLineMgr_hpp__
#define f9omstw_OmsTradingLineMgr_hpp__
#include "fon9/fmkt/TradingLine.hpp"
#include "fon9/framework/IoManager.hpp"
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {
namespace f9fmkt = fon9::fmkt;

class OmsTradingLineMgrBase {
   fon9_NON_COPY_NON_MOVE(OmsTradingLineMgrBase);

   OmsOrdTeamGroupId       OrdTeamGroupId_{};
   char                    Padding____[4];
   fon9::CharVector        OrdTeamConfig_;

   using Locker = f9fmkt::TradingLineManager::Locker;
   void SetOrdTeamGroupId(OmsResource& coreResource, const Locker&);
   OmsCoreSP IsNeedsUpdateOrdTeamConfig(fon9::CharVector ordTeamConfig, const Locker&);

protected:
   OmsRequestRunnerInCore* CurrRunner_{};
   OmsResource*            CurrOmsResource_{};
   OmsCoreSP               OmsCore_;

   bool SetOmsCore(OmsResource& coreResource, Locker&&);
   f9fmkt::SendRequestResult NoReadyLineReject(f9fmkt::TradingRequest& req, fon9::StrView cause);
   static void ClearReqQueue(Locker&& tsvr, fon9::StrView cause, OmsCoreSP core);

   virtual Locker Lock() = 0;
   virtual void AddRef() const = 0;
   virtual void Release() const = 0;
   inline friend void intrusive_ptr_add_ref(const OmsTradingLineMgrBase* px) { px->AddRef(); }
   inline friend void intrusive_ptr_release(const OmsTradingLineMgrBase* px) { px->Release(); }
   using ThisSP = fon9::intrusive_ptr<OmsTradingLineMgrBase>;

public:
   const fon9::CharVector  StrQueuingIn_;

   /// 通常 name = ioargs.Name_; 用來建立排隊訊息: 「"Queuing in " + name」.
   OmsTradingLineMgrBase(fon9::StrView name);
   virtual ~OmsTradingLineMgrBase();

   void OnOrdTeamConfigChanged(fon9::CharVector ordTeamConfig);
   
   OmsCoreSP OmsCore() const {
      return this->OmsCore_;
   }

   OmsRequestRunnerInCore* MakeRunner(fon9::DyObj<OmsRequestRunnerInCore>& tmpRunner,
                                      const OmsRequestBase& req,
                                      unsigned exLogForUpdArg) {
      if (fon9_LIKELY(this->CurrRunner_)) {
         // 從上一個步驟(例:風控檢查)而來的, 會來到這, 使用相同的 runner 繼續處理.
         assert(&this->CurrRunner_->OrderRaw_.Request() == &req);
         return this->CurrRunner_;
      }
      // 來到這裡, 建立新的 runner:
      // 從 OnNewTradingLineReady() 進入 OmsCore 之後, 從 Queue 取出送單.
      assert(tmpRunner.get() == nullptr && this->CurrOmsResource_ != nullptr);
      return tmpRunner.emplace(*this->CurrOmsResource_,
                               *req.LastUpdated()->Order().BeginUpdate(req),
                               exLogForUpdArg);
   }
   /// \retval true  已填妥 OrdNo.
   /// \retval false 已填妥了拒絕狀態.
   bool AllocOrdNo(OmsRequestRunnerInCore& runner) {
      return runner.AllocOrdNo_IniOrTgid(this->OrdTeamGroupId_);
   }
   /// 您必須自行注意: 只有在 Running in OmsCore 時才能安全的取得 CurrRunner_;
   OmsRequestRunnerInCore* CurrRunner() const {
      return this->CurrRunner_;
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
      assert(dynamic_cast<OrderRawT*>(&currRunner->OrderRaw_) != nullptr);
      auto* ordraw = static_cast<OrderRawT*>(&currRunner->OrderRaw_);
      if (uReq.RxKind() == f9fmkt_RxKind_RequestQuery) {
         // 查詢, 不改變任何狀態: 讓回報機制直接回覆最後的 OrderRaw 即可.
         currRunner->Update(f9fmkt_TradingRequestSt_Done);
         return f9fmkt::TradingRequest::Op_ThisDone;
      }
      if (uReq.RxKind() == f9fmkt_RxKind_RequestChgPri) {
         ordraw->LastPri_ = uReq.Pri_;
         ordraw->LastPriType_ = uReq.PriType_;
         currRunner->Update(f9fmkt_TradingRequestSt_Done);
         return f9fmkt::TradingRequest::Op_ThisDone;
      }
      assert(uReq.RxKind() == f9fmkt_RxKind_RequestChgQty || uReq.RxKind() == f9fmkt_RxKind_RequestDelete);
      CalcInternalChgQty(*ordraw, uReq.Qty_);
      if (ordraw->AfterQty_ == 0) {
         ordraw->UpdateOrderSt_ = f9fmkt_OrderSt_NewQueuingCanceled;
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
};

template <class TradingLineMgrBaseT>
class OmsTradingLineMgrT : public TradingLineMgrBaseT
                         , public OmsTradingLineMgrBase {
   fon9_NON_COPY_NON_MOVE(OmsTradingLineMgrT);
   using base = TradingLineMgrBaseT;
   using baseOmsTradingLineMgr = OmsTradingLineMgrBase;

protected:
   inline friend void intrusive_ptr_add_ref(const OmsTradingLineMgrT* px) { intrusive_ptr_add_ref(static_cast<const base*>(px)); }
   inline friend void intrusive_ptr_release(const OmsTradingLineMgrT* px) { intrusive_ptr_release(static_cast<const base*>(px)); }
   using ThisSP = fon9::intrusive_ptr<OmsTradingLineMgrT>;
   using Locker = typename base::Locker;
   Locker Lock() override { return Locker{this->TradingSvr_}; };
   void AddRef() const override { intrusive_ptr_add_ref(this); }
   void Release() const override { intrusive_ptr_release(this); }

   void OnBeforeDestroy() {
      base::OnBeforeDestroy();
      this->OmsCore_.reset();
   }
   f9fmkt::SendRequestResult NoReadyLineReject(f9fmkt::TradingRequest& req, fon9::StrView cause) override {
      return this->baseOmsTradingLineMgr::NoReadyLineReject(req, cause);
   }
   void ClearReqQueue(Locker&& tsvr, fon9::StrView cause) override {
      assert(this->use_count() != 0); // 必定要在解構前呼叫 ClearReqQueue();
      this->baseOmsTradingLineMgr::ClearReqQueue(std::move(tsvr), cause, this->OmsCore_);
   }

public:
   template <class... ArgsT>
   OmsTradingLineMgrT(const fon9::IoManagerArgs& ioargs, ArgsT&&... args)
      : base(ioargs, std::forward<ArgsT>(args)...)
      , baseOmsTradingLineMgr(&ioargs.Name_) {
   }

   void RunRequest(OmsRequestRunnerInCore&& runner) {
      Locker tsvr{this->TradingSvr_};
      assert(this->CurrRunner_ == nullptr);
      assert(this->OmsCore_.get() == &runner.Resource_.Core_);
      this->CurrRunner_ = &runner;
      if (fon9_UNLIKELY(this->SendRequest(*const_cast<OmsRequestBase*>(&runner.OrderRaw_.Request()), tsvr)
                        == f9fmkt::SendRequestResult::Queuing)) {
         runner.Update(f9fmkt_TradingRequestSt_Queuing, ToStrView(this->StrQueuingIn_));
      }
      this->CurrRunner_ = nullptr;
   }

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
      (void)src;
      if (tsvr->ReqQueue_.empty())
         return;
      OmsCoreSP core = this->OmsCore_;
      if (!core)
         return;
      tsvr.unlock();

      ThisSP pthis{this};
      core->RunCoreTask([pthis](OmsResource& resource) {
         Locker tsvrInCore{pthis->TradingSvr_};
         if (pthis->OmsCore_.get() == &resource.Core_) {
            pthis->CurrOmsResource_ = &resource;
            pthis->SendReqQueue(tsvrInCore);
            pthis->CurrOmsResource_ = nullptr;
         }
         else {
            // pthis->OmsCore_ 已經改變, 則必定已呼叫過 this->ClearReqQueue(resource.Core_);
            // 如果此時 ReqQueue 還有下單要求, 則必定不屬於 resource.Core_ 的!
         }
      });
   }
};

} // namespaces
#endif//__f9omstw_OmsTradingLineMgr_hpp__
