// \file f9omstw/OmsRequestTws.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestTws.hpp"
#include "f9omstw/OmsOrder.hpp"
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
const char* OmsRequestTwsIni::IsIniFieldEqual(const OmsRequestBase& req) const {
   if (auto r = dynamic_cast<const OmsRequestTwsIni*>(&req)) {
      #define CHECK_IniFieldEqual(fldName)   do { if(this->fldName##_ != r->fldName##_) return #fldName; } while(0)
      CHECK_IniFieldEqual(Side);
      CHECK_IniFieldEqual(Symbol);
      if (r->OType_ != TwsOType{})
         CHECK_IniFieldEqual(OType);
      return base::IsIniFieldEqualImpl(*r);
   }
   return "RequestTwsIni";
}
const OmsRequestIni* OmsRequestTwsIni::PreCheck_OrdKey(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) {
   if (this->Market_ == f9fmkt_TradingMarket_Unknown || this->SessionId_ == f9fmkt_TradingSessionId_Unknown) {
      if (!scRes.Symb_)
         scRes.Symb_ = res.Symbs_->GetSymb(ToStrView(this->Symbol_));
      if (this->SessionId_ == f9fmkt_TradingSessionId_Unknown) {
         if (this->Pri_.IsNull() && (this->PriType_ == f9fmkt_PriType_Limit || this->PriType_ == f9fmkt_PriType_Unknown))
            this->SessionId_ = f9fmkt_TradingSessionId_FixedPrice;
         else {
            auto shUnit = scRes.GetTwsSymbShUnit();
            if (this->Qty_ < shUnit)
               this->SessionId_ = f9fmkt_TradingSessionId_OddLot;
            else
               this->SessionId_ = f9fmkt_TradingSessionId_Normal;
         }
      }
   }
   return base::PreCheck_OrdKey(runner, res, scRes);
}
//--------------------------------------------------------------------------//
void OmsRequestTwsChg::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsRequestTwsChg>(flds);
   flds.Add(fon9_MakeField2(OmsRequestTwsChg, Qty));
}
bool OmsRequestTwsChg::ValidateInUser(OmsRequestRunner& reqRunner) {
   if (this->RxKind_ == f9fmkt_RxKind_Unknown)
      this->RxKind_ = (this->Qty_ == 0 ? f9fmkt_RxKind_RequestDelete : f9fmkt_RxKind_RequestChgQty);
   return base::ValidateInUser(reqRunner);
}
//--------------------------------------------------------------------------//
void OmsRequestTwsFilled::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsRequestTwsFilled>(flds);
   flds.Add(fon9_MakeField2(OmsRequestTwsFilled, Time));
   flds.Add(fon9_MakeField2(OmsRequestTwsFilled, Pri));
   flds.Add(fon9_MakeField2(OmsRequestTwsFilled, Qty));
}

} // namespaces
