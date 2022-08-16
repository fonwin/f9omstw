// \file f9omstw/OmsScBase.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsScBase.hpp"
#include "f9omstw/OmsMdEvent.hpp"

namespace f9omstw {

fon9::EnabledYN  gIsScLogAll;

OmsErrCode CheckLmtPri(fon9::fmkt::Pri curpri, fon9::fmkt::Pri upLmt, fon9::fmkt::Pri dnLmt, fon9::RevBuffer& rbuf) {
   if (fon9_UNLIKELY(curpri.IsNull()))
      return OmsErrCode_Sc_BadPri;
   if (fon9_LIKELY(!upLmt.IsNull())) {
      if (fon9_UNLIKELY(curpri > upLmt)) {
         fon9::RevPrint(rbuf, "UpLmt=", upLmt);
         return OmsErrCode_Sc_OverPriUpLmt;
      }
   }
   if (fon9_LIKELY(!dnLmt.IsNull())) {
      if (fon9_UNLIKELY(curpri < dnLmt)) {
         fon9::RevPrint(rbuf, "DnLmt=", dnLmt);
         return OmsErrCode_Sc_OverPriDnLmt;
      }
   }
   return OmsErrCode_NoError;
}

OmsErrCode GetExecPri(OmsSymb& mdSymb, const f9fmkt_ExecPriSel execPriSel, const int8_t priTicksAway, fon9::fmkt::Pri& out) {
   switch (execPriSel) {
   default:
   case f9fmkt_ExecPriSel_Unknown:
      return OmsErrCode_NoError;
   case f9fmkt_ExecPriSel_Bid:
   case f9fmkt_ExecPriSel_Ask:
      if (mdSymb.MdReceiverSt() == fon9::fmkt::MdReceiverSt::DataBroken)
         return OmsErrCode_MdNoData;
      assert(mdSymb.GetMdBSEv() != nullptr);
      if (const auto* bs = mdSymb.GetMdBSEv()) {
         const auto& pq = (execPriSel == f9fmkt_ExecPriSel_Bid ? bs->Buy_ : bs->Sell_);
         if (pq.Qty_ > 0) {
            out = pq.Pri_;
            goto __ADJ_TICKS;
         }
      }
      // 沒有買賣價, 使用最後成交價.
      goto __SEL_LastPrice;
   case f9fmkt_ExecPriSel_LastPrice:
      if (mdSymb.MdReceiverSt() == fon9::fmkt::MdReceiverSt::DataBroken)
         return OmsErrCode_MdNoData;
   __SEL_LastPrice:;
      assert(mdSymb.GetMdLastPriceEv() != nullptr);
      if (const auto* lastpri = mdSymb.GetMdLastPriceEv())
         out = lastpri->LastPrice_;
      else {
         return OmsErrCode_CondSc_MdSymb;
      }
   __ADJ_TICKS:;
      assert(mdSymb.LvPriSteps() != nullptr);
      if (const auto* lvPriSteps = mdSymb.LvPriSteps()) {
         if (priTicksAway > 0) {
            fon9::fmkt::Pri upLmt;
            mdSymb.GetPriLmt(&upLmt, nullptr);
            out = fon9::fmkt::MoveTicksUp(lvPriSteps, out, fon9::unsigned_cast(priTicksAway), upLmt);
         }
         else if (priTicksAway < 0) {
            fon9::fmkt::Pri dnLmt;
            mdSymb.GetPriLmt(nullptr, &dnLmt);
            out = fon9::fmkt::MoveTicksDn(lvPriSteps, out, static_cast<uint16_t>(-priTicksAway), dnLmt);
         }
      }
      break;
   }
   return OmsErrCode_NoError;
}

bool CheckPriTickSizeAndLmt(OmsRequestRunnerInCore& runner, OmsSymb& mdSymb, fon9::fmkt::Pri reqPri) {
   // 檢查 reqPri 的漲跌停範圍, 檔位;
   if (!fon9::fmkt::CheckPriTickSize(mdSymb.LvPriSteps(), reqPri)) {
      runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_BadPriTickSize, nullptr);
      return false;
   }
   fon9::fmkt::Pri upLmt, dnLmt;
   if (mdSymb.GetPriLmt(&upLmt, &dnLmt)) {
      fon9::RevBufferFixedSize<256> rbuf;
      OmsErrCode errCode = CheckLmtPri(reqPri, upLmt, dnLmt, rbuf);
      if (errCode != OmsErrCode_NoError) {
         runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, errCode, ToStrView(rbuf));
         return false;
      }
   }
   return true;
}

} // namespaces
