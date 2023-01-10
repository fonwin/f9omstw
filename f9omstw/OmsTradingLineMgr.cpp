// \file f9omstw/OmsTradingLineMgr.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsTradingLineMgr.hpp"

namespace f9omstw {

static fon9::CharVector Make_StrQueuingIn(fon9::StrView name) {
   if (name.size() > 3) { // 移除尾端的 "_io";
      const char* pend = name.end() - 3;
      if (memcmp(pend, "_io", 3) == 0)
         name.SetEnd(pend);
   }
   fon9::CharVector retval{fon9::StrView{fon9_kCSTR_QueuingIn}};
   retval.append(name);
   return retval;
}

OmsTradingLineMgrBase::OmsTradingLineMgrBase(fon9::StrView name, f9fmkt::TradingLineManager& thisLineMgr)
   : StrQueuingIn_{Make_StrQueuingIn(name)}
   , ThisLineMgr_(thisLineMgr) {
}
OmsTradingLineMgrBase::~OmsTradingLineMgrBase() {
}
f9fmkt::SendRequestResult OmsTradingLineMgrBase::NoReadyLineReject(f9fmkt::TradingRequest& req, fon9::StrView cause) {
   (void)req; // 必定是從 RunRequest() => SendRequest(); 來到這裡, 所以直接使用 this->CurrRunner_ 處理.
   assert(this->CurrRunner_ != nullptr && &this->CurrRunner_->OrderRaw().Request() == &req);
   fon9::CharVector        msgbuf;
   const size_t            maxsz = this->StrQueuingIn_.size() + cause.size();
   // maxsz = 4 + GetCommonName(this).size() // 簡化為 this->StrQueuingIn_.size()
   //       + cause.size(); 
   fon9::RevBufferFixedMem rbuf(msgbuf.alloc(maxsz), maxsz);
   fon9::RevPrint(rbuf, cause, "|Lg=", this->GetName());
   this->CurrRunner_->Reject(f9fmkt_TradingRequestSt_LineRejected, OmsErrCode_NoReadyLine, ToStrView(rbuf));
   return f9fmkt::SendRequestResult::NoReadyLine;
}
bool OmsTradingLineMgrBase::CheckUnsendableBroken() {
   switch (this->State_) {
   default:
   case TradingLineMgrState::Ready:
   {
      // 由於並不是每次的 [無線路 & 無Helper] 都會透過 ClearReqQueue() 處理, 有時會透過 SendReqQueue(),
      // 所以需在此判斷 IsSendable(); 來決定是否仍處在 TradingLineMgrState::Ready 狀態.
      auto lk = this->LockLineMgr();
      if (!lk->IsSendable())
         this->SetState(TradingLineMgrState::UnsendableReject, lk);
      break;
   }
   case TradingLineMgrState::UnsendableBroken:
      break;
   case TradingLineMgrState::UnsendableReject:
   case TradingLineMgrState::UnsendableBroken1:
      auto lk = this->LockLineMgr();
      if (lk->IsSendable()) {
         this->SetState(TradingLineMgrState::Ready, lk);
         return false;
      }
      switch (this->State_) {
      default:
      case TradingLineMgrState::UnsendableBroken:
      case TradingLineMgrState::Ready:
         return false;
      case TradingLineMgrState::UnsendableReject:
         // 下次檢查時, 若仍為 UnsendableBroken1, 則確定無法恢復連線, 才會進入 UnsendableBroken 狀態.
         this->State_ = TradingLineMgrState::UnsendableBroken1;
         break;
      case TradingLineMgrState::UnsendableBroken1:
         this->State_ = TradingLineMgrState::UnsendableBroken;
         return true;
      }
   }
   return false;
}
//--------------------------------------------------------------------------//
void OmsTradingLineMgrBase::SetOrdTeamGroupId(OmsResource& coreResource, const Locker&) {
   if (this->OmsCore_.get() != &coreResource.Core_)
      return;
   fon9::StrView name = this->GetName();
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
   if (OmsCoreSP core = this->IsNeedsUpdateOrdTeamConfig(std::move(ordTeamConfig), this->LockLineMgr())) {
      ThisSP pthis{this};
      core->RunCoreTask([pthis](OmsResource& coreResource) {
         pthis->SetOrdTeamGroupId(coreResource, pthis->LockLineMgr());
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
   fon9::fmkt::TradingLineManager::Reqs reqs;
   tsvr->ReqQueueMoveTo(reqs);
   if (reqs.empty())
      return;

   using Reqs = fon9::decay_t<decltype(reqs)>;
   struct RejInCore : public fon9::intrusive_ref_counter<RejInCore> {
      char              Padding____[4];
      OmsCoreSP         OmsCore_;
      fon9::CharVector  Cause_;
      Reqs              Reqs_;
      RejInCore(OmsCoreSP omsCore, fon9::StrView cause, OmsTradingLineMgrBase* lmgr)
         : OmsCore_{std::move(omsCore)}
         , Cause_{cause} {
         this->Cause_.append("|Lg=");
         this->Cause_.append(lmgr->GetName());
      }
   };
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
   RejInCoreSP rejInCore{new RejInCore(std::move(core), cause, this)};
   tsvr.unlock();

   rejInCore->Reqs_ = std::move(reqs);
   rejInCore->OmsCore_->RunCoreTask(rejInCore);
}
void OmsTradingLineMgrBase::OnHelperBroken(OmsResource& resource) {
   auto tsvr = this->LockLineMgr();
   if (tsvr->IsLinesEmpty()) {
      this->SetState(TradingLineMgrState::UnsendableReject, tsvr);
      this->ClearReqQueue(std::move(tsvr), "Helper broken.", &resource.Core_);
   }
}
void OmsTradingLineMgrBase::OnOfferSendable(OmsResource& resource, f9fmkt::TradingLineManager::FnHelpOffer&& fnOffer) {
   Locker tsvr = this->LockLineMgr();
   if (tsvr->IsReqQueueEmpty())
      return;
   if (this->OmsCore_.get() == &resource.Core_) {
      this->CurrOmsResource_ = &resource;
      tsvr->SendReqQueueByOffer(&tsvr, std::move(fnOffer));
      this->CurrOmsResource_ = nullptr;
   }
}
void OmsTradingLineMgrBase::ToCore_SendReqQueue(Locker&& tsvr) {
   if (tsvr->IsReqQueueEmpty())
      return;
   if (OmsCoreSP core = this->OmsCore_) {
      tsvr.unlock(); // 如果 RunCoreTask 立即執行, 則在 InCore_SendReqQueue() 裡面的 LockLineMgr() 會死結, 所以這裡必須先解鎖.
      core->RunCoreTask(std::bind(&OmsTradingLineMgrBase::InCore_SendReqQueue, ThisSP{this}, std::placeholders::_1));
   }
}
void OmsTradingLineMgrBase::InCore_SendReqQueue(OmsResource& resource) {
   Locker tsvr = this->LockLineMgr();
   if (this->OmsCore_.get() == &resource.Core_) {
      this->CurrOmsResource_ = &resource;
      tsvr->SendReqQueue(&tsvr);
      this->CurrOmsResource_ = nullptr;
   }
   else {
      // pthis->OmsCore_ 已經改變, 則必定已呼叫過 this->ClearReqQueue(resource.Core_);
      // 如果此時 ReqQueue 還有下單要求, 則必定不屬於 resource.Core_ 的!
   }
}
void OmsTradingLineMgrBase::RunRequest(OmsRequestRunnerInCore&& runner, const Locker& tsvr) {
   assert(this->CurrRunner_ == nullptr);
   assert(this->OmsCore_.get() == &runner.Resource_.Core_);
   this->CurrRunner_ = &runner;
   if (fon9_UNLIKELY(this->ThisLineMgr_.SendRequest(*const_cast<OmsRequestBase*>(&runner.OrderRaw().Request()), tsvr)
                     == f9fmkt::SendRequestResult::Queuing)) {
      runner.Update(f9fmkt_TradingRequestSt_Queuing, ToStrView(this->StrQueuingIn_));
   }
   this->CurrRunner_ = nullptr;
}
f9fmkt::SendRequestResult OmsTradingLineMgrBase::OnAskFor_SendRequest_InCore(f9fmkt::TradingRequest& req, OmsResource& resource) {
   assert(this->CurrRunner_ == nullptr && this->CurrOmsResource_ == nullptr);
   assert(static_cast<OmsRequestBase*>(&req)->LastUpdated() != nullptr);
   auto tsvr = this->LockLineMgr();
   if (this->OmsCore_.get() != &resource.Core_)
      return f9fmkt::SendRequestResult::NoReadyLine;
   this->CurrOmsResource_ = &resource;
   f9fmkt::SendRequestResult retval = this->ThisLineMgr_.SendRequest_ByLines_NoQueue(req, tsvr);
   this->CurrOmsResource_ = nullptr;
   return retval;
}
f9fmkt::SendRequestResult OmsTradingLineMgrBase::OnAskFor_SendRequest_FromLocal(f9fmkt::TradingRequest& req, const Locker& tsvrSelf, OmsTradingLineMgrBase& askerFrom) {
   assert(askerFrom.CurrRunner_ != nullptr || askerFrom.CurrOmsResource_ != nullptr);
   this->CurrRunner_ = askerFrom.CurrRunner_;           // 來源: RunStep;
   this->CurrOmsResource_ = askerFrom.CurrOmsResource_; // 來源: SendReqQueue;
   const auto retval = this->ThisLineMgr_.SendRequest_ByLines_NoQueue(req, tsvrSelf);
   this->CurrRunner_ = nullptr;
   this->CurrOmsResource_ = nullptr;
   return retval;
}
f9fmkt::SendRequestResult OmsTradingLineMgrBase::OnAskFor_SendRequest_ByRunner(OmsInternalRunnerInCore& runner) {
   assert(this->CurrRunner_ == nullptr && this->CurrOmsResource_ == nullptr);
   auto tsvr = this->LockLineMgr();
   if (this->OmsCore_.get() != &runner.Resource_.Core_)
      return f9fmkt::SendRequestResult::NoReadyLine;
   this->CurrRunner_ = &runner;
   f9fmkt::SendRequestResult retval = this->ThisLineMgr_.SendRequest_ByLines_NoQueue(
      *const_cast<OmsRequestBase*>(&runner.OrderRaw().Request()),
      tsvr);
   this->CurrRunner_ = nullptr;
   return retval;
}

} // namespaces
