// \file f9omstw/OmsSubac.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsSubac_hpp__
#define __f9omstw_OmsSubac_hpp__
#include "f9omstw/OmsIvBase.hpp"
#include "f9omstw/OmsTreeSvctSet.hpp"

namespace f9omstw {

class OmsSubac : public OmsIvBase {
   fon9_NON_COPY_NON_MOVE(OmsSubac);
   using base = OmsIvBase;
public:
   const fon9::CharVector  SubacNo_;

   OmsSubac(const fon9::StrView& subacNo, OmsIvacSP parent);
   ~OmsSubac();

   /// 建立 grid view, 包含 SubacNo_; 不含尾端分隔符號.
   virtual void MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) = 0;
   virtual void OnPodOp(OmsTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) = 0;
};
using OmsSubacSP = fon9::intrusive_ptr<OmsSubac>;

struct OmsSubacSP_Comper {
   bool operator()(const OmsSubacSP& lhs, const OmsSubacSP& rhs) const {
      return lhs->SubacNo_ < rhs->SubacNo_;
   }
   bool operator()(const fon9::StrView& lhs, const OmsSubacSP& rhs) const {
      return lhs < ToStrView(rhs->SubacNo_);
   }
   bool operator()(const OmsSubacSP& lhs, const fon9::StrView& rhs) const {
      return ToStrView(lhs->SubacNo_) < rhs;
   }
};
using OmsSubacMap = fon9::SortedVectorSet<OmsSubacSP, OmsSubacSP_Comper>;
using OmsSubacTree = OmsTreeSvctSet<OmsSubacMap, OmsIvBaseSP>;

} // namespaces
#endif//__f9omstw_OmsSubac_hpp__
