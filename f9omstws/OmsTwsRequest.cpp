// \file f9omstws/OmsTwsRequest.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsRequest.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwsRequestIni::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwsRequestIni>(flds);
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, Side));
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, Symbol));
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, Qty));
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, Pri));
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, PriType));
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, IvacNoFlag));
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, OType));
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, TimeInForce));
}
const char* OmsTwsRequestIni::IsIniFieldEqual(const OmsRequestBase& req) const {
   if (auto r = dynamic_cast<const OmsTwsRequestIni*>(&req)) {
      #define CHECK_IniFieldEqual(fldName)   do { if(this->fldName##_ != r->fldName##_) return #fldName; } while(0)
      CHECK_IniFieldEqual(Side);
      CHECK_IniFieldEqual(Symbol);
      if (r->OType_ != OmsTwsOType{})
         CHECK_IniFieldEqual(OType);
      return base::IsIniFieldEqualImpl(*r);
   }
   return "RequestTwsIni";
}
const OmsRequestIni* OmsTwsRequestIni::BeforeReq_CheckOrdKey(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) {
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
   return base::BeforeReq_CheckOrdKey(runner, res, scRes);
}
//--------------------------------------------------------------------------//
void OmsTwsRequestChg::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwsRequestChg>(flds);
   flds.Add(fon9_MakeField2(OmsTwsRequestChg, Qty));
   flds.Add(fon9_MakeField2(OmsTwsRequestChg, PriType));
   flds.Add(fon9_MakeField2(OmsTwsRequestChg, Pri));
}
bool OmsTwsRequestChg::ValidateInUser(OmsRequestRunner& reqRunner) {
   if (fon9_UNLIKELY(this->RxKind_ == f9fmkt_RxKind_Unknown)) {
      if (this->Qty_ == 0) {
         if (this->PriType_ == f9fmkt_PriType{} && this->Pri_.IsNull())
            this->RxKind_ = f9fmkt_RxKind_RequestDelete;
         else
            this->RxKind_ = f9fmkt_RxKind_RequestChgPri;
      }
      else {
         if (this->PriType_ == f9fmkt_PriType{} && this->Pri_.IsNull())
            this->RxKind_ = f9fmkt_RxKind_RequestChgQty;
         else // 不能同時「改價 & 改量」.
            reqRunner.RequestAbandon(nullptr, OmsErrCode_Bad_RxKind, nullptr);
      }
   }
   return base::ValidateInUser(reqRunner);
}

} // namespaces
