// \file f9omstws/OmsTwsTypes.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsTypes_hpp__
#define __f9omstws_OmsTwsTypes_hpp__
#include "f9omstw/OmsTypes.hpp"
#include "f9tws/ExgTypes.hpp"
#include "fon9/Decimal.hpp"

namespace f9omstw {

class OmsTwsRequestIni;

using OmsTwsPri = fon9::Decimal<uint32_t, 2>;
using OmsTwsQty = uint32_t;
using OmsTwsAmt = fon9::Decimal<uint64_t, 2>;
using OmsTwsSymbol = fon9::CharAry<sizeof(f9tws::StkNo)>;
using OmsTwsOType = f9tws::TwsOType;

enum OmsTwsTradingSessionIdx : unsigned {
   OmsTwsTradingSessionIdx_Normal = 0,
   OmsTwsTradingSessionIdx_Fixed = 1,
   OmsTwsTradingSessionIdx_Odd = 2,
};

union OmsTwsQtyBS {
   struct {
      OmsTwsQty   Buy_;
      OmsTwsQty   Sell_;
   }              Get_;
   OmsTwsQty      BS_[2];

   OmsTwsQtyBS() {
      memset(this, 0, sizeof(*this));
   }
};

fon9_MSC_WARN_DISABLE(4582); // constructor is not implicitly called
union OmsTwsAmtBS {
   struct {
      OmsTwsAmt   Buy_;
      OmsTwsAmt   Sell_;
   }              Get_;
   OmsTwsAmt      BS_[2];

   OmsTwsAmtBS() {
      memset(this, 0, sizeof(*this));
   }
};
fon9_MSC_WARN_POP;

struct OmsTwsAmtTX {
   OmsTwsAmtBS Normal_;
   OmsTwsAmtBS Fixed_;
   OmsTwsAmtBS Odd_;
   static_assert(OmsTwsTradingSessionIdx_Normal == 0 &&
                 OmsTwsTradingSessionIdx_Fixed == 1 &&
                 OmsTwsTradingSessionIdx_Odd == 2,
                 "Check OmsTwsTradingSessionIdx.");

   OmsTwsAmtBS& AmtTX(OmsTwsTradingSessionIdx idx) {
      return (&this->Normal_)[idx];
   }
   const OmsTwsAmtBS& AmtTX(OmsTwsTradingSessionIdx idx) const {
      return (&this->Normal_)[idx];
   }
};

} // namespaces
#endif//__f9omstws_OmsTwsTypes_hpp__
