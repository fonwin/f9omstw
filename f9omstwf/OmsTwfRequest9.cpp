// \file f9omstwf/OmsTwfRequest9.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfRequest9.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwfRequestIni9::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields(flds);
   flds.Add(fon9_MakeField2(OmsTwfRequestIni9, BidQty));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni9, BidPri));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni9, OfferQty));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni9, OfferPri));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni9, TimeInForce));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni9, IvacNoFlag));
}
//--------------------------------------------------------------------------//
void OmsTwfRequestChg9::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwfRequestChg9>(flds);
   flds.Add(fon9_MakeField2(OmsTwfRequestChg9, BidQty));
   flds.Add(fon9_MakeField2(OmsTwfRequestChg9, BidPri));
   flds.Add(fon9_MakeField2(OmsTwfRequestChg9, OfferQty));
   flds.Add(fon9_MakeField2(OmsTwfRequestChg9, OfferPri));
}
bool OmsTwfRequestChg9::ValidateInUser(OmsRequestRunner& reqRunner) {
   if (fon9_LIKELY(this->RxKind_ == f9fmkt_RxKind_Unknown)) {
      if (this->BidQty_ == 0 && this->OfferQty_ == 0) {
         if (this->BidPri_.IsNullOrZero() && this->OfferPri_.IsNullOrZero())
            this->RxKind_ = f9fmkt_RxKind_RequestDelete;
         else
            this->RxKind_ = f9fmkt_RxKind_RequestChgPri;
      }
      else
         this->RxKind_ = f9fmkt_RxKind_RequestChgQty;
   }
   return base::ValidateInUser(reqRunner);
}

} // namespaces
