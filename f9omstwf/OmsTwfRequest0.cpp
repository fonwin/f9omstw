// \file f9omstwf/OmsTwfRequest0.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfRequest0.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwfRequestIni0::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwfRequestIni0>(flds);
   flds.Add(fon9_MakeField2(OmsTwfRequestIni0, Symbol));
   flds.Add(fon9_MakeField(OmsTwfRequestIni0,  TmpSource_.OrderSource_, "OrderSource"));
   flds.Add(fon9_MakeField(OmsTwfRequestIni0,  TmpSource_.InfoSource_,  "InfoSource"));
}
const char* OmsTwfRequestIni0::IsIniFieldEqual(const OmsRequestBase& req) const {
   if (auto r = dynamic_cast<const OmsTwfRequestIni0*>(&req)) {
      #define CHECK_IniFieldEqual(fldName)   do { if(this->fldName##_ != r->fldName##_) return #fldName; } while(0)
      CHECK_IniFieldEqual(Symbol);
      return base::IsIniFieldEqualImpl(*r);
   }
   return "RequestTwfIni0";
}
f9twf::ExgCombSide OmsTwfRequestIni0::CombSide(OmsResource& res) const {
   if (this->LastUpdated())
      this->LastUpdated()->Order().GetSymb(res, this->Symbol_);
   return this->CombSide_;
}
f9twf::TmpCombOp OmsTwfRequestIni0::CombOp(OmsResource& res) const {
   if (this->LastUpdated())
      this->LastUpdated()->Order().GetSymb(res, this->Symbol_);
   return this->CombOp_;
}
OmsSymbSP OmsTwfRequestIni0::RegetSymb(OmsResource& res) {
   fon9::StrView symbid = ToStrView(this->Symbol_);
   auto retval = res.Symbs_->GetOmsSymb(symbid);
   if (fon9_LIKELY(retval)) { // 單式 or 期貨價差.
      if (fon9_LIKELY(!f9twf::IsFutShortComb(this->Symbol_.begin())))
         return retval;
      // 期貨價差 => 必須拆解成複式.
   }
   f9twf::ExgCombSymbId combId;
   if (fon9_UNLIKELY(!combId.Parse(symbid) || combId.CombOp_ == f9twf::TmpCombOp::Single))
      return retval;
   this->CombOp_ = combId.CombOp_;
   this->CombSide_ = combId.CombSide_;
   retval = res.Symbs_->GetOmsSymb(ToStrView(combId.LegId1_));
   if (retval) {
      if ((this->SymbLeg2_ = res.Symbs_->GetOmsSymb(ToStrView(combId.LegId2_))).get() == nullptr)
         return nullptr;
   }
   return retval;
}
const OmsRequestIni* OmsTwfRequestIni0::BeforeReq_CheckOrdKey(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) {
   if (!scRes.Symb_) {
      scRes.Symb_ = this->RegetSymb(res);
      if (fon9_UNLIKELY(!scRes.Symb_)) {
         runner.RequestAbandon(&res, OmsErrCode_SymbNotFound);
         return nullptr;
      }
   }
   if (this->SessionId_ == f9fmkt_TradingSessionId_Unknown) {
      // if (!scRes.Symb_) {
      //    runner.RequestAbandon(&res, OmsErrCode_Bad_SessionId_SymbNotFound);
      //    return nullptr;
      // }
      if ((this->SessionId_ = scRes.Symb_->TradingSessionId_) == f9fmkt_TradingSessionId_Unknown) {
         runner.RequestAbandon(&res, OmsErrCode_Bad_SymbSessionId);
         return nullptr;
      }
   }
   return base::BeforeReq_CheckOrdKey(runner, res, scRes);
}

} // namespaces
