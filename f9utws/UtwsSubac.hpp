// \file f9utws/UtwsSubac.hpp
//
// 台股子帳的資料結構.
//
// \author fonwinz@gmail.com
#ifndef __f9utws_UtwsSubac_hpp__
#define __f9utws_UtwsSubac_hpp__
#include "f9omstw/OmsSubac.hpp"
#include "f9utws/UtwsIvSc.hpp"

namespace f9omstw {

/// 子帳基本資料.
struct UtwsSubacInfo {
   fon9::fmkt::BigAmt   OrderLimit_;
};

class UtwsSubac : public OmsSubac {
   fon9_NON_COPY_NON_MOVE(UtwsSubac);
   using base = OmsSubac;
   struct PodOp;
public:
   UtwsSubacInfo  Info_;
   UtwsIvSc       Sc_;

   using base::base;

   static fon9::seed::LayoutSP MakeLayout();

   void OnPodOp(OmsSubacTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) override;
};
using UtwsSubacSP = fon9::intrusive_ptr<UtwsSubac>;

} // namespaces
#endif//__f9utws_UtwsSubac_hpp__
