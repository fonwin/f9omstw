// \file f9omstwf/OmsTwfRptLineTmp.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfRptLineTmp.hpp"
#include "f9omstwf/OmsTwfTradingLineMgr.hpp"
#include "f9omstwf/OmsTwfReport3.hpp"
#include "f9twf/ExgTmpTradingR1.hpp"
#include "f9twf/ExgTmpTradingR7.hpp"
#include "f9twf/ExgTmpTradingR9.hpp"

namespace f9omstw {

TwfRptLineTmp::TwfRptLineTmp(TwfLineTmpWorker&            worker,
                             f9twf::ExgTradingLineMgr&    lineMgr,
                             const f9twf::ExgLineTmpArgs& lineArgs,
                             f9twf::ExgLineTmpLog&&       log)
   : base(lineMgr, lineArgs, std::move(log))
   , Worker_(worker) {
}
void TwfRptLineTmp::OnExgTmp_ApReady() {
}
void TwfRptLineTmp::OnExgTmp_ApBroken() {
}
//--------------------------------------------------------------------------//
inline f9fmkt_RxKind TmpExecTypeToRxKind(f9twf::TmpExecType execType) {
   switch (execType) {
   case f9twf::TmpExecType::New:     return f9fmkt_RxKind_RequestNew;
   case f9twf::TmpExecType::Delete:  return f9fmkt_RxKind_RequestDelete;
   case f9twf::TmpExecType::ChgQty:  return f9fmkt_RxKind_RequestChgQty;
   case f9twf::TmpExecType::ChgPriM:
   case f9twf::TmpExecType::ChgPrim: return f9fmkt_RxKind_RequestChgPri;
   case f9twf::TmpExecType::Query:   return f9fmkt_RxKind_RequestQuery;
   case f9twf::TmpExecType::FilledNew:
   case f9twf::TmpExecType::Filled:  return f9fmkt_RxKind_Filled;
   }
   return f9fmkt_RxKind_Unknown;
}
//--------------------------------------------------------------------------//
// 設定 rpt 的基本資料:
// - Market(Fut/Opt), TradingSessionId(Normal/AfterHour);
inline void SetupReportBase(OmsRequestBase& rpt, f9twf::ExgSystemType sysType) {
   switch (sysType) {
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
}
//--------------------------------------------------------------------------//
using ExgMaps = f9twf::ExgMapMgr::MapsConstLocker;
// 設定 rpt 的基本資料:
// - Market(Fut/Opt), TradingSessionId(Normal/AfterHour);
// - BrkId(根據 ivacFcmId);
static void SetupReport0Base(OmsRequestBase& rpt, f9twf::TmpFcmId ivacFcmId, f9twf::ExgSystemType sysType, const ExgMaps& exgMaps) {
   SetupReportBase(rpt, sysType);
   f9twf::BrkId brkid = exgMaps->MapBrkFcmId_.GetBrkId(f9twf::TmpGetValueU(ivacFcmId));
   rpt.BrkId_.CopyFrom(brkid.begin(), brkid.size());
}
// SetupReport0Base() 及 rptSymbol;
static void SetupReport0Symbol(OmsRequestBase& rpt, f9twf::TmpFcmId ivacFcmId,
                               const f9twf::TmpSymbolType* pkSym, OmsTwfSymbol& rptSymbol, TwfRptLineTmp& line) {
   ExgMaps exgMaps = line.LineMgr_.ExgMapMgr_->Lock();
   SetupReport0Base(rpt, ivacFcmId, line.SystemType(), exgMaps);
   const auto symType = *pkSym++;
   if (f9twf::TmpSymbolTypeIsNum(symType)) {
      // SymbolId: 數字格式 => 文字格式.
      auto& p08s = exgMaps->MapP08Recs_[ExgSystemTypeToIndex(line.SystemType())];
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
//--------------------------------------------------------------------------//
void SetupReportExt(...) {
}
inline void SetupReportExt(OmsTwfFilled* rpt, const f9twf::TmpR2Back* pkr2back) {
   rpt->IvacNo_ = f9twf::TmpGetValueU(pkr2back->IvacNo_);
   rpt->Qty_ = f9twf::TmpGetValueU(pkr2back->LastQty_);
   if (pkr2back->LegSide_[0] == f9twf::TmpSide::Single)
      rpt->Pri_.SetOrigValue(f9twf::TmpGetValueS(pkr2back->LastPx_));
   else
      rpt->Pri_.SetOrigValue(f9twf::TmpGetValueS(pkr2back->LegPx_[0]));
}
inline void SetupReportExt(OmsTwfFilled* rpt, const f9twf::TmpR22* pkr2back) {
   rpt->Qty_ = f9twf::TmpGetValueU(pkr2back->LegQty_[0]);
   rpt->Pri_.SetOrigValue(f9twf::TmpGetValueS(pkr2back->LegPx_[0]));
}
inline void SetupReportExt(OmsTwfReport2* rpt2, const f9twf::TmpR2Back* pkr2back) {
   rpt2->IvacNo_ = f9twf::TmpGetValueU(pkr2back->IvacNo_);
   rpt2->IvacNoFlag_ = pkr2back->IvacFlag_;
   rpt2->TimeInForce_ = TmpTimeInForceTo(pkr2back->TimeInForce_);
   rpt2->PriType_ = TmpPriTypeTo(pkr2back->PriType_);
}
inline void SetupReportExt(OmsTwfReport9* rpt9, const f9twf::TmpR2Back* pkr2back) {
   rpt9->IvacNo_ = f9twf::TmpGetValueU(pkr2back->IvacNo_);
   rpt9->IvacNoFlag_ = pkr2back->IvacFlag_;
   rpt9->TimeInForce_ = TmpTimeInForceTo(pkr2back->TimeInForce_);
}

template <class TmpBack>
OmsTwfSymbol* SetupReport2Back(OmsRequestRunner& runner, TwfRptLineTmp& line, f9fmkt_RxKind rxKind, const TmpBack& pkr2back) {
   if (rxKind == f9fmkt_RxKind_Filled) {
      // 單式商品成交、複式商品成交、新單合併成交、報價成交.
      OmsRequestFactory* fac;
      const auto         priLeg2 = f9twf::TmpGetValueS(pkr2back.LegPx_[1]);
      if (priLeg2 == 0)
         fac = &line.Worker_.Fil1Factory_;
      else
         fac = &line.Worker_.Fil2Factory_;
      runner.Request_ = fac->MakeReportIn(rxKind, line.LastRxTime());
      assert(dynamic_cast<OmsTwfFilled*>(runner.Request_.get()) != nullptr);
      OmsTwfFilled& rpt = *static_cast<OmsTwfFilled*>(runner.Request_.get());
      rpt.PriStyle_ = OmsReportPriStyle::NoDecimal;
      SetupReportExt(&rpt, &pkr2back);
      if (priLeg2 != 0) {
         assert(dynamic_cast<OmsTwfFilled2*>(&rpt) != nullptr);
         static_cast<OmsTwfFilled2*>(&rpt)->PriLeg2_.SetOrigValue(priLeg2);
      }
      rpt.Time_ = pkr2back.TransactTime_.ToTimeStamp();
      // 期交所刪除剩餘量.
      auto bfQty = f9twf::TmpGetValueU(pkr2back.BeforeQty_);
      auto afQty = f9twf::TmpGetValueU(pkr2back.LeavesQty_);
      if (afQty == 0 && bfQty > rpt.Qty_)
         rpt.QtyCanceled_ = static_cast<OmsTwfQty>(bfQty - rpt.Qty_);
      // (新單 or 新單合併成交) && (PriType == f9fmkt_PriType_Mwp): pkr2.Price_ = 實際委託價.
      rpt.PriOrd_.SetOrigValue(f9twf::TmpGetValueS(pkr2back.Price_));
      rpt.PosEff_ = pkr2back.PosEff_;
      rpt.Side_ = TmpSideTo(pkr2back.Side_);
      rpt.MatchKey_ = f9twf::TmpGetValueU(pkr2back.RptSeq_)
                    + line.OutPvcId_ * 10000000ull;
      return &rpt.Symbol_;
   }
   if (fon9_LIKELY(pkr2back.PosEff_ != f9twf::TmpPosEff::Quote)) {
      // 一般委託回報.
      runner.Request_ = line.Worker_.Rpt2Factory_.MakeReportIn(rxKind, line.LastRxTime());
      runner.Request_->SetReportSt(f9fmkt_TradingRequestSt_ExchangeAccepted);
      assert(dynamic_cast<OmsTwfReport2*>(runner.Request_.get()) != nullptr);
      OmsTwfReport2&  rpt2 = *static_cast<OmsTwfReport2*>(runner.Request_.get());
      rpt2.PriStyle_ = OmsReportPriStyle::NoDecimal;
      rpt2.ExgTime_ = pkr2back.TransactTime_.ToTimeStamp();
      rpt2.BeforeQty_ = f9twf::TmpGetValueU(pkr2back.BeforeQty_);
      rpt2.Qty_ = f9twf::TmpGetValueU(pkr2back.LeavesQty_);
      rpt2.Pri_.SetOrigValue(f9twf::TmpGetValueS(pkr2back.Price_));
      rpt2.PosEff_ = pkr2back.PosEff_;
      rpt2.Side_ = TmpSideTo(pkr2back.Side_);
      SetupReportExt(&rpt2, &pkr2back);
      return &rpt2.Symbol_;
   }
   // 報價委託回報: 會有2筆(Bid+Offer)連續, 使用 PartExchange 機制回報.
   runner.Request_ = line.Worker_.Rpt9Factory_.MakeReportIn(rxKind, line.LastRxTime());
   assert(dynamic_cast<OmsTwfReport9*>(runner.Request_.get()) != nullptr);
   OmsTwfReport9&  rpt9 = *static_cast<OmsTwfReport9*>(runner.Request_.get());
   rpt9.PriStyle_ = OmsReportPriStyle::NoDecimal;
   rpt9.ExgTime_ = pkr2back.TransactTime_.ToTimeStamp();
   // if (rxKind == f9fmkt_RxKind_RequestDelete) {
   //    // - 交易所主動刪單? 還是 User刪單?
   //    // - 這裡無法判斷, 所以放在 OmsTwfReport9::RunReportInCore_FromOrig() 處理;
   // }
   switch (rpt9.Side_ = TmpSideTo(pkr2back.Side_)) { // Bid or Offer.
   default:
      break;
   case f9fmkt_Side_Buy:
      rpt9.BidBeforeQty_ = f9twf::TmpGetValueU(pkr2back.BeforeQty_);
      rpt9.BidQty_ = f9twf::TmpGetValueU(pkr2back.LeavesQty_);
      rpt9.BidPri_.SetOrigValue(f9twf::TmpGetValueS(pkr2back.Price_));
      rpt9.SetReportSt(rxKind != f9fmkt_RxKind_RequestChgPri // 只有單邊改價,所以改價回報沒有Part.
                       ? f9fmkt_TradingRequestSt_PartExchangeAccepted
                       : f9fmkt_TradingRequestSt_ExchangeAccepted);
      break;
   case f9fmkt_Side_Sell:
      rpt9.OfferBeforeQty_ = f9twf::TmpGetValueU(pkr2back.BeforeQty_);
      rpt9.OfferQty_ = f9twf::TmpGetValueU(pkr2back.LeavesQty_);
      rpt9.OfferPri_.SetOrigValue(f9twf::TmpGetValueS(pkr2back.Price_));
      rpt9.SetReportSt(f9fmkt_TradingRequestSt_ExchangeAccepted);
      break;
   }
   SetupReportExt(&rpt9, &pkr2back);
   return &rpt9.Symbol_;
}

template <class TmpFront>
void SetupReport2Front(OmsRequestRunner& runner, TwfRptLineTmp& line, const TmpFront& pkr2) {
   runner.Request_->OrdNo_ = pkr2.OrderNo_;
   static_cast<TwfTradingLineMgr*>(&line.LineMgr_)->ExgOrdIdToReqUID(pkr2.OrdId_, *runner.Request_);
   if (fon9_LIKELY(pkr2.StatusCode_ != 0))
      runner.Request_->SetErrCode(static_cast<OmsErrCode>(pkr2.StatusCode_ + OmsErrCode_FromTwFEX));
}

// 「委託/成交」回報: R02/R32;
static void SetupReport2(TwfRptLineTmp& line, const f9twf::TmpR2Front& pkr2) {
   OmsRequestRunner runner{fon9::StrView{reinterpret_cast<const char*>(&pkr2), pkr2.GetPacketSize()}};
   OmsTwfSymbol*    rptSymbol = SetupReport2Back(runner, line, TmpExecTypeToRxKind(pkr2.ExecType_), *f9twf::TmpPtrAfterSym(&pkr2));
   SetupReport2Front(runner, line, pkr2);
   SetupReport0Symbol(*runner.Request_, pkr2.IvacFcmId_, &pkr2.SymbolType_, *rptSymbol, line);
   static_cast<TwfTradingLineMgr*>(&line.LineMgr_)->OmsCore()->MoveToCore(std::move(runner));
}
// 短格式委託/成交回報(R22) ApCode=6
static void SetupReport22(TwfRptLineTmp& line, const f9twf::TmpR22& pkr2) {
   OmsRequestRunner runner{fon9::StrView{reinterpret_cast<const char*>(&pkr2), pkr2.GetPacketSize()}};
   SetupReport2Back(runner, line, TmpExecTypeToRxKind(pkr2.ExecType_), pkr2);
   SetupReport2Front(runner, line, pkr2);
   SetupReport0Base(*runner.Request_, pkr2.IvacFcmId_, line.SystemType(), line.LineMgr_.ExgMapMgr_->Lock());
   static_cast<TwfTradingLineMgr*>(&line.LineMgr_)->OmsCore()->MoveToCore(std::move(runner));
}
//--------------------------------------------------------------------------//
static void SetupReport3(TwfRptLineTmp& line, const f9twf::TmpR03& pkr3) {
   OmsRequestRunner runner{fon9::StrView{reinterpret_cast<const char*>(&pkr3), pkr3.GetPacketSize()}};
   runner.Request_ = line.Worker_.Rpt3Factory_.MakeReportIn(TmpExecTypeToRxKind(pkr3.ExecType_), line.LastRxTime());
   assert(dynamic_cast<OmsTwfReport3*>(runner.Request_.get()) != nullptr);
   OmsTwfReport3&  rpt = *static_cast<OmsTwfReport3*>(runner.Request_.get());
   rpt.OrdNo_ = pkr3.OrderNo_;
   rpt.Side_ = TmpSideTo(pkr3.Side_);
   rpt.ExgTime_ = pkr3.MsgTime_.ToTimeStamp();
   rpt.SetReportSt(f9fmkt_TradingRequestSt_ExchangeRejected);
   rpt.SetErrCode(static_cast<OmsErrCode>(pkr3.StatusCode_ + OmsErrCode_FromTwFEX));
   SetupReport0Base(rpt, pkr3.IvacFcmId_, line.SystemType(), line.LineMgr_.ExgMapMgr_->Lock());
   static_cast<TwfTradingLineMgr*>(&line.LineMgr_)->ExgOrdIdToReqUID(pkr3.OrdId_, rpt);
   static_cast<TwfTradingLineMgr*>(&line.LineMgr_)->OmsCore()->MoveToCore(std::move(runner));
}
//--------------------------------------------------------------------------//
// 詢價輸入回報 (R08/R38)
static void SetupReport8(TwfRptLineTmp& line, const f9twf::TmpR8Front& pkr8) {
   OmsRequestRunner runner{fon9::StrView{reinterpret_cast<const char*>(&pkr8), pkr8.GetPacketSize()}};
   runner.Request_ = line.Worker_.Rpt8Factory_.MakeReportIn(f9fmkt_RxKind_RequestNew, line.LastRxTime());
   assert(dynamic_cast<OmsTwfReport8*>(runner.Request_.get()) != nullptr);
   OmsTwfReport8& rpt = *static_cast<OmsTwfReport8*>(runner.Request_.get());
   rpt.OrdNo_ = pkr8.OrderNo_;
   rpt.ExgTime_ = pkr8.MsgTime_.ToTimeStamp();
   rpt.SetReportSt(f9fmkt_TradingRequestSt_ExchangeAccepted);
   if (fon9_LIKELY(pkr8.StatusCode_ != 0)) {
      // 如果有失敗, 依然使用 R08/R38 回報.
      // - 「期交所TCPIP_TMP_v2.11.0.pdf」文件有誤, 文件說 R07/R37 失敗使用 R03, 但實際使用 R08/R38;
      // - 但因 StatusCode 不一定代表失敗, 例如: 248=已達設定之流量值80%; 委託依然成功.
      // - 所以不能設定 f9fmkt_TradingRequestSt_ExchangeRejected;
      // - 因此, 這裡提供 f9fmkt_TradingRequestSt_ExchangeAccepted 表示期交所已收到詢價,
      //   由使用者自行根據 OmsErrCode 判斷結果.
      // rpt.SetReportSt(f9fmkt_TradingRequestSt_ExchangeRejected);
      rpt.SetErrCode(static_cast<OmsErrCode>(pkr8.StatusCode_ + OmsErrCode_FromTwFEX));
   }
   SetupReport0Symbol(rpt, pkr8.IvacFcmId_, &pkr8.SymbolType_, rpt.Symbol_, line);
   static_cast<TwfTradingLineMgr*>(&line.LineMgr_)->ExgOrdIdToReqUID(pkr8.OrdId_, rpt);
   static_cast<TwfTradingLineMgr*>(&line.LineMgr_)->OmsCore()->MoveToCore(std::move(runner));
}
//--------------------------------------------------------------------------//
void TwfRptLineTmp::OnExgTmp_ApPacket(const f9twf::TmpHeader& pktmp) {
   fon9_WARN_DISABLE_SWITCH;
   switch (pktmp.MessageType_) {
   case f9twf::TmpMessageType_R(03):
      SetupReport3(*this, *static_cast<const f9twf::TmpR03*>(&pktmp));
      break;
   case f9twf::TmpMessageType_R(02):
   case f9twf::TmpMessageType_R(32):
      SetupReport2(*this, *static_cast<const f9twf::TmpR2Front*>(&pktmp));
      break;
   case f9twf::TmpMessageType_R(22):
      SetupReport22(*this, *static_cast<const f9twf::TmpR22*>(&pktmp));
      break;
   case f9twf::TmpMessageType_R(8):
   case f9twf::TmpMessageType_R(38):
      SetupReport8(*this, *static_cast<const f9twf::TmpR8Front*>(&pktmp));
      break;
   } // switch(pktmp.MessageType_)
   fon9_WARN_POP;
}

} // namespaces
