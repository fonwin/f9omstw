// \file f9omstw/OmsRequestRunner.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestRunner.hpp"
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {

void OmsRequestRunner::RequestAbandon(OmsResource* res, OmsErrCode errCode) {
   this->Request_->Abandon(errCode);
   if (res)
      res->Backend_.Append(*this->Request_, std::move(this->ExLog_));
}
void OmsRequestRunner::RequestAbandon(OmsResource* res, OmsErrCode errCode, std::string reason) {
   this->Request_->Abandon(errCode, std::move(reason));
   if (res)
      res->Backend_.Append(*this->Request_, std::move(this->ExLog_));
}
bool OmsRequestRunner::CheckReportRights(const OmsRequestPolicy& pol) {
   assert(IsEnumContains(this->Request_->RequestFlags(), OmsRequestFlag_ReportIn));
   if (IsEnumContains(pol.GetIvrAdminRights(), OmsIvRight::AllowAddReport))
      return true;
   this->RequestAbandon(nullptr, OmsErrCode_DenyAddReport);
   return false;
}
//--------------------------------------------------------------------------//
bool OmsRequestRunnerInCore::AllocOrdNo(OmsOrdNo reqOrdNo) {
   if (OmsBrk* brk = this->OrderRaw_.Order().GetBrk(this->Resource_)) {
      if (OmsOrdNoMap* ordNoMap = brk->GetOrdNoMap(this->OrderRaw_.Request()))
         return ordNoMap->AllocOrdNo(*this, reqOrdNo);
   }
   return OmsOrdNoMap::Reject(*this, OmsErrCode_OrdNoMapNotFound);
}
bool OmsRequestRunnerInCore::AllocOrdNo(OmsOrdTeamGroupId tgId) {
   if (OmsBrk* brk = this->OrderRaw_.Order().GetBrk(this->Resource_)) {
      if (OmsOrdNoMap* ordNoMap = brk->GetOrdNoMap(this->OrderRaw_.Request()))
         return ordNoMap->AllocOrdNo(*this, tgId);
   }
   return OmsOrdNoMap::Reject(*this, OmsErrCode_OrdNoMapNotFound);
}
void OmsRequestRunnerInCore::Update(f9fmkt_TradingRequestSt reqst) {
   this->OrderRaw_.RequestSt_ = reqst;
   if (this->OrderRaw_.Request().RxKind() == f9fmkt_RxKind_RequestNew) {
      if (this->OrderRaw_.UpdateOrderSt_ < static_cast<f9fmkt_OrderSt>(reqst)
          && reqst < f9fmkt_TradingRequestSt_Restated) {
         this->OrderRaw_.UpdateOrderSt_ = static_cast<f9fmkt_OrderSt>(reqst);
         if (f9fmkt_TradingRequestSt_IsFinishedRejected(reqst))
            this->OrderRaw_.OnOrderReject();
      }
   }
}
//--------------------------------------------------------------------------//
OmsRequestRunStep::~OmsRequestRunStep() {
}
void OmsRequestRunStep::RerunRequest(OmsReportRunnerInCore&& runner) {
   if (this->NextStep_)
      this->NextStep_->RerunRequest(std::move(runner));
}

} // namespaces
