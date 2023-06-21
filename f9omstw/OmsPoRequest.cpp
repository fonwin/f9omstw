// \file f9omstw/OmsPoRequest.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsPoRequest.hpp"
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {

OmsPoRequestHandler::~OmsPoRequestHandler() {
   this->ClearResource();
}
void OmsPoRequestHandler::ClearResource() {
   if (this->PolicyConfig_.IvList_.PolicyItem_)
      this->PolicyConfig_.IvList_.PolicyItem_->AfterChangedEvent_.Unsubscribe(&this->PoIvListSubr_);
}
//--------------------------------------------------------------------------//
bool OmsPoRequestHandler_Start(OmsPoRequestHandlerSP handler) {
   if (!handler->Core_)
      return false;
   handler->SubrIvListChanged();
   handler->Core_->RunCoreTask([handler](OmsResource& res) {
      handler->CheckReloadIvList();
      handler->RebuildRequestPolicy(res);
   });
   return true;
}
void OmsPoRequestHandler::RebuildRequestPolicy(OmsResource& res) {
   auto before{this->PolicyConfig_.MakePolicy(res)};
   this->RequestPolicy_.swap(before);
   this->OnRequestPolicyUpdated(before);
}
bool OmsPoRequestHandler::CheckReloadIvList() {
   if (fon9_LIKELY(!this->PolicyConfig_.IvList_.IsPolicyChanged()))
      return false;
   return OmsPoIvListAgent::CheckRegetPolicy(this->PolicyConfig_.IvList_, ToStrView(this->UserId_), ToStrView(this->AuthzId_));
}
void OmsPoRequestHandler::SubrIvListChanged() {
   assert(this->PoIvListSubr_ == nullptr);
   if (!this->PolicyConfig_.IvList_.PolicyItem_)
      return;
   assert(this->Core_);
   OmsPoRequestHandlerSP pthis{this};
   this->PolicyConfig_.IvList_.PolicyItem_->AfterChangedEvent_.Subscribe(
      &this->PoIvListSubr_, [pthis](const fon9::auth::PolicyItem&) {
      pthis->Core_->RunCoreTask([pthis](OmsResource& res) {
         if (pthis->CheckReloadIvList())
            pthis->RebuildRequestPolicy(res);
      });
   });
}
void OmsPoRequestHandler::OnRequestPolicyUpdated(OmsRequestPolicySP before) {
   (void)before;
}
//--------------------------------------------------------------------------//
static void ResetFlowCounter(fon9::FlowCounter& fc, fon9::FlowCounterArgs fcArgs) {
   unsigned count = fcArgs.FcCount_;
   auto     ti = fon9::TimeInterval_Millisecond(fcArgs.FcTimeMS_);
   if (count && ti.GetOrigValue() > 0) {
      // 增加一點緩衝, 避免網路延遲造成退單爭議.
      unsigned cbuf = count / 10;
      count += (cbuf <= 0) ? 1 : cbuf;
      ti -= ti / 10;
   }
   fc.Resize(count, ti);
}

fon9::intrusive_ptr<OmsPoUserRightsAgent> OmsPoRequestConfig::LoadPoUserRights(const fon9::auth::AuthResult& authr) {
   auto agUserRights = authr.AuthMgr_->Agents_->Get<OmsPoUserRightsAgent>(fon9_kCSTR_OmsPoUserRightsAgent_Name);
   if (!agUserRights)
      return nullptr;
   agUserRights->GetPolicy(authr, this->UserRights_, &this->TeamGroupName_);
   this->OrigIvDenys_ = this->UserRights_.IvDenys_;
   if (auto agUserDenys = authr.AuthMgr_->Agents_->Get<OmsPoUserDenysAgent>(fon9_kCSTR_OmsPoUserDenysAgent_Name)) {
      agUserDenys->FetchPolicy(authr, this->PoIvDenys_);
      this->UserRights_.IvDenys_ |= this->PoIvDenys_.IvDenys_;
   }
   ResetFlowCounter(this->FcReq_, this->UserRights_.FcRequest_);
   return agUserRights;
}
fon9::intrusive_ptr<OmsPoIvListAgent> OmsPoRequestConfig::LoadPoIvList(const fon9::auth::AuthResult& authr) {
   auto agIvList = authr.AuthMgr_->Agents_->Get<OmsPoIvListAgent>(fon9_kCSTR_OmsPoIvListAgent_Name);
   if (agIvList)
      agIvList->GetPolicy(authr, this->IvList_);
   return agIvList;
}

} // namespaces
