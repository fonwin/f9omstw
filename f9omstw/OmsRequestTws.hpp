// \file f9omstw/OmsRequestTws.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestTws_hpp__
#define __f9omstw_OmsRequestTws_hpp__
#include "f9omstw/OmsRequestTrade.hpp"
#include "f9omstw/OmsRequestFilled.hpp"
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

struct OmsRequestTwsIniDat {
   OmsTwsSymbol      Symbol_;
   char              padding_____[6];

   /// 投資人下單類別: ' '=一般, 'A'=自動化設備, 'D'=DMA, 'I'=Internet, 'V'=Voice, 'P'=API;
   fon9::CharAry<1>  IvacNoFlag_;
   f9fmkt_Side       Side_;
   TwsOType          OType_;
   f9fmkt_PriType    PriType_;
   OmsTwsPri         Pri_;
   OmsTwsQty         Qty_;

   OmsRequestTwsIniDat() {
      memset(this, 0, sizeof(*this));
   }
};

class OmsRequestTwsIni : public OmsRequestIni, public OmsRequestTwsIniDat {
   fon9_NON_COPY_NON_MOVE(OmsRequestTwsIni);
   using base = OmsRequestIni;
public:
   using base::base;
   OmsRequestTwsIni() = default;

   static void MakeFields(fon9::seed::Fields& flds);

   const char* IsIniFieldEqual(const OmsRequestBase& req) const override;

   /// - 若煤填 Market 或 SessionId, 則會設定 scRes.Symb_;
   /// - 如果沒填 SessionId, 則:
   ///   - f9fmkt_PriType_Limit && Pri.IsNull() 使用:  f9fmkt_TradingSessionId_FixedPrice;
   ///   - Qty < scRes.Symb_->ShUnit_ 使用:            f9fmkt_TradingSessionId_OddLot;
   ///   - 其他:                                       f9fmkt_TradingSessionId_Normal;
   const OmsRequestIni* PreCheck_OrdKey(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) override;
};

class OmsRequestTwsChg : public OmsRequestUpd {
   fon9_NON_COPY_NON_MOVE(OmsRequestTwsChg);
   using base = OmsRequestUpd;
public:
   using QtyType = fon9::make_signed_t<OmsTwsQty>;
   /// 數量(股數): =0刪單, >0期望改後數量(含成交), <0想要減少的數量.
   QtyType  Qty_{0};
   char     padding_____[4];

   using base::base;
   OmsRequestTwsChg() = default;

   static void MakeFields(fon9::seed::Fields& flds);

   /// 如果沒填 RequestKind, 則根據 Qty 設定 RequestKind.
   /// 然後返回 base::ValidateInUser();
   bool ValidateInUser(OmsRequestRunner& reqRunner) override;
};

class OmsRequestTwsFilled : public OmsRequestFilled {
   fon9_NON_COPY_NON_MOVE(OmsRequestTwsFilled);
   using base = OmsRequestFilled;
public:
   using base::base;
   OmsRequestTwsFilled() = default;

   static void MakeFields(fon9::seed::Fields& flds) {
      base::MakeFields<OmsRequestTwsFilled>(flds);
   }

   /// 取得方式: static_cast<const OmsOrderTwsRaw*>(this->LastUpdated())->LastFilledTime_;
   fon9::DayTime Time() const;
   /// 取得方式: static_cast<const OmsOrderTwsRaw*>(this->LastUpdated())->CumQty_ - prev->CumQty_;
   OmsTwsQty Qty() const;
   OmsTwsPri Pri() const;
   OmsTwsAmt Amt() const;
};

} // namespaces
#endif//__f9omstw_OmsRequestTws_hpp__
