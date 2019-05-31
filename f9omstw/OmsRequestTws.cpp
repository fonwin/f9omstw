﻿// \file f9omstw/OmsRequestTws.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestTws.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsRequestTwsIni::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsRequestTwsIni>(flds);
   flds.Add(fon9_MakeField2(OmsRequestTwsIni, Side));
   flds.Add(fon9_MakeField2(OmsRequestTwsIni, Symbol));
   flds.Add(fon9_MakeField2(OmsRequestTwsIni, Qty));
   flds.Add(fon9_MakeField2(OmsRequestTwsIni, Pri));
   flds.Add(fon9_MakeField2(OmsRequestTwsIni, PriType));
   flds.Add(fon9_MakeField2(OmsRequestTwsIni, IvacNoFlag));
   flds.Add(fon9_MakeField2(OmsRequestTwsIni, OType));
}

void OmsRequestTwsChg::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsRequestTwsChg>(flds);
   flds.Add(fon9_MakeField2(OmsRequestTwsChg, Qty));
}
bool OmsRequestTwsChg::PreCheckInUser(OmsRequestRunner& reqRunner) {
   if (this->RequestKind_ == f9fmkt_RequestKind_Unknown)
      this->RequestKind_ = (this->Qty_ == 0 ? f9fmkt_RequestKind_Cancel : f9fmkt_RequestKind_ChgQty);
   return base::PreCheckInUser(reqRunner);
}

} // namespaces
