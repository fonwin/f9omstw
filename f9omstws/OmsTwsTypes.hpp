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

enum OmsTwsTradingSessionIdx : uint8_t {
   OmsTwsTradingSessionIdx_Normal = 0,
   OmsTwsTradingSessionIdx_Fixed = 1,
   OmsTwsTradingSessionIdx_Odd = 2,
};

fon9_MSC_WARN_DISABLE(4582  // constructor is not implicitly called
                      4587  // 'OmsTwsAmtBS::Buy_' behavior change : constructor is no longer implicitly called
                      4201);// nonstandard extension used: nameless struct/union
struct OmsTwsQtyBS {
   OmsTwsQty   Buy_{0};
   OmsTwsQty   Sell_{0};

   OmsTwsQtyBS() {
   }
   void Clear() {
      fon9::ForceZeroNonTrivial(this);
   }
   OmsTwsQty& BS(OmsBSIdx idx) {
      assert(idx == OmsBSIdx_Buy || idx == OmsBSIdx_Sell);
      return *(&this->Buy_ + idx);
   }
   const OmsTwsQty& BS(OmsBSIdx idx) const {
      assert(idx == OmsBSIdx_Buy || idx == OmsBSIdx_Sell);
      return *(&this->Buy_ + idx);
   }
};

struct OmsTwsAmtBS {
   OmsTwsAmt   Buy_{};
   OmsTwsAmt   Sell_{};

   OmsTwsAmtBS() {
   }
   void Clear() {
      fon9::ForceZeroNonTrivial(this);
   }
   OmsTwsAmt& BS(OmsBSIdx idx) {
      assert(idx == OmsBSIdx_Buy || idx == OmsBSIdx_Sell);
      return *(&this->Buy_ + idx);
   }
   const OmsTwsAmt& BS(OmsBSIdx idx) const {
      assert(idx == OmsBSIdx_Buy || idx == OmsBSIdx_Sell);
      return *(&this->Buy_ + idx);
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

/// 每個商品, 在不同 Trading Session 的參考資料.
struct TrsRefs {
   f9omstw::OmsTwsPri   PriRef_{};
   f9omstw::OmsTwsPri   PriUpLmt_{};
   f9omstw::OmsTwsPri   PriDnLmt_{};
   ActMarketPri         ActMarketPri_{};
   GnDayTrade           GnDayTrade_{};
   char                 Padding___[2];

   void Clear() {
      fon9::ForceZeroNonTrivial(this);
   }
   void CopyFrom(const TrsRefs& rhs) {
      *this = rhs;
   }
};

/// 相關文件簽署情況.
enum class TwsIvScSignFlag : uint8_t {
   None = 0,
   /// 當沖先買後賣概括授權書: DARISK.GnRvRigh_T; .GnRvRigh_O;
   GnRvRighB = 0x01,
   /// 當沖先買後賣風險預告書: DARISK.GnRvRisk_T; .GnRvRigh_O;
   GnRvRiskB = 0x02,
   /// 當沖先賣後買概括授權書: DARISK.GnRvRighBS_T; .GnRvRigh_O;
   GnRvRighS = 0x04,
   /// 當沖先賣後買風險預告書: DARISK.GnRvRiskBS_T; .GnRvRigh_O;
   GnRvRiskS = 0x08,
};
fon9_ENABLE_ENUM_BITWISE_OP(TwsIvScSignFlag);

/// 帳號相關管控.
enum class TwsIvScRightFlag : uint8_t {
   /// 允許 [現股先買後賣當沖]: CUST.GnRvRigh_;
   AllowGnRvB = 0x01,
   /// 允許 [現股先賣後買當沖]: CUST.GnRvSBRight_;
   AllowGnRvS = 0x02,
   /// 現股只能庫存了結: 通常用在帳號有異常風險, or 想要關閉帳號.
   OnlyClearBalGn = 0x04,
   /// 信用只能庫存了結: 通常用在帳號有異常風險, or 信用帳號了結時;
   OnlyClearBalCD = 0x08,
};
fon9_ENABLE_ENUM_BITWISE_OP(TwsIvScRightFlag);

struct TwsIvScCtl {
   TwsIvScSignFlag   Signs_{};
   TwsIvScRightFlag  Rights_{};

   bool IsAllowGnRvB() const {
      return IsEnumContainsAny(this->Rights_, TwsIvScRightFlag::AllowGnRvB | TwsIvScRightFlag::AllowGnRvS)
         && IsEnumContainsAny(this->Signs_, TwsIvScSignFlag::GnRvRighB | TwsIvScSignFlag::GnRvRighS)
         && IsEnumContainsAny(this->Signs_, TwsIvScSignFlag::GnRvRiskB | TwsIvScSignFlag::GnRvRiskS);
   }
   bool IsAllowGnRvS() const {
      return IsEnumContains(this->Rights_, TwsIvScRightFlag::AllowGnRvS)
         && IsEnumContains(this->Signs_, TwsIvScSignFlag::GnRvRighS)
         && IsEnumContains(this->Signs_, TwsIvScSignFlag::GnRvRiskS);
   }
};

//--------------------------------------------------------------------------//
static inline char* ToStrRev(char* pout, TwsIvScSignFlag value) {
   return fon9::HexToStrRev(pout, fon9::cast_to_underlying(value));
}
static inline char* ToStrRev(char* pout, TwsIvScRightFlag value) {
   return fon9::HexToStrRev(pout, fon9::cast_to_underlying(value));
}
} // namespace f9omstw

//--------------------------------------------------------------------------//
#include "fon9/seed/FieldMaker.hpp"
namespace fon9 { namespace seed {
static inline FieldSP MakeField(Named&& named, int32_t ofs, f9omstw::TwsIvScSignFlag&) {
   return FieldSP{new FieldIntHx<underlying_type_t<f9omstw::TwsIvScSignFlag>>{std::move(named), ofs}};
}
static inline FieldSP MakeField(Named&& named, int32_t ofs, f9omstw::TwsIvScRightFlag&) {
   return FieldSP{new FieldIntHx<underlying_type_t<f9omstw::TwsIvScRightFlag>>{std::move(named), ofs}};
}
} } // namespace fon9, namespace seed

#endif//__f9omstws_OmsTwsTypes_hpp__
