// \file f9omstwf/OmsTwfRptTools.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfRptTools.hpp"

namespace f9omstw {

void SetupReport0Base(OmsRequestBase& rpt, f9twf::TmpFcmId ivacFcmId, f9twf::ExgSystemType exgSysType, const ExgMaps& exgMaps) {
   switch (exgSysType) {
   case f9twf::ExgSystemType::OptNormal:
      rpt.SetMarket(f9fmkt_TradingMarket_TwOPT);
      rpt.SetSessionId(f9fmkt_TradingSessionId_Normal);
      break;
   case f9twf::ExgSystemType::FutNormal:
      rpt.SetMarket(f9fmkt_TradingMarket_TwFUT);
      rpt.SetSessionId(f9fmkt_TradingSessionId_Normal);
      break;
   case f9twf::ExgSystemType::OptAfterHour:
      rpt.SetMarket(f9fmkt_TradingMarket_TwOPT);
      rpt.SetSessionId(f9fmkt_TradingSessionId_AfterHour);
      break;
   case f9twf::ExgSystemType::FutAfterHour:
      rpt.SetMarket(f9fmkt_TradingMarket_TwFUT);
      rpt.SetSessionId(f9fmkt_TradingSessionId_AfterHour);
      break;
   }
   f9twf::BrkId brkid = exgMaps->MapBrkFcmId_.GetBrkId(f9twf::TmpGetValueU(ivacFcmId));
   rpt.BrkId_.CopyFrom(brkid.begin(), brkid.size());
}

void SetupReport0Symbol(OmsRequestBase&             rpt,
                        f9twf::TmpFcmId             ivacFcmId,
                        const f9twf::TmpSymbolType* pkSym,
                        OmsTwfSymbol&               rptSymbol,
                        const ExgMaps&              exgMaps,
                        f9twf::ExgSystemType        exgSysType) {
   SetupReport0Base(rpt, ivacFcmId, exgSysType, exgMaps);
   const auto symType = *pkSym++;
   if (f9twf::TmpSymbolTypeIsNum(symType)) {
      // SymbolId: 數字格式 => 文字格式.
      auto& p08s = exgMaps->MapP08Recs_[ExgSystemTypeToIndex(exgSysType)];
      auto* symNum = reinterpret_cast<const f9twf::TmpSymNum*>(pkSym);
      if (auto* leg1p08 = p08s.GetP08(symNum->Pseq1_)) {
         if (auto pseq2 = f9twf::TmpGetValueU(symNum->Pseq2_)) {
            if (auto* leg2p08 = p08s.GetP08(pseq2)) {
               fon9::StrView leg1id, leg2id;
               if (fon9_LIKELY(f9twf::TmpSymbolTypeIsShort(symType))) {
                  leg1id = ToStrView(leg1p08->ShortId_);
                  leg2id = ToStrView(leg2p08->ShortId_);
               }
               if (leg1id.empty() || leg2id.empty()) {
                  leg1id = ToStrView(leg1p08->LongId_);
                  leg2id = ToStrView(leg2p08->LongId_);
               }
               if (rpt.Market() == f9fmkt_TradingMarket_TwFUT)
                  f9twf::FutMakeCombSymbolId(rptSymbol, leg1id, leg2id, symNum->CombOp_);
               else
                  f9twf::OptMakeCombSymbolId(rptSymbol, leg1id, leg2id, symNum->CombOp_);
            }
         }
         else {
            const uint8_t* id;
            fon9_GCC_WARN_DISABLE("-Wswitch-bool");
            switch (f9twf::TmpSymbolTypeIsShort(symType)) {
            case true:
               if (*(id = leg1p08->ShortId_.LenChars()) != 0)
                  break;
               /* fall through */ // *id == 0: 沒有 ShortId, 則使用 LongId.
            default:
               id = leg1p08->LongId_.LenChars();
               break;
            }
            fon9_GCC_WARN_POP;
            rptSymbol.CopyFrom(reinterpret_cast<const char*>(id + 1), *id);
         }
      }
   }
   else {
      const char* pend = fon9::StrFindTrimTail(reinterpret_cast<const char*>(pkSym),
                                               reinterpret_cast<const char*>(pkSym)
                                               + (f9twf::TmpSymbolTypeIsLong(symType)
                                                  ? sizeof(f9twf::TmpSymIdL)
                                                  : sizeof(f9twf::TmpSymIdS)));
      rptSymbol.CopyFrom(reinterpret_cast<const char*>(pkSym),
                         pend - reinterpret_cast<const char*>(pkSym));
   }
}

} // namespaces
