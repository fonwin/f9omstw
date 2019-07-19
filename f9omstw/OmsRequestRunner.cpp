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
//--------------------------------------------------------------------------//
bool OmsRequestRunnerInCore::AllocOrdNo(OmsOrdNo reqOrdNo) {
   if (OmsBrk* brk = this->OrderRaw_.Order_->GetBrk(this->Resource_)) {
      if (OmsOrdNoMap* ordNoMap = brk->GetOrdNoMap(*this->OrderRaw_.Request_))
         return ordNoMap->AllocOrdNo(*this, reqOrdNo);
   }
   return OmsOrdNoMap::Reject(*this, OmsErrCode_OrdNoMapNotFound);
}
bool OmsRequestRunnerInCore::AllocOrdNo(OmsOrdTeamGroupId tgId) {
   if (OmsBrk* brk = this->OrderRaw_.Order_->GetBrk(this->Resource_)) {
      if (OmsOrdNoMap* ordNoMap = brk->GetOrdNoMap(*this->OrderRaw_.Request_))
         return ordNoMap->AllocOrdNo(*this, tgId);
   }
   return OmsOrdNoMap::Reject(*this, OmsErrCode_OrdNoMapNotFound);
}
void OmsRequestRunnerInCore::Reject(f9fmkt_TradingRequestSt reqst, OmsErrCode ec, fon9::StrView cause) {
   this->OrderRaw_.ErrCode_ = ec;
   this->Update(reqst, cause);
}
void OmsRequestRunnerInCore::Update(f9fmkt_TradingRequestSt reqst, fon9::StrView cause) {
   this->OrderRaw_.Message_.assign(cause);
   this->OrderRaw_.RequestSt_ = reqst;
   if (this->OrderRaw_.Request_->RxKind() == f9fmkt_RxKind_RequestNew) {
      if (this->OrderRaw_.OrderSt_ < static_cast<f9fmkt_OrderSt>(reqst))
         this->OrderRaw_.OrderSt_ = static_cast<f9fmkt_OrderSt>(reqst);
   }
}
//--------------------------------------------------------------------------//
OmsRequestRunStep::~OmsRequestRunStep() {
}

} // namespaces
