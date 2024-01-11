// \file f9omstw/OmsRequestRunner.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestRunner.hpp"
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {

bool OmsRequestRunner::CheckReportRights(const OmsRequestPolicy& pol) {
   assert(IsEnumContains(this->Request_->RequestFlags(), OmsRequestFlag_ReportIn));
   if (IsEnumContains(pol.GetIvrAdminRights(), OmsIvRight::AllowAddReport))
      return true;
   this->RequestAbandon(nullptr, OmsErrCode_DenyAddReport);
   return false;
}
//--------------------------------------------------------------------------//
bool OmsRequestRunnerInCore::AllocOrdNo(OmsOrdNo reqOrdNo, const OmsRequestTrade& iniReq) {
   if (OmsBrk* brk = this->OrderRaw_->Order().GetBrk(this->Resource_)) {
      if (OmsOrdNoMap* ordNoMap = brk->GetOrdNoMap(iniReq))
         return ordNoMap->AllocOrdNo(*this, reqOrdNo, iniReq);
   }
   return OmsOrdNoMap::Reject(*this, OmsErrCode_OrdNoMapNotFound);
}
bool OmsRequestRunnerInCore::AllocOrdNo(OmsOrdTeamGroupId tgId, const OmsRequestTrade& iniReq) {
   if (OmsBrk* brk = this->OrderRaw_->Order().GetBrk(this->Resource_)) {
      if (OmsOrdNoMap* ordNoMap = brk->GetOrdNoMap(iniReq))
         return ordNoMap->AllocOrdNo(*this, tgId);
   }
   return OmsOrdNoMap::Reject(*this, OmsErrCode_OrdNoMapNotFound);
}
void OmsRequestRunnerInCore::Update(f9fmkt_TradingRequestSt reqst) {
   this->OrderRaw_->RequestSt_ = reqst;
   // 新單異動, 需同時更新 OrderSt
   if (fon9_LIKELY(this->OrderRaw_->Request().RxKind() == f9fmkt_RxKind_RequestNew)) {
   __UPDATE_ORDER_ST:;
      if (this->OrderRaw_->UpdateOrderSt_ < static_cast<f9fmkt_OrderSt>(reqst)
          && reqst < f9fmkt_TradingRequestSt_Restated) {
         this->OrderRaw_->UpdateOrderSt_ = static_cast<f9fmkt_OrderSt>(reqst);
         if (f9fmkt_TradingRequestSt_IsFinishedRejected(reqst))
            this->OrderRaw_->OnOrderReject();
      }
   }
   else if (fon9_UNLIKELY(this->OrderRaw_->Request().RxKind() == f9fmkt_RxKind_RequestRerun)) {
      if (auto* upd = dynamic_cast<const OmsRequestUpd*>(&this->OrderRaw_->Request())) {
         if (this->Resource_.Backend_.GetItem(upd->IniSNO())->RxKind() == f9fmkt_RxKind_RequestNew)
            goto __UPDATE_ORDER_ST;
      }
   }
}
//--------------------------------------------------------------------------//
OmsRequestRunStep::~OmsRequestRunStep() {
}
void OmsRequestRunStep::OnCurrentCoreChangedImpl(OmsCore&) {
}
void OmsRequestRunStep::RerunRequest(OmsReportRunnerInCore&& runner) {
   if (this->NextStep_)
      this->NextStep_->RerunRequest(std::move(runner));
}

} // namespaces
