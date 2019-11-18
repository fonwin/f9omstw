// \file f9omstw/OmsTradingLineMgr.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsTradingLineMgr.hpp"

namespace f9omstw {

#define kCSTR_QueuingIn    "Queuing in "
#define GetCommonName(p)    fon9::StrView{p->StrQueuingIn_.begin() + sizeof(kCSTR_QueuingIn) - 1, \
                                          p->StrQueuingIn_.end()}

OmsTradingLineMgrBase::OmsTradingLineMgrBase(fon9::StrView name)
   : StrQueuingIn_{fon9::RevPrintTo<fon9::CharVector>(kCSTR_QueuingIn, name)} {
}
OmsTradingLineMgrBase::~OmsTradingLineMgrBase() {
}
f9fmkt::SendRequestResult OmsTradingLineMgrBase::NoReadyLineReject(f9fmkt::TradingRequest& req, fon9::StrView cause) {
   (void)req; // 必定是從 RunRequest() => SendRequest(); 來到這裡, 所以直接使用 this->CurrRunner_ 處理.
   assert(this->CurrRunner_ != nullptr && &this->CurrRunner_->OrderRaw_.Request() == &req);
   this->CurrRunner_->Reject(f9fmkt_TradingRequestSt_LineRejected, OmsErrCode_NoReadyLine, cause);
   return f9fmkt::SendRequestResult::NoReadyLine;
}
//--------------------------------------------------------------------------//
void OmsTradingLineMgrBase::SetOrdTeamGroupId(OmsResource& coreResource, const Locker&) {
   if (this->OmsCore_.get() != &coreResource.Core_)
      return;
   fon9::StrView name = GetCommonName(this);
   if (name.empty() || this->OrdTeamConfig_.empty())
      this->OrdTeamGroupId_ = 0;
   else {
      auto cfg = coreResource.OrdTeamGroupMgr_.SetTeamGroup(name, ToStrView(this->OrdTeamConfig_));
      this->OrdTeamGroupId_ = cfg ? cfg->TeamGroupId_ : 0;
   }
}
OmsCoreSP OmsTradingLineMgrBase::IsNeedsUpdateOrdTeamConfig(fon9::CharVector ordTeamConfig, const Locker&) {
   if (this->OrdTeamConfig_ == ordTeamConfig)
      return nullptr;
   this->OrdTeamConfig_ = std::move(ordTeamConfig);
   return this->OmsCore_;
}
void OmsTradingLineMgrBase::OnOrdTeamConfigChanged(fon9::CharVector ordTeamConfig) {
   if (OmsCoreSP core = this->IsNeedsUpdateOrdTeamConfig(std::move(ordTeamConfig), this->Lock())) {
      ThisSP pthis{this};
      core->RunCoreTask([pthis](OmsResource& coreResource) {
         pthis->SetOrdTeamGroupId(coreResource, pthis->Lock());
      });
   }
}
bool OmsTradingLineMgrBase::SetOmsCore(OmsResource& coreResource, Locker&& tsvr) {
   if (this->OmsCore_.get() == &coreResource.Core_)
      return false;
   OmsCoreSP oldCore = this->OmsCore_;
   if (oldCore && oldCore->TDay() > coreResource.Core_.TDay())
      return false;
   this->OmsCore_.reset(&coreResource.Core_);
   this->SetOrdTeamGroupId(coreResource, tsvr);
   this->ClearReqQueue(std::move(tsvr), "TDayChanged", oldCore);
   return true;
}
void OmsTradingLineMgrBase::ClearReqQueue(Locker&& tsvr, fon9::StrView cause, OmsCoreSP core) {
   auto reqs = std::move(tsvr->ReqQueue_);
   if (reqs.empty())
      return;

   fon9_WARN_DISABLE_PADDING;
   using Reqs = fon9::decay_t<decltype(reqs)>;
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
            if (OmsOrderRaw* ord = req->LastUpdated()->Order().BeginUpdate(*req)) {
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

} // namespaces
