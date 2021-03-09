// \file f9omstws/OmsTwsTradingLineTmp2019.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsTradingLineTmp2019.hpp"
#include "f9omstws/OmsTwsTradingLineMgr.hpp"
#include "f9omstws/OmsTwsRequestTools.hpp"
#include "f9omstw/OmsCoreMgr.hpp"

namespace f9omstw {

//--------------------------------------------------------------------------//
TwsTradingLineTmpFactory2019::TwsTradingLineTmpFactory2019(
                                 OmsCoreMgr&          coreMgr,
                                 OmsTwsReportFactory& rptFactory,
                                 OmsTwsFilledFactory& filFactory,
                                 std::string          logPathFmt,
                                 Named&&              name)
   : base(std::move(logPathFmt), std::move(name))
   , TwsTradingLineFactoryBase(coreMgr, rptFactory, filFactory) {
}
fon9::TimeStamp TwsTradingLineTmpFactory2019::GetTDay() {
   return this->CoreMgr_.CurrentCoreTDay();
}
fon9::io::SessionSP TwsTradingLineTmpFactory2019::CreateLineTmp(
                        f9tws::ExgTradingLineMgr&    lineMgr,
                        const f9tws::ExgLineTmpArgs& args,
                        std::string&                 errReason,
                        std::string                  logFileName) {
   fon9_WARN_DISABLE_SWITCH;
   switch (args.ApCode_) {
   case f9tws::TwsApCode::Regular:
   case f9tws::TwsApCode::FixedPrice:
   case f9tws::TwsApCode::OddLot:
      return new TwsTradingLineTmp2019(this->RptFactory_, lineMgr, args, std::move(logFileName));
   default:
      errReason = "Unknown ApCode.";
      return nullptr;
   }
   fon9_WARN_POP;
}
//--------------------------------------------------------------------------//
TwsTradingLineTmp2019::TwsTradingLineTmp2019(OmsTwsReportFactory&         rptFactory,
                                             f9tws::ExgTradingLineMgr&    lineMgr,
                                             const f9tws::ExgLineTmpArgs& lineargs,
                                             std::string                  logFileName)
   : base(lineMgr, lineargs, std::move(logFileName))
   , TwsTradingLineBase{lineargs}
   , RptFactory_(rptFactory) {
}
void TwsTradingLineTmp2019::OnExgTmp_ApReady() {
   assert(this->SendingSNO_ == 0 && this->OmsCore_.get() == nullptr);
   this->LineMgr_.OnTradingLineReady(*this);
}
void TwsTradingLineTmp2019::OnExgTmp_ApBroken() {
   this->LineMgr_.OnTradingLineBroken(*this);
   this->ClearSending();
}
void TwsTradingLineTmp2019::ClearSending() {
   this->SendingSNO_ = 0;
   this->OmsCore_.reset();
}
//--------------------------------------------------------------------------//
/// T010, O010 前端共有的欄位.
struct ExgTmpTradeHead : public f9tws::ExgTmpHead {
   f9tws::BrkId      BrkId_;
   fon9::CharAryF<2> PvcID_;
   f9tws::OrdNo      OrdNo_;
   fon9::CharAryF<7> IvacNo_;
};
//--------------------------------------------------------------------------//
using Pri42 = fon9::CharAryF<6>;
using Qty3 = fon9::CharAryF<3>;
using Qty8 = fon9::CharAryF<8>;

struct ExgTmpT010 : public ExgTmpTradeHead {
   char                    IvacNoFlag_;
   f9tws::StkNo            StkNo_;
   Pri42                   Pri42_;
   Qty3                    Qty3_;
   f9fmkt_Side             BSCode_;
   f9tws::TwsExchangeCode  ExchangeCode_;
   OmsTwsOType             OType_;
};
static_assert(f9tws_kSIZE_TMP_HEAD_A + 37, "struct ExgTmpT010; must pack?");

/// T020: 下單回報.
struct ExgTmpT020 : public ExgTmpT010 {
   fon9::CharAryF<6> OrdDate_;
   fon9::CharAryF<6> OrdTime_;
   Qty3              BeforeQty_;
   Qty3              AfterQty_;
};
static_assert(sizeof(ExgTmpT010) + 18, "struct ExgTmpT020; must pack?");
//--------------------------------------------------------------------------//
struct ExgTmpO010 : public ExgTmpTradeHead {
   f9tws::StkNo            StkNo_;
   Pri42                   Pri42_;
   Qty8                    Qty8_;
   f9fmkt_Side             BSCode_;
   f9tws::TwsExchangeCode  ExchangeCode_;
};
static_assert(f9tws_kSIZE_TMP_HEAD_A + 38, "struct ExgTmpO010; must pack?");

struct ExgTmpO020 : public ExgTmpO010 {
   fon9::CharAryF<6>  OrdDate_;
   fon9::CharAryF<6>  OrdTime_;
   Qty8               BeforeQty_;
   Qty8               AfterQty_;
};
static_assert(f9tws_kSIZE_TMP_HEAD_A + 38, "struct ExgTmpO020; must pack?");
//--------------------------------------------------------------------------//
/// T010,O010 都有, 但位置不同的欄位.
template <class ExgTmpTradeT>
inline void PutExgTmpTradeCommon(ExgTmpTradeT& pktmp, const OmsTwsRequestIni& iniReq, const OmsTwsOrderRaw& ordraw) {
   pktmp.BSCode_ = iniReq.Side_;

   fon9::Pic9ToStrRev<pktmp.Pri42_.size()>(
      pktmp.Pri42_.end(), ordraw.LastPri_.GetOrigValue() * (100 / ordraw.LastPri_.Divisor));

   if (const char* peos = static_cast<const char*>(memchr(iniReq.Symbol_.Chars_, '\0', iniReq.Symbol_.size())))
      pktmp.StkNo_.CopyFrom(iniReq.Symbol_.Chars_, static_cast<size_t>(peos - iniReq.Symbol_.Chars_));
   else
      pktmp.StkNo_.CopyFrom(iniReq.Symbol_.Chars_, iniReq.Symbol_.size());
}
template <class ExgTmpTradeT>
inline void ExgTmpToReport(const ExgTmpTradeT& pktmp, OmsTwsReport& rpt) {
   // 指定回報, 只要填 beforeQty, afterQty 就足夠了!
   rpt.Qty_ = fon9::Pic9StrTo<pktmp.AfterQty_.size(), OmsTwsQty>(pktmp.AfterQty_.Chars_);
   rpt.BeforeQty_ = fon9::Pic9StrTo<pktmp.BeforeQty_.size(), OmsTwsQty>(pktmp.BeforeQty_.Chars_);
   rpt.ExgTime_ = fon9::StrTo(fon9::StrView_all(pktmp.OrdTime_.Chars_), fon9::DayTime::Null());
}
//--------------------------------------------------------------------------//
void TwsTradingLineTmp2019::OnExgTmp_ApPacket(const f9tws::ExgTmpHead& pktmp, unsigned pksz) {
   // B=>E 00 委託輸入訊息      this->SendRequest();
   // B<=E 01 委託回報訊息      this mf: isRptOK=true;
   // B=>E 02 確認連線訊息      ExgLineTmp::OnExgTmp_CheckApTimer();
   // B<=E 03 錯誤發生回覆訊息  this mf: isRptOK=false;
   // B=>E 04 重新連線查詢訊息
   // B<=E 05 確定連線回覆訊息  dont care.
   if (pktmp.MsgType_.Chars_[0] != '0')
      return;
   bool isRptOK;
   switch (pktmp.MsgType_.Chars_[1]) {
   case '1': // MsgType = "01" = 委託回報訊息.
      // 仍可能有 StCode:
      // 31 = QUANTITY WAS CUT     | 外資買進、借券賣出委託數量被刪減。IOC委託可成交部分之委託數量生效，剩餘委託數量剔退
      // 32 = DELETE OVER QUANTITY | 取消數量超過原有數量
      // 51 = 委託觸及價格穩定措施上、下限價格，部分委託被刪減。市價、IOC委託可成交部分之委託數量生效，剩餘委託數量剔退
      isRptOK = true;
      break;
   case '3': // MsgType = "03" = 錯誤發生回覆訊息.
      // 僅提供 pktmp.StCode_; 判斷錯誤原因.
      isRptOK = false;
      break;
   default:
      return;
   }
   if (this->SendingSNO_ == 0)
      return;
   OmsRequestRunner runner{fon9::StrView{reinterpret_cast<const char*>(&pktmp), pksz}};
   runner.Request_ = this->RptFactory_.MakeReportIn(f9fmkt_RxKind_Unknown, this->LastRxTime());
   assert(dynamic_cast<OmsTwsReport*>(runner.Request_.get()) != nullptr);
   OmsTwsReport&  rpt = *static_cast<OmsTwsReport*>(runner.Request_.get());
   OmsForceAssignReportReqUID(rpt, this->SendingSNO_);
   if (fon9_LIKELY(isRptOK)) {
      rpt.SetReportSt(f9fmkt_TradingRequestSt_ExchangeAccepted);
      if (fon9_UNLIKELY(this->LineArgs_.ApCode_ == f9tws::TwsApCode::OddLot))
         ExgTmpToReport(*static_cast<const ExgTmpO020*>(&pktmp), rpt);
      else
         ExgTmpToReport(*static_cast<const ExgTmpT020*>(&pktmp), rpt);
   }
   else {
      rpt.SetReportSt(f9fmkt_TradingRequestSt_ExchangeRejected);
      rpt.ExgTime_ = fon9::StrTo(fon9::StrView_all(pktmp.Time_.Chars_), fon9::DayTime::Null());
   }

   OmsErrCode ec = static_cast<OmsErrCode>(pktmp.GetStCode());
   if (fon9_UNLIKELY(ec != OmsErrCode_NoError)) {
      assert(fon9::isdigit(static_cast<unsigned char>(this->LineArgs_.ApCode_)));
      rpt.SetErrCode(static_cast<OmsErrCode>(ec
             + (static_cast<unsigned char>(this->LineArgs_.ApCode_) - '0') * 1000
             + (this->LineArgs_.Market_ == f9fmkt_TradingMarket_TwOTC
                ? OmsErrCode_FromTwOTC : OmsErrCode_FromTwSEC)));
   }

   // TODO: 是否可以在 OmsCore thread 執行完回報後,
   //       直接在 OmsCore thread 處理 this->LineMgr_.OnTradingLineReady();
   OmsCoreSP core{std::move(this->OmsCore_)};
   this->ClearSending();
   core->MoveToCore(std::move(runner));
   this->LineMgr_.OnTradingLineReady(*this);
}
//--------------------------------------------------------------------------//
TwsTradingLineTmp2019::SendResult TwsTradingLineTmp2019::SendRequest(f9fmkt::TradingRequest& req) {
   assert(dynamic_cast<OmsRequestTrade*>(&req) != nullptr);
   assert(dynamic_cast<TwsTradingLineMgr*>(&this->LineMgr_) != nullptr);

   const OmsRequestTrade*  curReq = static_cast<OmsRequestTrade*>(&req);
   fon9_WARN_DISABLE_SWITCH;
   switch (curReq->SessionId()) {
   case f9fmkt_TradingSessionId_Normal:
      if (this->LineArgs_.ApCode_ == f9tws::TwsApCode::Regular)
         break;
      return SendResult::NotSupport;
   case f9fmkt_TradingSessionId_FixedPrice:
      if (this->LineArgs_.ApCode_ == f9tws::TwsApCode::FixedPrice)
         break;
      return SendResult::NotSupport;
   case f9fmkt_TradingSessionId_OddLot:
      if (this->LineArgs_.ApCode_ == f9tws::TwsApCode::OddLot)
         break;
      return SendResult::NotSupport;
   default:
      return SendResult::NotSupport;
   }
   fon9_WARN_POP;
   if (this->SendingSNO_ > 0)
      return SendResult::Busy;

   fon9::DyObj<OmsRequestRunnerInCore> tmpRunner;
   OmsRequestRunnerInCore* runner = static_cast<TwsTradingLineMgr*>(&this->LineMgr_)
      ->MakeRunner(tmpRunner, *curReq, 256u);

   OmsTwsOrderRaw&         ordraw = *static_cast<OmsTwsOrderRaw*>(&runner->OrderRaw_);
   OmsOrder&               order = ordraw.Order();
   const OmsTwsRequestIni* iniReq = static_cast<const OmsTwsRequestIni*>(order.Initiator());
   assert(iniReq != nullptr);

   const bool isOddLot = (this->LineArgs_.ApCode_ == f9tws::TwsApCode::OddLot);
   const auto kPkSize = static_cast<fon9::BufferNodeSize>(isOddLot ? sizeof(ExgTmpO010) : sizeof(ExgTmpT010));
   f9tws::ExgTmpIoRevBuffer rbuf{kPkSize};
   char* pout = rbuf.AllocPrefix(kPkSize) - kPkSize;
   rbuf.SetPrefixUsed(pout);

   ExgTmpTradeHead*  pktmp = reinterpret_cast<ExgTmpTradeHead*>(pout);
   const auto*       lastOrd = static_cast<const OmsTwsOrderRaw*>(order.Tail());
   OmsTwsQty         qty = 0;
   fon9_WARN_DISABLE_SWITCH;
   switch (curReq->RxKind()) {
   case f9fmkt_RxKind_RequestNew:
      if (iniReq->Side_ == f9fmkt_Side_Buy)
         pktmp->SetFuncMsg("0100");
      else
         pktmp->SetFuncMsg("0200");
      qty = lastOrd->LeavesQty_;
      if (fon9_UNLIKELY(!static_cast<TwsTradingLineMgr*>(&this->LineMgr_)->AllocOrdNo(*runner)))
         return SendResult::RejectRequest;
      break;
   case f9fmkt_RxKind_RequestChgQty:
   {  // OrderQty = 整股:欲刪數量, 零股:剩餘數量.
      // 20200323: 零股也是「欲刪減量」, 但因格式有大幅調整, 所以不共用此檔.
      const bool isWantToKill = !isOddLot;
      qty = static_cast<OmsTwsQty>(GetTwsRequestChgQty(*runner, curReq, lastOrd, isWantToKill));
      if (fon9::signed_cast(qty) < 0)
         return SendResult::RejectRequest;
      if (qty > 0) {
         pktmp->SetFuncMsg("0300");
         break;
      }
   }
   /* fall through */ // qty == 0: 不用 break; 使用刪單功能.
   case f9fmkt_RxKind_RequestDelete:
      pktmp->SetFuncMsg("0400");
      break;
   case f9fmkt_RxKind_RequestQuery:
      pktmp->SetFuncMsg("0500");
      break;
   default:
      runner->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_RxKind, nullptr);
      return SendResult::RejectRequest;
   }
   fon9_WARN_POP;
   // TradeHead
   ordraw.OutPvcId_ = this->LineArgs_.SocketId_;
   pktmp->BrkId_.CopyFrom(iniReq->BrkId_.begin(), pktmp->BrkId_.size());
   pktmp->PvcID_ = this->LineArgs_.SocketId_;
   pktmp->OrdNo_ = *static_cast<const f9tws::OrdNo*>(&ordraw.OrdNo_);
   fon9::Pic9ToStrRev<pktmp->IvacNo_.size()>(pktmp->IvacNo_.end(), iniReq->IvacNo_);

