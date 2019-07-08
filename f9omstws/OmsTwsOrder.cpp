// \file f9omstws/OmsTwsOrder.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsOrder.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwsOrderRaw::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwsOrderRaw>(flds);
   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, OType));

   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, LastExgTime));
   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, BeforeQty));
   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, AfterQty));
   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, LeavesQty));

   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, LastFilledTime));
   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, CumQty));
   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, CumAmt));
}

void OmsTwsOrderRaw::ContinuePrevUpdate() {
   base::ContinuePrevUpdate();
   assert(dynamic_cast<const OmsTwsOrderRaw*>(this->Prev_) != nullptr);
   OmsTwsOrderRawDat::ContinuePrevUpdate(*static_cast<const OmsTwsOrderRaw*>(this->Prev_));
}

} // namespaces
