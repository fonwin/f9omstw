// \file f9omstw/OmsRequestTrade.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsOrder.hpp"
#include "f9omstw/OmsRequestRunner.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsRequestTrade::MakeFieldsImpl(fon9::seed::Fields& flds) {
   base::MakeFields<OmsRequestTrade>(flds);
   flds.Add(fon9_MakeField2(OmsRequestTrade, SesName));
   flds.Add(fon9_MakeField2(OmsRequestTrade, UserId));
   flds.Add(fon9_MakeField2(OmsRequestTrade, FromIp));
   flds.Add(fon9_MakeField2(OmsRequestTrade, Src));
   flds.Add(fon9_MakeField2(OmsRequestTrade, UsrDef));
   flds.Add(fon9_MakeField2(OmsRequestTrade, ClOrdId));
}
bool OmsRequestTrade::PreCheckInUser(OmsRequestRunner& reqRunner) {
   return this->Policy_ ? this->Policy_->PreCheckInUser(reqRunner) : true;
}
//--------------------------------------------------------------------------//
bool OmsRequestIni::PreCheckInUser(OmsRequestRunner& reqRunner) {
   if (this->RequestKind_ == f9fmkt_RequestKind_New && *this->OrdNo_.begin() != '\0') {
      const OmsRequestPolicy* pol = this->Policy();
      if (pol == nullptr || !pol->IsAllowAnyOrdNo()) {
         reqRunner.RequestAbandon(nullptr, "OrdNo must empty.");
         return false;
      }
   }
   return base::PreCheckInUser(reqRunner);
}
void OmsRequestIni::MakeFieldsImpl(fon9::seed::Fields& flds) {
   base::MakeFields<OmsRequestIni>(flds);
   flds.Add(fon9_MakeField2(OmsRequestIni, IvacNo));
   flds.Add(fon9_MakeField2(OmsRequestIni, SubacNo));
   flds.Add(fon9_MakeField2(OmsRequestIni, SalesNo));
}
//--------------------------------------------------------------------------//
void OmsRequestUpd::MakeFieldsImpl(fon9::seed::Fields& flds) {
   flds.Add(fon9_MakeField2(OmsRequestUpd, IniSNO));
   base::MakeFields<OmsRequestUpd>(flds);
}

} // namespaces