   // TMP Head;
   const auto now = fon9::UtcNow();
   pktmp->SubsysName_.CopyFrom(this->ApCfmLink_.Chars_, pktmp->SubsysName_.size());
   pktmp->SetUtcTime(now);
   pktmp->SetStCode("00");

   if (fon9_LIKELY(!isOddLot)) {
      // T010(整股,定價)
      ExgTmpT010* pkt010 = static_cast<ExgTmpT010*>(pktmp);
      pkt010->ExchangeCode_ = f9tws::TwsExchangeCode::Regular;
      if ((pkt010->IvacNoFlag_ = iniReq->IvacNoFlag_.Chars_[0]) == '\0')
         pkt010->IvacNoFlag_ = ' ';
      pkt010->OType_ = ordraw.OType_;
      if (fon9_LIKELY(qty)) {
         qty /= f9fmkt::GetSymbTwsShUnit(order.GetSymb(runner->Resource_, iniReq->Symbol_));
         fon9::Pic9ToStrRev<pkt010->Qty3_.size()>(pkt010->Qty3_.end(), qty);
      }
      else
         pkt010->Qty3_.CopyFrom("000", 3);
      PutExgTmpTradeCommon(*pkt010, *iniReq, ordraw);
   }
   else {
      // O010(零股)
      ExgTmpO010* pko010 = static_cast<ExgTmpO010*>(pktmp);
      pko010->ExchangeCode_ = f9tws::TwsExchangeCode::OddLot;
      fon9::Pic9ToStrRev<pko010->Qty8_.size()>(pko010->Qty8_.end(), qty);
      PutExgTmpTradeCommon(*pko010, *iniReq, ordraw);
   }
   this->SendTmp(now, std::move(rbuf));
   this->SendingSNO_ = curReq->RxSNO();
   this->OmsCore_.reset(&runner->Resource_.Core_);
   runner->Update(f9fmkt_TradingRequestSt_Sending, ToStrView(this->StrSendingBy_));
   return SendResult::Sent;
}

} // namespaces
