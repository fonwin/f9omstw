// \file f9omstwf/OmsTwfOrder1.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfOrder1.hpp"
#include "f9omstw/OmsErrCodeAct.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwfOrderRaw1::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields(flds);
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw1, PosEff));

   flds.Add(fon9_MakeField2(OmsTwfOrderRaw1, BeforeQty));
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw1, AfterQty));
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw1, LeavesQty));

   flds.Add(fon9_MakeField2(OmsTwfOrderRaw1, LastFilledTime));
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw1, CumQty));
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw1, CumAmt));
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw1, Leg1CumQty));
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw1, Leg1CumAmt));
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw1, Leg2CumQty));
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw1, Leg2CumAmt));

   flds.Add(fon9_MakeField2(OmsTwfOrderRaw1, LastPriTime));
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw1, LastPriType));
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw1, LastPri));
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw1, LastTimeInForce));
}

void OmsTwfOrderRaw1::ContinuePrevUpdate(const OmsOrderRaw& prev) {
   base::ContinuePrevUpdate(prev);
   this->OmsTwfOrderRawDat1::ContinuePrevUpdate(*static_cast<const OmsTwfOrderRaw1*>(&prev));
}
void OmsTwfOrderRaw1::OnOrderReject() {
   assert(f9fmkt_OrderSt_IsFinishedRejected(this->UpdateOrderSt_));
   this->OmsTwfOrderRawDat1::OnOrderReject();
}
bool OmsTwfOrderRaw1::IsWorking() const {
   return this->LeavesQty_ > 0;
}
OmsFilledFlag  OmsTwfOrderRaw1::HasFilled() const {
   return this->CumQty_ > 0 ? OmsFilledFlag::HasFilled : OmsFilledFlag::None;
}
bool OmsTwfOrderRaw1::OnBeforeRerun(const OmsReportRunnerInCore& runner) {
   (void)runner;
   if (this->Request().RxKind() == f9fmkt_RxKind_RequestNew)
      this->LeavesQty_ = this->AfterQty_ = this->BeforeQty_;
   return this->LeavesQty_ > 0;
}

} // namespaces
