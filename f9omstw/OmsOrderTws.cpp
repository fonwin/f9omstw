// \file f9omstw/OmsOrderTws.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsOrderTws.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsOrderTwsRaw::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields(flds);
   flds.Add(fon9_MakeField(fon9::Named{"OrdNo"},       OmsOrderTwsRaw, OrdNo_));
   flds.Add(fon9_MakeField(fon9::Named{"OType"},       OmsOrderTwsRaw, OType_));

   flds.Add(fon9_MakeField(fon9::Named{"LastExgTime"}, OmsOrderTwsRaw, LastExgTime_));
   flds.Add(fon9_MakeField(fon9::Named{"BeforeQty"},   OmsOrderTwsRaw, BeforeQty_));
   flds.Add(fon9_MakeField(fon9::Named{"AfterQty"},    OmsOrderTwsRaw, AfterQty_));
   flds.Add(fon9_MakeField(fon9::Named{"LeavesQty"},   OmsOrderTwsRaw, LeavesQty_));

   flds.Add(fon9_MakeField(fon9::Named{"LastMatTime"}, OmsOrderTwsRaw, LastMatTime_));
   flds.Add(fon9_MakeField(fon9::Named{"CumQty"},      OmsOrderTwsRaw, CumQty_));
   flds.Add(fon9_MakeField(fon9::Named{"CumAmt"},      OmsOrderTwsRaw, CumAmt_));

   flds.Add(fon9_MakeField(fon9::Named{"ErrCode"},     OmsOrderTwsRaw, ErrCode_));
   flds.Add(fon9_MakeField(fon9::Named{"Message"},     OmsOrderTwsRaw, Message_));
}

void OmsOrderTwsRaw::ContinuePrevUpdate() {
   base::ContinuePrevUpdate();
   assert(dynamic_cast<const OmsOrderTwsRaw*>(this->Prev_) != nullptr);
   OmsOrderTwsRawDat::ContinuePrevUpdate(*static_cast<const OmsOrderTwsRaw*>(this->Prev_));
}

} // namespaces
