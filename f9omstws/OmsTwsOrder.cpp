﻿// \file f9omstws/OmsTwsOrder.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstw/OmsErrCodeAct.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwsOrderRaw::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwsOrderRaw>(flds);
   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, OType));
   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, OutPvcId));

   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, LastExgTime));
   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, BeforeQty));
   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, AfterQty));
   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, LeavesQty));

   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, LastFilledTime));
   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, CumQty));
   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, CumAmt));

   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, LastPriTime));
   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, LastPriType));
   flds.Add(fon9_MakeField2(OmsTwsOrderRaw, LastPri));
}

void OmsTwsOrderRaw::ContinuePrevUpdate(const OmsOrderRaw& prev) {
   base::ContinuePrevUpdate(prev);
   OmsTwsOrderRawDat::ContinuePrevUpdate(*static_cast<const OmsTwsOrderRaw*>(&prev));
}
void OmsTwsOrderRaw::OnOrderReject() {
   assert(f9fmkt_OrderSt_IsFinishedRejected(this->UpdateOrderSt_));
   this->AfterQty_ = 0;
   this->LeavesQty_ = 0;
}
bool OmsTwsOrderRaw::CheckErrCodeAct(const OmsErrCodeAct& act) const {
   if (act.OTypes_.empty())
      return true;
   return memchr(act.OTypes_.begin(), static_cast<char>(this->OType_), act.OTypes_.size()) != nullptr;
}
bool OmsTwsOrderRaw::IsWorking() const {
   return this->LeavesQty_ > 0;
}
OmsFilledFlag OmsTwsOrderRaw::HasFilled() const {
   return this->CumQty_ > 0 ? OmsFilledFlag::HasFilled : OmsFilledFlag::None;
}
bool OmsTwsOrderRaw::OnBeforeRerun(const OmsReportRunnerInCore& runner) {
   (void)runner;
   if (this->Request().RxKind() == f9fmkt_RxKind_RequestNew)
      this->LeavesQty_ = this->AfterQty_ = this->BeforeQty_;
   return this->LeavesQty_ > 0;
}

} // namespaces
