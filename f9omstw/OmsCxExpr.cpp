// \file f9omstw/OmsCxExpr.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCxExpr.hpp"
#include "fon9/RevPrint.hpp"

namespace f9omstw {

OmsCxExpr::~OmsCxExpr() {
   if (IsEnumContains(this->CombSt_, CombSt_CurrIsUnit))
      delete this->Curr_.Unit_;
   else {
      delete this->Curr_.Expr_;
   }
   if (IsEnumContains(this->CombSt_, CombSt_RightIsUnit))
      delete this->Right_.Unit_;
   else {
      delete this->Right_.Expr_;
   }
}
bool OmsCxExpr::IsTrue() const noexcept {
   const bool isCurrTrue = (IsEnumContains(this->CombSt_, CombSt_CurrIsUnit)
                            ? IsOmsCxUnitSt_True(this->Curr_.Unit_->CurrentSt())
                            : this->Curr_.Expr_->IsTrue());
   const CombSt opr = (this->CombSt_ & CombSt_OpMask);
   if (opr == CombSt{}) {
      assert(this->Right_.Unit_ == nullptr);
      return isCurrTrue;
   }
   assert(this->Right_.Unit_ != nullptr);
   if (opr == CombSt_OR) {
      if (isCurrTrue)
         return true;
      return this->IsRightTrue();
   }
   if (opr == CombSt_AND) {
      if (isCurrTrue)
         return this->IsRightTrue();
      return SkipRightUntilOpOR(this);
   }
   return false;
}
bool OmsCxExpr::SkipRightUntilOpOR(const OmsCxExpr* from) noexcept {
   for (;;) {
      assert((from->CombSt_ & CombSt_OpMask) == CombSt_AND);
      if (IsEnumContains(from->CombSt_, CombSt_RightIsUnit))
         return false;
      from = from->Right_.Expr_;
      const CombSt opr = (from->CombSt_ & CombSt_OpMask);
      if (opr == CombSt{})
         return false;
      if (opr == CombSt_OR)
         return from->IsRightTrue();
   }
}
//--------------------------------------------------------------------------//
void OmsCxExpr::RevPrintLeaf(fon9::RevBuffer& rbuf, Leaf leaf, CombSt combSt, CombSt maskIsUnit) {
   if (IsEnumContains(combSt, maskIsUnit)) {
      // Leaf is Unit;
      leaf.Unit_->RevPrint(rbuf);
   }
   else if (maskIsUnit == CombSt_CurrIsUnit) {
      // [Curr Leaf] is Expr;
      fon9::RevPrint(rbuf, '(');
      leaf.Expr_->RevPrint(rbuf);
      fon9::RevPrint(rbuf, ')');
   }
   else {
      // [Right Leaf] is Expr;
      leaf.Expr_->RevPrint(rbuf);
   }
}
void OmsCxExpr::RevPrint(fon9::RevBuffer& rbuf) const {
   RevPrintLeaf(rbuf, this->Curr_, this->CombSt_, CombSt_CurrIsUnit);
   if ((this->CombSt_ & CombSt_OpMask) != CombSt{}) {
      assert(this->Right_.Expr_ != nullptr);
      if (IsEnumContains(this->CombSt_, CombSt_OR))
         fon9::RevPrint(rbuf, '|');
      if (IsEnumContains(this->CombSt_, CombSt_AND))
         fon9::RevPrint(rbuf, '&');
      RevPrintLeaf(rbuf, this->Right_, this->CombSt_, CombSt_RightIsUnit);
   }
}

} // namespaces
