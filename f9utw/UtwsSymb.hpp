// \file f9utw/UtwsSymb.hpp
// \author fonwinz@gmail.com
#ifndef __f9utw_UtwsSymb_hpp__
#define __f9utw_UtwsSymb_hpp__
#include "f9omstw/OmsSymb.hpp"
#include "f9omstwf/OmsTwfExgSymbBasic.hpp"

namespace f9omstw {

/// 這裡提供一個 OMS 商品基本資料的範例.
class UtwsSymb : public OmsSymb, public TwfExgSymbBasic {
   fon9_NON_COPY_NON_MOVE(UtwsSymb);
   using base = OmsSymb;
public:
   using base::base;

   fon9::fmkt::SymbData* GetSymbData(int tabid) override;
   fon9::fmkt::SymbData* FetchSymbData(int tabid) override;

   static fon9::seed::LayoutSP MakeLayout(fon9::seed::TreeFlag flags);

   static OmsSymbSP SymbMaker(const fon9::StrView& symbid) {
      return new UtwsSymb{symbid};
   }
};

} // namespaces
#endif//__f9utw_UtwsSymb_hpp__
