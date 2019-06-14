// \file f9omstw/OmsOrderTws.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsOrderTws.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsOrderTwsRaw::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsOrderTwsRaw>(flds);
   flds.Add(fon9_MakeField2(OmsOrderTwsRaw, OType));

   flds.Add(fon9_MakeField2(OmsOrderTwsRaw, LastExgTime));
   flds.Add(fon9_MakeField2(OmsOrderTwsRaw, BeforeQty));
   flds.Add(fon9_MakeField2(OmsOrderTwsRaw, AfterQty));
   flds.Add(fon9_MakeField2(OmsOrderTwsRaw, LeavesQty));

   flds.Add(fon9_MakeField2(OmsOrderTwsRaw, LastFilledTime));
   flds.Add(fon9_MakeField2(OmsOrderTwsRaw, CumQty));
   flds.Add(fon9_MakeField2(OmsOrderTwsRaw, CumAmt));
}

void OmsOrderTwsRaw::ContinuePrevUpdate() {
   base::ContinuePrevUpdate();
   assert(dynamic_cast<const OmsOrderTwsRaw*>(this->Prev_) != nullptr);
   OmsOrderTwsRawDat::ContinuePrevUpdate(*static_cast<const OmsOrderTwsRaw*>(this->Prev_));
}

} // namespaces
