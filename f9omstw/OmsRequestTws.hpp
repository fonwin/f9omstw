// \file f9omstw/OmsRequestTws.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestTws_hpp__
#define __f9omstw_OmsRequestTws_hpp__
#include "f9omstw/OmsRequest.hpp"
#include "f9tws/ExgTypes.hpp"

namespace f9omstw {

enum class TwsOType : char {
   /// 現股.
   Gn = '0',
   /// 代辦融資.
   CrAgent = '1',
   /// 代辦融券.
   DbAgent = '2',
   /// 自辦融資.
   CrSelf = '3',
   /// 自辦融券.
   DbSelf = '4',
   /// 借券賣出.
   SBL5 = '5',
   /// 避險套利借券賣出.
   SBL6 = '6',

   /// 現股當沖.
   DayTradeGn = 'a',
   /// 信用當沖.
   DayTradeCD = 'A',
};

using OmsTwsPri = fon9::Decimal<uint32_t, 2>;
using OmsTwsQty = uint32_t;
using OmsTwsAmt = fon9::Decimal<uint64_t, 2>;
using OmsTwsSymbol = fon9::CharAry<sizeof(f9tws::StkNo)>;

/// 需要再配合 fon9::fmkt::TradingRequest 裡面的 Market_; Session_; 才能組成完整的 Key;
struct TwsOrdKey {
   f9tws::BrkId   BrkId_;
   f9tws::OrdNo   OrdNo_;

   TwsOrdKey() {
      memset(this, 0, sizeof(*this));
   }
};

struct OmsRequestTwsNewDat {
   OmsTwsSymbol      Symbol_;
   f9fmkt_Side       Side_;
   TwsOType          OType_;

   /// 數量(股數).
   OmsTwsQty         Qty_;
   OmsTwsPri         Pri_;
   f9fmkt_PriType    PriType_;

   /// 投資人下單類別: ' '=一般, 'A'=自動化設備, 'D'=DMA, 'I'=Internet, 'V'=Voice, 'P'=API;
   fon9::CharAry<1>  IvacNoFlag_;
   char              padding____[2];

   OmsRequestTwsNewDat() {
      memset(this, 0, sizeof(*this));
   }
};

class OmsRequestTwsNew : public OmsRequestNew, public TwsOrdKey, public OmsRequestTwsNewDat {
   fon9_NON_COPY_NON_MOVE(OmsRequestTwsNew);
   using base = OmsRequestNew;
public:
   using base::base;
   OmsRequestTwsNew() = default;

   static void MakeFields(fon9::seed::Fields& flds);
};

class OmsRequestTwsChg : public OmsRequestUpd, public TwsOrdKey {
   fon9_NON_COPY_NON_MOVE(OmsRequestTwsChg);
   using base = OmsRequestUpd;
public:
   using base::base;
   OmsRequestTwsChg() = default;

   static void MakeFields(fon9::seed::Fields& flds);

   char     padding___[3];
   using QtyType = fon9::make_signed_t<OmsTwsQty>;
   /// 數量(股數): =0刪單, >0期望改後數量(含成交), <0想要減少的數量.
   QtyType  Qty_{0};

   /// 檢查並設定 RequestKind, 然後返回 base::PreCheck();
   bool PreCheck(OmsRequestRunner& reqRunner) override;
};

class OmsRequestTwsMatch : public OmsRequestMatch {
   fon9_NON_COPY_NON_MOVE(OmsRequestTwsMatch);
   using base = OmsRequestMatch;
public:
   using base::base;
   OmsRequestTwsMatch() = default;

   /// 取得方式: static_cast<const OmsOrderTwsRaw*>(this->LastUpdated())->LastMatTime_;
   fon9::DayTime Time() const;
   /// 取得方式: static_cast<const OmsOrderTwsRaw*>(this->LastUpdated())->CumQty_ - prev->CumQty_;
   OmsTwsQty Qty() const;
   OmsTwsPri Pri() const;
   OmsTwsAmt Amt() const;
};

} // namespaces
#endif//__f9omstw_OmsRequestTws_hpp__
