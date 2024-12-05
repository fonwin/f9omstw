// \file f9omstw/OmsCxUnit.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCxUnit.hpp"
#include "f9omstw/OmsScBase.hpp"

namespace f9omstw {

OmsCxUnit::~OmsCxUnit() {
}
// -------------------------------------------------------------------------------- //
bool OmsCxUnit::OmsCx_IsNeedsFireNow(OmsSymb& condSymb, const OmsCxBaseRaw& cxRaw) {
   (void)condSymb; (void)cxRaw;
   return false;
}
bool OmsCxUnit::IsAllowChgCond() const {
   return false;
}
OmsErrCode OmsCxUnit::OnOmsCx_CheckCondDat(const OmsCxBaseChangeable& dat) const {
   if (IsEnumContains(this->Cond_.CheckFlags_, OmsCxCheckFlag::QtyCannotZero) && dat.CondQty_ == 0)
      return OmsErrCode_CondSc_BadQty;
   if (IsEnumContains(this->Cond_.CheckFlags_, OmsCxCheckFlag::PriNeeds))
      if (!IsEnumContains(this->Cond_.CheckFlags_, OmsCxCheckFlag::PriAllowMarket) && dat.CondPri_.IsNull())
         return OmsErrCode_CondSc_BadPri;
   return OmsErrCode_Null;
}
OmsErrCode OmsCxUnit::OnOmsCx_AssignReqChgToOrd(const OmsCxBaseChgDat& reqChg, OmsCxBaseRaw& cxRaw) const {
   OmsErrCode retval = this->OnOmsCx_CheckCondDat(reqChg);
   if (retval == OmsErrCode_Null)
      cxRaw = reqChg;
   return retval;
}
// -------------------------------------------------------------------------------- //
bool OmsCxUnit::OnOmsCx_MdLastPriceEv(const OmsCxMdEvArgs& args) {
   (void)args;
   return false;
}
bool OmsCxUnit::OnOmsCx_MdLastPriceEv_OddLot(const OmsCxMdEvArgs& args) {
   (void)args;
   return false;
}
bool OmsCxUnit::OnOmsCx_MdBSEv(const OmsCxMdEvArgs& args) {
   (void)args;
   return false;
}
bool OmsCxUnit::OnOmsCx_MdBSEv_OddLot(const OmsCxMdEvArgs& args) {
   (void)args;
   return false;
}
// -------------------------------------------------------------------------------- //
bool OmsCxUnit::RevPrintCxArgs(fon9::RevBuffer& rbuf) const {
   bool isNeedsComma = false;
   if (IsEnumContains(this->Cond_.CheckFlags_, OmsCxCheckFlag::QtyNeeds)) {
      fon9::RevPrint(rbuf, this->Cond_.CondQty_);
      isNeedsComma = true;
   }
   if (IsEnumContains(this->Cond_.CheckFlags_, OmsCxCheckFlag::PriNeeds)) {
      if (isNeedsComma)
         fon9::RevPrint(rbuf, ',');
      if (this->Cond_.CondPri_.IsNull())
         fon9::RevPrint(rbuf, 'M');
      else
         fon9::RevPrint(rbuf, this->Cond_.CondPri_);
      isNeedsComma = true;
   }
   return isNeedsComma;
}
void OmsCxUnit::RevPrint(fon9::RevBuffer& rbuf) const {
   if (this->IsUsrWaitTrueLock())
      fon9::RevPrint(rbuf, '@');
   this->RevPrintCxArgs(rbuf);
   fon9::RevPrint(rbuf, this->Cond_.Name_, this->Cond_.Op_);
   if (this->CondSymb_)
      fon9::RevPrint(rbuf, this->CondSymb_->SymbId_, '.');
}

} // namespaces
