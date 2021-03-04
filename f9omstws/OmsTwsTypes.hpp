// \file f9omstws/OmsTwsTypes.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsTypes_hpp__
#define __f9omstws_OmsTwsTypes_hpp__
#include "f9omstw/OmsTypes.hpp"
#include "f9tws/ExgTypes.hpp"
#include "fon9/Decimal.hpp"

namespace f9omstw {

class OmsTwsRequestIni;

using OmsTwsPri = fon9::Decimal<uint32_t, 4>;
using OmsTwsQty = uint32_t;
using OmsTwsAmt = fon9::Decimal<uint64_t, 4>;
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
      this->Clear();
   }
   void Clear() {
      fon9::ForceZeroNonTrivial(this);
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
      this->Clear();
   }
   void Clear() {
      fon9::ForceZeroNonTrivial(this);
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

   void Clear() {
      this->Normal_.Clear();
      this->Fixed_.Clear();
      this->Odd_.Clear();
   }

   OmsTwsAmtBS& AmtTX(OmsTwsTradingSessionIdx idx) {
      return (&this->Normal_)[idx];
   }
   const OmsTwsAmtBS& AmtTX(OmsTwsTradingSessionIdx idx) const {
      return (&this->Normal_)[idx];
   }
};

/// 市價單的下單行為.
enum class ActMarketPri : char {
   /// 直接使用市價送交易所.
   Market = 0,
   /// 使用漲跌停限價.
   Limit = 'L',
   /// 拒絕下單.
   Reject = 'J',
};
fon9_DEFINE_EnumChar_StrTo_toupper(ActMarketPri);

enum class GnDayTrade : char {
   /// 為空白時(轉檔時填入'\0')，表示該證券不可現股當沖。
   Reject = 0,
   /// 值為Y時，表示該證券可先買後賣現股當沖。
   MustBuyFirst = 'Y',
   /// 值為X時，表示該證券可先買後賣或先賣後買現股當沖。
   BothSide = 'X',
};
fon9_DEFINE_EnumChar_StrTo_toupper(GnDayTrade);

struct OmsTwsPriRefs {
   f9omstw::OmsTwsPri   PriRef_{};
   f9omstw::OmsTwsPri   PriUpLmt_{};
   f9omstw::OmsTwsPri   PriDnLmt_{};
   ActMarketPri         ActMarketPri_{};
   GnDayTrade           GnDayTrade_{};
   char                 Padding___[2];

   void ClearPriRefs() {
      fon9::ForceZeroNonTrivial(this);
   }
   void CopyFromPriRefs(const OmsTwsPriRefs& rhs) {
      *this = rhs;
   }
};

} // namespaces
#endif//__f9omstws_OmsTwsTypes_hpp__
