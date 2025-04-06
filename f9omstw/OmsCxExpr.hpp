// \file f9omstw/OmsCxExpr.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCxExpr_hpp__
#define __f9omstw_OmsCxExpr_hpp__
#include "f9omstw/OmsCxUnit.hpp"

namespace f9omstw {

/// 條件判斷運算式.
class OmsCxExpr {
   fon9_NON_COPYABLE(OmsCxExpr);
   friend struct OmsCxCreatorInternal;
public:
   enum CombSt : uint8_t {
      CombSt_CurrIsUnit = 0x01,
      CombSt_RightIsUnit = 0x02,
      CombSt_OpMask = 0xf0,
      CombSt_OR = 0x10,
      CombSt_AND = 0x20,
   };

   OmsCxExpr() {
      fon9::ForceZeroNonTrivial(this);
   }
   ~OmsCxExpr();

   bool IsCxUnit() const {
      return((this->CombSt_ &  (CombSt_OpMask | CombSt_CurrIsUnit)) == CombSt_CurrIsUnit);
   }
   /// 單一 CxUnit 或 CxExpr;
   bool IsSingleCx() const {
      return((this->CombSt_ &  CombSt_OpMask) == CombSt{});
   }

   bool IsTrue() const noexcept;

   /// 將運算式內容用字串輸出.
   void RevPrint(fon9::RevBuffer& rbuf) const;

protected:
   OmsCxExpr(OmsCxExpr&& from) {
      memcpy(this, &from, sizeof(from));
      fon9::ForceZeroNonTrivial(from);
   }
   OmsCxExpr& operator=(OmsCxExpr&& from) = delete;

   bool IsRightTrue() const noexcept {
      return ((this->CombSt_ & CombSt_RightIsUnit) == CombSt_RightIsUnit
              ? IsOmsCxUnitSt_True(this->Right_.Unit_->CurrentSt())
              : this->Right_.Expr_->IsTrue());
   }
   static bool SkipRightUntilOpOR(const OmsCxExpr* from) noexcept;

   union Leaf {
      OmsCxUnit* Unit_;
      OmsCxExpr* Expr_;
   };
   Leaf     Curr_;
   Leaf     Right_;
   CombSt   CombSt_;
   uint8_t  CxReqSt_;
   char     Padding6____[6];

   static void RevPrintLeaf(fon9::RevBuffer& rbuf, Leaf leaf, CombSt combSt, CombSt maskIsUnit);
};
fon9_ENABLE_ENUM_BITWISE_OP(OmsCxExpr::CombSt);

} // namespaces
#endif//__f9omstw_OmsCxExpr_hpp__
