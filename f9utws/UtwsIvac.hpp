// \file f9utws/UtwsIvac.hpp
// \author fonwinz@gmail.com
#ifndef __f9utws_UtwsIvac_hpp__
#define __f9utws_UtwsIvac_hpp__
#include "f9utws/UtwsIvSc.hpp"
#include "f9omstw/OmsIvac.hpp"
#include "f9omstw/OmsSubac.hpp"
#include "fon9/LevelArray.hpp"

namespace f9omstw {

class UtwsSubac;
using UtwsSubacSP = fon9::intrusive_ptr<UtwsSubac>;

/// 投資帳號基本資料.
struct UtwsIvacInfo {
   fon9::CharVector     Name_;
   /// 投資上限.
   fon9::fmkt::BigAmt   OrderLimit_;
};

class UtwsIvac : public OmsIvac {
   fon9_NON_COPY_NON_MOVE(UtwsIvac);
   using base = OmsIvac;
   struct PodOp;

   OmsSubacSP MakeSubac(fon9::StrView subacNo) override;

public:
   UtwsIvacInfo Info_;
   UtwsIvSc     Sc_;

   using base::base;

   static fon9::seed::LayoutSP MakeLayout(fon9::seed::TreeFlag treeflags);
   /// 建立 grid view, 包含 IvacNo_; 不含尾端分隔符號.
   void MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) override;
   void OnPodOp(OmsIvacTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) override;
};
using UtwsIvacSP = fon9::intrusive_ptr<UtwsIvac>;

} // namespaces
#endif//__f9utws_UtwsIvac_hpp__
