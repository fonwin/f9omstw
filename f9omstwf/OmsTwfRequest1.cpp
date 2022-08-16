// \file f9omstwf/OmsTwfRequest1.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfRequest1.hpp"
#include "f9omstwf/OmsTwfOrder1.hpp"
#include "f9omstw/OmsTradingLineMgr.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwfRequestIni1::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields(flds);
   flds.Add(fon9_MakeField2(OmsTwfRequestIni1, Side));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni1, Qty));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni1, Pri));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni1, PriType));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni1, PosEff));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni1, TimeInForce));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni1, IvacNoFlag));
}
const char* OmsTwfRequestIni1::IsIniFieldEqual(const OmsRequestBase& req) const {
   if (auto r = dynamic_cast<const OmsTwfRequestIni1*>(&req)) {
      #define CHECK_IniFieldEqual(fldName)   do { if(this->fldName##_ != r->fldName##_) return #fldName; } while(0)
      CHECK_IniFieldEqual(Side);
      if (r->PosEff_ != OmsTwfPosEff{})
         CHECK_IniFieldEqual(PosEff);
      return base::IsIniFieldEqualImpl(*r);
   }
   return "RequestTwfIni1";
}
//--------------------------------------------------------------------------//
void OmsTwfRequestChg1::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwfRequestChg1>(flds);
   flds.Add(fon9_MakeField2(OmsTwfRequestChg1, Qty));
   flds.Add(fon9_MakeField2(OmsTwfRequestChg1, PriType));
   flds.Add(fon9_MakeField2(OmsTwfRequestChg1, Pri));
   flds.Add(fon9_MakeField2(OmsTwfRequestChg1, TimeInForce));
}
bool OmsTwfRequestChg1::ValidateInUser(OmsRequestRunner& reqRunner) {
   if (fon9_LIKELY(this->RequestUpd_AutoRxKind(*this, reqRunner)))
      return base::ValidateInUser(reqRunner);
   return false;
}
OmsTwfRequestChg1::OpQueuingRequestResult OmsTwfRequestChg1::OpQueuingRequest(fon9::fmkt::TradingLineManager& from,
                                                                              TradingRequest& queuingRequest) {
   if (auto* lmgr = dynamic_cast<OmsTradingLineMgrBase*>(&from))
      return lmgr->OpQueuingRequest<OmsTwfOrderRaw1, OmsTwfRequestIni1>(*this, queuingRequest);
   return Op_NotSupported;
}
OmsOrderRaw* OmsTwfRequestChg1::BeforeReqInCore(OmsRequestRunner& runner, OmsResource& res) {
   return this->BeforeReqInCoreT<OmsTwfOrder1>(runner, res);
}

} // namespaces
