// \file f9omstws/OmsTwsTradingLineMgr.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsTradingLineMgr.hpp"

namespace f9omstw {

TwsTradingLineMgr::TwsTradingLineMgr(const fon9::IoManagerArgs& ioargs, f9fmkt_TradingMarket mkt)
   : base{ioargs, mkt}
   , StrQueuingIn_{fon9::RevPrintTo<fon9::CharVector>("Queuing in ", ioargs.Name_)} {
}
void TwsTradingLineMgr::OnBeforeDestroy() {
   base::OnBeforeDestroy();
   this->OmsCore_.reset();
}
//--------------------------------------------------------------------------//
void TwsTradingLineMgr::RunRequest(OmsRequestRunnerInCore&& runner) {
   Locker tsvr{this->TradingSvr_};
   assert(this->CurrRunner_ == nullptr);
   assert(this->OmsCore_.get() == &runner.Resource_.Core_);
   this->CurrRunner_ = &runner;
   if (this->SendRequest(*const_cast<OmsRequestBase*>(runner.OrderRaw_.Request_), tsvr)
       == f9fmkt::SendRequestResult::Queuing) {
      runner.Update(f9fmkt_TradingRequestSt_Queuing, ToStrView(StrQueuingIn_));
   }
   this->CurrRunner_ = nullptr;
}
f9fmkt::SendRequestResult TwsTradingLineMgr::NoReadyLineReject(f9fmkt::TradingRequest& req, fon9::StrView cause) {
   (void)req; // 必定是從 RunRequest() => SendRequest(); 來到這裡, 所以直接使用 this->CurrRunner_ 處理.
   assert(this->CurrRunner_ != nullptr && this->CurrRunner_->OrderRaw_.Request_ == &req);
   this->CurrRunner_->Reject(f9fmkt_TradingRequestSt_LineRejected, OmsErrCode_NoReadyLine, cause);
   return f9fmkt::SendRequestResult::NoReadyLine;
}
//--------------------------------------------------------------------------//
void TwsTradingLineMgr::OnOmsCoreChanged(OmsResource& coreResource) {
   {
      Locker tsvr{this->TradingSvr_};
      if (this->OmsCore_.get() == &coreResource.Core_)
         return;
      OmsCoreSP oldCore = this->OmsCore_;
      if (oldCore && oldCore->TDay() > coreResource.Core_.TDay())
         return;
      if (this->Name_.empty() || this->OrdTeamConfig_.empty())
         this->OrdTeamGroupId_ = 0;
      else {
         auto cfg = coreResource.OrdTeamGroupMgr_.SetTeamGroup(&this->Name_, ToStrView(this->OrdTeamConfig_));
         this->OrdTeamGroupId_ = cfg ? cfg->TeamGroupId_ : 0;
      }
      this->OmsCore_.reset(&coreResource.Core_);
      this->ClearReqQueue(std::move(tsvr), "TDayChanged", oldCore);
   } // tsvr unlock.
   // 交易日改變了, 交易線路應該要刪除後重建, 因為 FIX log 必須換日.
   this->DisposeAndReopen("OmsCore.TDayChanged");
}
void TwsTradingLineMgr::ClearReqQueue(Locker&& tsvr, fon9::StrView cause) {
   this->ClearReqQueue(std::move(tsvr), cause, this->OmsCore_);
}
void TwsTradingLineMgr::ClearReqQueue(Locker&& tsvr, fon9::StrView cause, OmsCoreSP core) {
   using Reqs = TradingSvrImpl::Reqs;
   Reqs  reqs = std::move(tsvr->ReqQueue_);
   if (reqs.empty())
      return;
   assert(this->use_count() != 0); // 必定要在解構前呼叫 ClearReqQueue();

   fon9_WARN_DISABLE_PADDING;
   struct RejInCore : public fon9::intrusive_ref_counter<RejInCore> {
      OmsCoreSP         OmsCore_;
      fon9::CharVector  Cause_;
      Reqs              Reqs_;
      RejInCore(OmsCoreSP omsCore, fon9::StrView cause)
         : OmsCore_{std::move(omsCore)}
         , Cause_{cause} {
      }
   };
   fon9_WARN_POP;
   struct RejInCoreSP : public fon9::intrusive_ptr<RejInCore> {
      RejInCoreSP(RejInCore* p) : fon9::intrusive_ptr<RejInCore>{p} {
      }
      void operator()(OmsResource& resource) {
         RejInCore* pthis = this->get();
         for (f9fmkt::TradingRequestSP& reqL : pthis->Reqs_) {
            const OmsRequestBase*  req = static_cast<OmsRequestBase*>(reqL.get());
            if (OmsOrderRaw* ord = req->LastUpdated()->Order_->BeginUpdate(*req)) {
               OmsRequestRunnerInCore runner{resource, *ord, 0u};
               runner.Reject(f9fmkt_TradingRequestSt_QueuingCanceled, OmsErrCode_NoReadyLine, ToStrView(pthis->Cause_));
            }
         }
      }
   };
   RejInCoreSP rejInCore{new RejInCore(std::move(core), cause)};
   tsvr.unlock();

   rejInCore->Reqs_ = std::move(reqs);
   rejInCore->OmsCore_->RunCoreTask(rejInCore);
}
void TwsTradingLineMgr::OnNewTradingLineReady(f9fmkt::TradingLine* src, Locker&& tsvr) {
   (void)src;
   if (tsvr->ReqQueue_.empty())
      return;
   OmsCoreSP omsCore = this->OmsCore_;
   tsvr.unlock();

   fon9::intrusive_ptr<TwsTradingLineMgr> pthis{this};
   omsCore->RunCoreTask([pthis](OmsResource& resource) {
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

} // namespaces
