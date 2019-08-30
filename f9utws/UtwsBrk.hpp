// \file f9utws/UtwsBrk.hpp
// \author fonwinz@gmail.com
#ifndef __f9utws_UtwsBrk_hpp__
#define __f9utws_UtwsBrk_hpp__
#include "f9omstw/OmsBrk.hpp"

namespace f9omstw {

/// 這裡是一個券商資料的範例.
class UtwsBrk : public OmsBrk {
   fon9_NON_COPY_NON_MOVE(UtwsBrk);
   using base = OmsBrk;
   struct PodOp;

protected:
   OmsIvacSP MakeIvac(IvacNo) override;

public:
   using base::base;

   static OmsBrkSP BrkMaker(const fon9::StrView& brkid) {
      return new UtwsBrk{brkid};
   }
   static fon9::seed::LayoutSP MakeLayout(fon9::seed::TreeFlag flags);
};

} // namespaces
#endif//__f9utws_UtwsBrk_hpp__
