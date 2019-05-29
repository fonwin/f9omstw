// \file f9omstw/OmsRequestTws.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestTws.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

OmsBrk* OmsRequestTwsNew::GetBrk(OmsResource& res) const {
   return res.Brks_->GetBrkRec(ToStrView(this->BrkId_));
}
void OmsRequestTwsNew::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsRequestTwsNew>(flds);
   flds.Add(fon9_MakeField(fon9::Named{"BrkId"},      OmsRequestTwsNew, BrkId_));
   flds.Add(fon9_MakeField(fon9::Named{"Side"},       OmsRequestTwsNew, Side_));
   flds.Add(fon9_MakeField(fon9::Named{"Symbol"},     OmsRequestTwsNew, Symbol_));
   flds.Add(fon9_MakeField(fon9::Named{"Qty"},        OmsRequestTwsNew, Qty_));
   flds.Add(fon9_MakeField(fon9::Named{"Pri"},        OmsRequestTwsNew, Pri_));
   flds.Add(fon9_MakeField(fon9::Named{"PriType"},    OmsRequestTwsNew, PriType_));
   flds.Add(fon9_MakeField(fon9::Named{"IvacNoFlag"}, OmsRequestTwsNew, IvacNoFlag_));
   flds.Add(fon9_MakeField(fon9::Named{"OType"},      OmsRequestTwsNew, OType_));
}

void OmsRequestTwsChg::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsRequestTwsChg>(flds);
   flds.Add(fon9_MakeField(fon9::Named{"BrkId"}, OmsRequestTwsChg, BrkId_));
   flds.Add(fon9_MakeField(fon9::Named{"OrdNo"}, OmsRequestTwsChg, OrdNo_));
   flds.Add(fon9_MakeField(fon9::Named{"Qty"},   OmsRequestTwsChg, Qty_));
}
bool OmsRequestTwsChg::PreCheckInUser(OmsRequestRunner& reqRunner) {
   if (this->RequestKind_ == f9fmkt_RequestKind_Unknown)
      this->RequestKind_ = (this->Qty_ == 0 ? f9fmkt_RequestKind_Cancel : f9fmkt_RequestKind_ChgQty);
   return base::PreCheckInUser(reqRunner);
}

} // namespaces
