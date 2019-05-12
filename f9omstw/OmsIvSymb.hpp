// \file f9omstw/OmsIvSymb.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsIvSymb_hpp__
#define __f9omstw_OmsIvSymb_hpp__
#include "f9omstw/OmsTreeSvctSet.hpp"
#include "f9omstw/OmsIvBase.hpp"
#include "fon9/SortedVector.hpp"

namespace f9omstw {

/// 帳號商品風控資料(基底): 庫存、今日委託量...
/// - 衍生出: UtwsIvSymb, UtwfIvSymb;
class OmsIvSymb : public fon9::intrusive_ref_counter<OmsIvSymb> {
   fon9_NON_COPY_NON_MOVE(OmsIvSymb);
public:
   OmsIvSymb(const fon9::StrView& symbid) : SymbId_{symbid} {
   }
   virtual ~OmsIvSymb();

   char                    padding___[4];
   const fon9::CharVector  SymbId_;

   virtual void MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) = 0;
   virtual void OnPodOp(OmsTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) = 0;
};
//--------------------------------------------------------------------------//
struct OmsIvSymbSP_Comper {
   bool operator()(const OmsIvSymbSP& lhs, const OmsIvSymbSP& rhs) const {
      return lhs->SymbId_ < rhs->SymbId_;
   }
   bool operator()(const fon9::StrView& lhs, const OmsIvSymbSP& rhs) const {
      return lhs < ToStrView(rhs->SymbId_);
   }
   bool operator()(const OmsIvSymbSP& lhs, const fon9::StrView& rhs) const {
      return ToStrView(lhs->SymbId_) < rhs;
   }
};
using OmsIvSymbs = fon9::SortedVectorSet<OmsIvSymbSP, OmsIvSymbSP_Comper>;
using OmsIvSymbTree = OmsTreeSvctSet<OmsIvSymbs, OmsIvBaseSP>;

} // namespaces
#endif//__f9omstw_OmsIvSymb_hpp__
