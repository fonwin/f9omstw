// \file f9omstw/OmsCxRequest.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCxRequest.hpp"
#include "f9omstw/OmsCx.h"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsCxLastPriceIniDat::MakeFields(fon9::seed::Fields& flds, int ofsadj) {
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxLastPriceIniDat, CondOp));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxLastPriceIniDat, CondPri));
}
void OmsCxLastPriceChangeable::MakeFields(fon9::seed::Fields& flds, int ofsadj) {
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxLastPriceChangeable, CondPri));
}
//--------------------------------------------------------------------------//
#define DEFINE_LastPrice_Cond_Fn(OP)                                                           \
[](const OmsCxLastPriceOrdRaw& ordraw, const OmsMdLastPrice& bf, const OmsMdLastPriceEv& af) { \
   (void)bf;                                                                                   \
   return af.LastPrice_  OP  ordraw.CondPri_;                                                  \
}

typedef bool (*LastPriceCondFnT)(const OmsCxLastPriceOrdRaw& ordraw, const OmsMdLastPrice& bf, const OmsMdLastPriceEv& af);
static LastPriceCondFnT LastPriceCondFn[] = {
   DEFINE_LastPrice_Cond_Fn(== ),
   DEFINE_LastPrice_Cond_Fn(!= ),
   DEFINE_LastPrice_Cond_Fn(<= ),
   DEFINE_LastPrice_Cond_Fn(>= ),
   DEFINE_LastPrice_Cond_Fn(<  ),
   DEFINE_LastPrice_Cond_Fn(>  ),
};

bool OmsCxLastPriceIniFn::ParseCondOp() {
   if (fon9_LIKELY(this->CondOp_.empty1st())) {
      // 沒設定 CondOp_; 是否要判斷其他 cond 欄位?
      return true;
   }
   f9oms_CondOp_Compare opcomp = f9oms_CondOp_Compare_Parse(this->CondOp_.Chars_);
   if (opcomp == f9oms_CondOp_Compare_Unknown)
      return false;
   this->CondFn_ = LastPriceCondFn[opcomp];
   return true;
}

} // namespaces
