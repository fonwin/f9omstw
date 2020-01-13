// \file f9omstwf/OmsTwfTypes.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfTypes_hpp__
#define __f9omstwf_OmsTwfTypes_hpp__
#include "f9omstw/OmsTypes.hpp"
#include "f9twf/ExgTmpTypes.hpp"
#include "fon9/CharAryL.hpp"
#include "fon9/Decimal.hpp"
#include "fon9/fmkt/FmktTypes.hpp"

namespace f9omstw {

/// 配合 P08 的小數位最大值: 小數最多 9 位.
/// 但也要配合 fmkt 的 Pri = Decimal<int64_t, 8>; 這樣 symb->PriceOrigDiv_ 才能共用;
using OmsTwfPri = fon9::fmkt::Pri;
using OmsTwfAmt = fon9::fmkt::Amt;
using OmsTwfQty = f9twf::TmpQty_t;
using OmsTwfSymbol = f9twf::SymbolId;
using OmsTwfPosEff = f9twf::ExgPosEff;

union OmsTwfQtyBS {
   struct {
      OmsTwfQty   Buy_;
      OmsTwfQty   Sell_;
   }              Get_;
   OmsTwfQty      BS_[2];

   OmsTwfQtyBS() {
      memset(this, 0, sizeof(*this));
   }
};

fon9_MSC_WARN_DISABLE(4582); // constructor is not implicitly called
union OmsTwfAmtBS {
   struct {
      OmsTwfAmt   Buy_;
      OmsTwfAmt   Sell_;
   }              Get_;
   OmsTwfAmt      BS_[2];

   OmsTwfAmtBS() {
      memset(this, 0, sizeof(*this));
   }
};
fon9_MSC_WARN_POP;

/// 可用於累計成交價量, 總成價金, 成交均價 = Amt_ / Qty_;
struct OmsTwfOrderQtys {
   OmsTwfQty      BeforeQty_;
   OmsTwfQty      AfterQty_;
   OmsTwfQty      LeavesQty_;
   OmsTwfQty      CumQty_;
   OmsTwfAmt      CumAmt_;

   void ContinuePrevQty(const OmsTwfOrderQtys& prev) {
      this->BeforeQty_ = this->AfterQty_ = prev.LeavesQty_;
   }
   void OnOrderReject() {
      this->AfterQty_ = this->LeavesQty_ = 0;
   }
};
static_assert(sizeof(OmsTwfOrderQtys) == 16, "pack OmsTwfOrderQtys?");

} // namespaces
#endif//__f9omstwf_OmsTwfTypes_hpp__
