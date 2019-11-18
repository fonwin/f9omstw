// \file f9omstwf/OmsTwfOrder0.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfOrder0.hpp"
#include "f9omstwf/OmsTwfRequest0.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

OmsTwfOrder0::~OmsTwfOrder0() {
}
fon9::fmkt::SymbSP OmsTwfOrder0::FindSymb(OmsResource& res, const fon9::StrView& symbid) {
   auto retval = base::FindSymb(res, symbid);
   if (retval)
      return retval;
   if (const auto* iniReq0 = static_cast<const OmsTwfRequestIni0*>(this->Initiator())) {
      assert(dynamic_cast<const OmsTwfRequestIni0*>(this->Initiator()) != nullptr);
      return const_cast<OmsTwfRequestIni0*>(iniReq0)->RegetSymb(res);
   }
   f9twf::ExgCombSymbId combId;
   if (fon9_UNLIKELY(!combId.Parse(symbid) || combId.CombOp_ == f9twf::TmpCombOp::Single))
      return nullptr;
   return res.Symbs_->GetSymb(ToStrView(combId.LegId1_));
}

OmsTwfOrderRaw0::~OmsTwfOrderRaw0() {
}
void OmsTwfOrderRaw0::ContinuePrevUpdate(const OmsOrderRaw& prev) {
   base::ContinuePrevUpdate(prev);
   this->OmsTwfOrderRawDat0::ContinuePrevUpdate(*static_cast<const OmsTwfOrderRaw0*>(&prev));
}
void OmsTwfOrderRaw0::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwfOrderRaw0>(flds);
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw0, LastExgTime));
}

} // namespaces
