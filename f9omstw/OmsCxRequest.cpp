// \file f9omstw/OmsCxRequest.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCxRequest.hpp"
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
static bool CxLastPrice_CondOp_NE(const OmsCxLastPriceOrdRaw& ordraw, const OmsMdLastPrice& bf, const OmsMdLastPriceEv& af) {
   (void)bf;
   return af.LastPrice_ != ordraw.CondPri_;
}
static bool CxLastPrice_CondOp_EQ(const OmsCxLastPriceOrdRaw& ordraw, const OmsMdLastPrice& bf, const OmsMdLastPriceEv& af) {
   (void)bf;
   return af.LastPrice_ == ordraw.CondPri_;
}
static bool CxLastPrice_CondOp_LEQ(const OmsCxLastPriceOrdRaw& ordraw, const OmsMdLastPrice& bf, const OmsMdLastPriceEv& af) {
   (void)bf;
   return af.LastPrice_ <= ordraw.CondPri_;
}
static bool CxLastPrice_CondOp_GEQ(const OmsCxLastPriceOrdRaw& ordraw, const OmsMdLastPrice& bf, const OmsMdLastPriceEv& af) {
   (void)bf;
   return af.LastPrice_ >= ordraw.CondPri_;
}
static bool CxLastPrice_CondOp_Less(const OmsCxLastPriceOrdRaw& ordraw, const OmsMdLastPrice& bf, const OmsMdLastPriceEv& af) {
   (void)bf;
   return af.LastPrice_ < ordraw.CondPri_;
}
static bool CxLastPrice_CondOp_Gre(const OmsCxLastPriceOrdRaw& ordraw, const OmsMdLastPrice& bf, const OmsMdLastPriceEv& af) {
   (void)bf;
   return af.LastPrice_ > ordraw.CondPri_;
}

bool OmsCxLastPriceIniFn::ParseCondOp() {
   if (fon9_LIKELY(this->CondOp_.empty1st())) {
      // 沒設定 CondOp_; 是否要判斷其他 cond 欄位?
      return true;
   }
   switch (this->CondOp_.Chars_[0]) {
   case '!':
      if (this->CondOp_.Chars_[1] == '=') { // "!="
         this->CondFn_ = &CxLastPrice_CondOp_NE;
         return true;
      }
      break;
   case '=':
      if (this->CondOp_.Chars_[1] == '=') { // "=="
         this->CondFn_ = &CxLastPrice_CondOp_EQ;
         return true;
      }
      break;
   case '<':
      switch (this->CondOp_.Chars_[1]) {
      case '=': // "<="
         this->CondFn_ = &CxLastPrice_CondOp_LEQ;
         return true;
      case '\0': // "<"
         this->CondFn_ = &CxLastPrice_CondOp_Less;
         return true;
      }
      break;
   case '>':
      switch (this->CondOp_.Chars_[1]) {
      case '=': // ">="
         this->CondFn_ = &CxLastPrice_CondOp_GEQ;
         return true;
      case '\0': // ">"
         this->CondFn_ = &CxLastPrice_CondOp_Gre;
         return true;
      }
      break;
   }
   return false;
}

} // namespaces
