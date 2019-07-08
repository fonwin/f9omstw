// \file f9utws/UtwsIvSymb.hpp
//
// 台股帳號(子帳、母帳)的各商品風控資料(庫存量、委託量、成交量...).
//
// \author fonwinz@gmail.com
#ifndef __f9utws_UtwsIvSymb_hpp__
#define __f9utws_UtwsIvSymb_hpp__
#include "f9omstw/OmsIvSymb.hpp"
#include "fon9/fmkt/FmktTypes.hpp"

namespace f9omstw {

struct UtwsIvSymbSc {
   /// 現股股數.
   fon9::fmkt::Qty   GnQty_;
   /// 融資股數.
   fon9::fmkt::Qty   CrQty_;
   /// 融資餘額.
   fon9::fmkt::Amt   CrAmt_;
   /// 融券股數.
   fon9::fmkt::Qty   DbQty_;
   /// 融券餘額.
   fon9::fmkt::Amt   DbAmt_;

   UtwsIvSymbSc() {
      memset(this, 0, sizeof(*this));
   }
};

/// 證券帳號商品風控: 庫存、今日委託量...
/// 子帳、母帳使用相同的風控資料結構.
class UtwsIvSymb : public OmsIvSymb {
   fon9_NON_COPY_NON_MOVE(UtwsIvSymb);
   struct PodOp;
   using base = OmsIvSymb;
public:
   using base::base;

   UtwsIvSymbSc Bal_;
   UtwsIvSymbSc Ord_;
   UtwsIvSymbSc Filled_;

   static OmsIvSymbSP SymbMaker(const fon9::StrView& symbid, OmsIvBase* owner) {
      (void)owner;
      return new UtwsIvSymb{symbid};
   }
   static fon9::seed::LayoutSP MakeLayout();
   void MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) override;
   void OnPodOp(OmsTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) override;
};

} // namespaces
#endif//__f9utws_UtwsIvSymb_hpp__
