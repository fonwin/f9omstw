// \file f9utw/UtwsIvSc.hpp
//
// 台股帳號(子帳、母帳)的共同風控資料結構.
//
// \author fonwinz@gmail.com
#ifndef __f9utw_UtwsIvSc_hpp__
#define __f9utw_UtwsIvSc_hpp__
#include "f9omstw/OmsIvSymb.hpp"
#include "fon9/fmkt/FmktTypes.hpp"

namespace f9omstw {

/// 帳號風控資料.
/// 子帳、母帳使用相同的風控資料結構.
struct UtwsIvSc {
   fon9_NON_COPY_NON_MOVE(UtwsIvSc);
   UtwsIvSc() = default;
   fon9::fmkt::BigAmt   TotalBuy_;
   fon9::fmkt::BigAmt   TotalSell_;
   OmsIvSymbs           Symbs_;

   /// ofsadj = fon9_OffsetOfRawPointer(UtwsIvac, Sc_)
   static void MakeFields(int ofsadj, fon9::seed::Fields& scflds);
   static fon9::seed::Fields MakeFields(int ofsadj) {
      fon9::seed::Fields scflds;
      MakeFields(ofsadj, scflds);
      return scflds;
   }
};

} // namespaces
#endif//__f9utw_UtwsIvSc_hpp__
