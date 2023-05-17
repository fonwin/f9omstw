// \file f9omstwf/OmsTwfTradingLineTmp.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfTradingLineTmp.hpp"
#include "f9omstwf/OmsTwfTradingLineMgr.hpp"
#include "f9omstwf/OmsTwfOrder1.hpp"
#include "f9omstwf/OmsTwfOrder9.hpp"
#include "f9twf/ExgTmpTradingR1.hpp"
#include "f9twf/ExgTmpTradingR7.hpp"
#include "f9twf/ExgTmpTradingR9.hpp"

namespace f9omstw {

TwfTradingLineTmp::TwfTradingLineTmp(TwfLineTmpWorker&            worker,
                                     f9twf::ExgTradingLineMgr&    lineMgr,
                                     const f9twf::ExgLineTmpArgs& lineArgs,
                                     f9twf::ExgLineTmpLog&&       log)
   : base(worker, lineMgr, lineArgs, std::move(log), true)
   , baseTradingLine{lineArgs.LineFc_}
   , StrSendingBy_{fon9::RevPrintTo<fon9::CharVector>(
      "Sending.", f9twf::TmpGetValueU(lineArgs.SessionFcmId_),
      '.', f9twf::TmpGetValueU(lineArgs.SessionId_))} {
}
void TwfTradingLineTmp::GetApReadyInfo(fon9::RevBufferList& rbuf) {
   if (fon9::fmkt::gTradingLineSelect_TryLastLine_YN == fon9::EnabledYN::Yes) {
      fon9::RevPrint(rbuf, "|LineFc=", this->LineFlowCount(), '/', this->LineFlowInterval());
   }
   fon9::RevPrint(rbuf, "|Fc=", this->MaxFlowCtrlCnt(), '/', this->LineArgs_.GetFcInterval());
}
void TwfTradingLineTmp::OnExgTmp_ApReady() {
   if (auto omsCore = static_cast<TwfTradingLineMgr*>(&this->LineMgr_)->OmsCore()) {
      fon9::intrusive_ptr<TwfTradingLineTmp> pthis{this};
      fon9::CharVector info = fon9::RevPrintTo<fon9::CharVector>(
         "LineReady.", f9twf::TmpGetValueU(this->LineArgs_.SessionFcmId_),
         '.', this->OutPvcId_);
      omsCore->SetSendingRequestFail(ToStrView(info), [pthis](const OmsOrderRaw& ordraw) {
         if (auto twford = dynamic_cast<const OmsTwfOrderRaw0*>(&ordraw))
            return (pthis->OutPvcId_ == twford->OutPvcId_);
         return false;
      });
   }
   this->Fc_.Resize(this->MaxFlowCtrlCnt(), this->LineArgs_.GetFcInterval());
   this->LineMgr_.OnTradingLineReady(*this);
}
void TwfTradingLineTmp::OnExgTmp_ApBroken() {
   this->LineMgr_.OnTradingLineBroken(*this);
}
//--------------------------------------------------------------------------//
bool TwfTradingLineTmp::IsOrigSender(const f9fmkt::TradingRequest& req) const {
   assert(dynamic_cast<const OmsRequestTrade*>(&req) != nullptr);
   if (const OmsOrderRaw* ordraw = static_cast<const OmsRequestTrade*>(&req)->LastUpdated()) {
      assert(dynamic_cast<const OmsTwfOrderRaw0*>(ordraw) != nullptr);
      return(static_cast<const OmsTwfOrderRaw0*>(ordraw)->OutPvcId_ == this->OutPvcId_);
   }
   return false;
}
TwfTradingLineTmp::SendResult TwfTradingLineTmp::SendRequest(f9fmkt::TradingRequest& srcReq) {
   assert(dynamic_cast<OmsRequestTrade*>(&srcReq) != nullptr);
   assert(dynamic_cast<TwfTradingLineMgr*>(&this->LineMgr_) != nullptr);

   fon9::TimeStamp   now = fon9::UtcNow();
   if (fon9_UNLIKELY(IsOverVaTimeMS(srcReq, now))) {
      fon9::DyObj<OmsInternalRunnerInCore> tmpRunner;
      OmsRequestRunnerInCore* runner = static_cast<TwfTradingLineMgr*>(&this->LineMgr_)
         ->MakeRunner(tmpRunner, *static_cast<OmsRequestTrade*>(&srcReq), 0u);
      runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_OverVaTimeMS, nullptr);
      return SendResult::RejectRequest;
   }

   fon9::TimeInterval fc = this->Fc_.Check(now);
   if (fc.GetOrigValue() > 0)
      return f9fmkt::ToFlowControlResult(fc);

   fon9::DyObj<OmsInternalRunnerInCore> tmpRunner;
   TwfTradingLineMgr&      lineMgr = *static_cast<TwfTradingLineMgr*>(&this->LineMgr_);
   const OmsRequestTrade*  curReq  = static_cast<OmsRequestTrade*>(&srcReq);
   OmsRequestRunnerInCore* runner  = lineMgr.MakeRunner(tmpRunner, *curReq, 256u);
   auto&                   ordraw  = runner->OrderRaw();
   OmsOrder&               order   = ordraw.Order();

   while (fon9_UNLIKELY(curReq->RxKind() == f9fmkt_RxKind_RequestRerun)) {
      if (auto* upd = dynamic_cast<const OmsRequestUpd*>(curReq)) {
         if ((curReq = dynamic_cast<const OmsRequestTrade*>(runner->Resource_.Backend_.GetItem(upd->IniSNO()))) != nullptr)
            break;
      }
      // 若沒有設計錯誤, 不可能來到此處!
      // 因為, 來到此處的基本條件:
      // - 必定使用 OmsRequestUpd 來處理 Rerun;
      // - 且 upd->IniSNO() 必定有找到需要 Rerun 的下單要求;
      // - 所以這裡直接使用 OmsErrCode_OrderNotFound; 不再另外定義 OmsErrCode_RequestNotFound;
      runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_OrderNotFound, nullptr);
      return SendResult::RejectRequest;
   }

   // 一般下單 / 詢價要求 / 報價(Bid/Offer)?
   assert(dynamic_cast<const OmsTwfRequestIni0*>(order.Initiator()) != nullptr);
   const OmsTwfRequestIni0* iniReq0  = static_cast<const OmsTwfRequestIni0*>(order.Initiator());
   // 因為需要知道 symb->PriceOrigDiv_ 才能正確的填價格欄位;
   // 所以如果沒有 symb, 就無法下單.
   OmsSymb* symb = order.GetSymb(runner->Resource_, iniReq0->Symbol_);
   if (fon9_UNLIKELY(symb == nullptr)) {
      runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_SymbNotFound, nullptr);
      return SendResult::RejectRequest;
   }
   if (fon9_UNLIKELY(symb->PriceOrigDiv_ <= 0)) {
      runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_SymbDecimalLocator, nullptr);
      return SendResult::RejectRequest;
   }
   OmsSymb*                   symbLeg2 = iniReq0->UnsafeSymbLeg2();
   const f9twf::TmpSymbolType symType = this->LineArgs_.IsUseSymNum_
      ? f9twf::TmpSymbolType::ShortNum
      : (iniReq0->Market() == f9fmkt_TradingMarket_TwFUT
         ? f9twf::FutCheckSymbolType(iniReq0->Symbol_, symbLeg2 != nullptr)
         : f9twf::OptCheckSymbolType(iniReq0->Symbol_, symbLeg2 != nullptr));
          
   // 必須要知道投資人的券商代號(FcmId,CmId);
   OmsBrk* brk = order.GetBrk(runner->Resource_);
   assert(brk != nullptr);

   f9twf::ExgLineTmpRevBuffer buf;
   f9twf::TmpR1BfSym*         pkr1bf{nullptr};
   f9twf::TmpR1Back*          pkr1back{nullptr};
   f9twf::TmpR9Back*          pkr9back{nullptr};
   f9twf::TmpSymNum*          pkSym;
   bool                       isNeedMsgSeqNum = true;
   switch (iniReq0->RequestType_) {
   case RequestType::Normal:
      if (TmpSymbolTypeIsLong(symType)) {
         f9twf::TmpR31& r31 = buf.Alloc<f9twf::TmpR31>();
         f9twf::TmpInitializeSkipSeqNum(r31, f9twf::TmpMessageType_R(31));
         pkr1bf = &r31;
         pkr1back = &r31;
         pkSym = &r31.Sym_.Num_;
      }
      else {
         f9twf::TmpR01& r01 = buf.Alloc<f9twf::TmpR01>();
         f9twf::TmpInitializeSkipSeqNum(r01, f9twf::TmpMessageType_R(01));
         pkr1bf = &r01;
         pkr1back = &r01;
         pkSym = &r01.Sym_.Num_;
      }
      break;
   case RequestType::QuoteR:
      isNeedMsgSeqNum = false;
      if (fon9_LIKELY(lineMgr.AllocOrdNo(*runner, *iniReq0))) {
         f9twf::TmpR7BfSym* pkr7bf;
         if (TmpSymbolTypeIsLong(symType)) {
            f9twf::TmpR37& r37 = buf.Alloc<f9twf::TmpR37>();
            f9twf::TmpInitializeSkipSeqNum(r37, f9twf::TmpMessageType_R(37));
            pkr7bf = &r37;
            pkSym = &r37.Sym_.Num_;
            r37.Source_ = iniReq0->TmpSource_;
         }
         else {
            f9twf::TmpR07& r07 = buf.Alloc<f9twf::TmpR07>();
            f9twf::TmpInitializeSkipSeqNum(r07, f9twf::TmpMessageType_R(7));
            pkr7bf = &r07;
            pkSym = &r07.Sym_.Num_;
            r07.Source_ = iniReq0->TmpSource_;
         }
         pkr7bf->OrderNo_ = ordraw.OrdNo_;
         f9twf::TmpPutValue(pkr7bf->OrdId_, static_cast<uint32_t>(curReq->RxSNO()));
         f9twf::TmpPutValue(pkr7bf->IvacFcmId_, brk->FcmId_);
         pkr7bf->SymbolType_ = symType;
         break;
      }
      return SendResult::RejectRequest;
   case RequestType::Quote:
      if (TmpSymbolTypeIsLong(symType)) {
         f9twf::TmpR39& r39 = buf.Alloc<f9twf::TmpR39>();
         f9twf::TmpInitializeSkipSeqNum(r39, f9twf::TmpMessageType_R(39));
         pkr1bf = &r39;
         pkr9back = &r39;
         pkSym = &r39.Sym_.Num_;
      }
      else {
         f9twf::TmpR09& r09 = buf.Alloc<f9twf::TmpR09>();
         f9twf::TmpInitializeSkipSeqNum(r09, f9twf::TmpMessageType_R(9));
         pkr1bf = &r09;
         pkr9back = &r09;
         pkSym = &r09.Sym_.Num_;
      }
      break;
   default:
      runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_UnknownRequestType, nullptr);
      return SendResult::RejectRequest;
   }
   // ====================================
   if (fon9_UNLIKELY(this->LineArgs_.IsUseSymNum_)) {
      memset(pkSym, 0, sizeof(f9twf::TmpSymTextS));
      f9twf::TmpPutValue(pkSym->Pseq1_, symb->ExgSymbSeq_);
      if (fon9_UNLIKELY(symbLeg2 != nullptr)) {
         f9twf::TmpPutValue(pkSym->Pseq2_, symbLeg2->ExgSymbSeq_);
         pkSym->CombOp_ = iniReq0->UnsafeCombOp();
      }
   }
   else {
      if (fon9_LIKELY(TmpSymbolTypeIsShort(symType)))
         memset(pkSym, ' ', sizeof(f9twf::TmpSymTextS));
      else {
         assert(TmpSymbolTypeIsLong(symType));
         memset(pkSym, ' ', sizeof(f9twf::TmpSymTextL));
      }
      memcpy(pkSym, iniReq0->Symbol_.begin(), iniReq0->Symbol_.length());
   }
   // ====================================
   if (fon9_LIKELY(pkr1back)) { // R01/R31
      assert(pkr1bf != nullptr);
      assert(dynamic_cast<const OmsTwfRequestIni1*>(iniReq0) != nullptr);
      assert(dynamic_cast<const OmsTwfOrderRaw1*>(order.Tail()) != nullptr);
      const OmsTwfRequestIni1* iniReq1 = static_cast<const OmsTwfRequestIni1*>(iniReq0);
      switch (iniReq1->Side_) {
      case f9fmkt_Side_Buy:
         pkr1back->Side_ = f9twf::TmpSide::Buy;
         break;
      case f9fmkt_Side_Sell:
         pkr1back->Side_ = f9twf::TmpSide::Sell;
         break;
      case f9fmkt_Side_Unknown:
      default:
         runner->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_Side, nullptr);
         return SendResult::RejectRequest;
      }
      if (fon9_UNLIKELY(this->LineArgs_.IsUseSymNum_ && symbLeg2 != nullptr)) {
         switch (iniReq0->UnsafeCombSide()) {
         case f9twf::ExgCombSide::None:
            break;
         case f9twf::ExgCombSide::SameSide:
            pkSym->LegSide_[0] = pkSym->LegSide_[1] = pkr1back->Side_;
            break;
         case f9twf::ExgCombSide::SideIsLeg1:
            pkSym->LegSide_[0] = pkr1back->Side_;
            pkSym->LegSide_[1] = static_cast<f9twf::TmpSide>(static_cast<uint8_t>(pkr1back->Side_) ^ 0x03);
            break;
         case f9twf::ExgCombSide::SideIsLeg2:
            pkSym->LegSide_[1] = pkr1back->Side_;
            pkSym->LegSide_[0] = static_cast<f9twf::TmpSide>(static_cast<uint8_t>(pkr1back->Side_) ^ 0x03);
            break;
         }
      }
      const auto*          lastOrd = static_cast<const OmsTwfOrderRaw1*>(order.Tail());
      OmsTwfQty            qty{};
      OmsTwfPri            pri = lastOrd->LastPri_;
      f9fmkt_PriType       priType = lastOrd->LastPriType_;
      f9fmkt_TimeInForce   tif = lastOrd->LastTimeInForce_;
      fon9_WARN_DISABLE_SWITCH;
      switch (curReq->RxKind()) {
      case f9fmkt_RxKind_RequestNew:
         pkr1bf->ExecType_ = f9twf::TmpExecType::New;
         if (fon9_UNLIKELY(!lineMgr.AllocOrdNo(*runner, *iniReq0)))
            return SendResult::RejectRequest;
         qty = lastOrd->LeavesQty_;
         break;
      case f9fmkt_RxKind_RequestChgQty:
         if (auto* chgQtyReq = dynamic_cast<const OmsTwfRequestChg1*>(curReq))
            qty = runner->GetRequestChgQty(chgQtyReq->Qty_, true/* isWantToKill */, lastOrd->LeavesQty_, lastOrd->CumQty_);
         else if (auto* chgQtyIniReq = dynamic_cast<const OmsTwfRequestIni1*>(curReq))
            // 使用 OmsTwsRequestIni1 改量, 則直接把 chgQtyIniReq->Qty_ 送交易所.
            qty = chgQtyIniReq->Qty_;
         else {
            runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_UnknownRequestType, nullptr);
            return SendResult::RejectRequest;
         }
         if (fon9_UNLIKELY(fon9::signed_cast(qty) < 0)) {
            runner->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_ChgQty, nullptr);
            return SendResult::RejectRequest;
         }
         if (fon9_LIKELY(qty > 0)) {
            pkr1bf->ExecType_ = f9twf::TmpExecType::ChgQty;
            break;
         }
         /* fall through */ // assert(qty == 0); 不用 break; 使用刪單功能.
      case f9fmkt_RxKind_RequestDelete:
         pkr1bf->ExecType_ = f9twf::TmpExecType::Delete;
         break;
      case f9fmkt_RxKind_RequestQuery:
         isNeedMsgSeqNum = false;
         pkr1bf->ExecType_ = f9twf::TmpExecType::Query;
         break;
      case f9fmkt_RxKind_RequestChgPri: // 報價: 沒有改價功能.
         pkr1bf->ExecType_ = f9twf::TmpExecType::ChgPrim; // 改價 & 不改成交回報線路.
         if (auto* chgPriReq = dynamic_cast<const OmsTwfRequestChg1*>(curReq)) {
            tif = chgPriReq->TimeInForce_;
            priType = chgPriReq->PriType_;
            pri = chgPriReq->Pri_;
         }
         else if (auto* chgPriIniReq = dynamic_cast<const OmsTwfRequestIni1*>(curReq)) {
            tif = chgPriIniReq->TimeInForce_;
            priType = chgPriIniReq->PriType_;
            pri = chgPriIniReq->Pri_;
         }
         else {
            runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_UnknownRequestType, nullptr);
            return SendResult::RejectRequest;
         }
         break;
      default:
         runner->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_RxKind, nullptr);
         return SendResult::RejectRequest;
      }
      switch (priType) {
      case f9fmkt_PriType_Unknown:
      case f9fmkt_PriType_Limit:
         pkr1back->PriType_ = f9twf::TmpPriType::Limit;
         if ((pri.GetOrigValue() % symb->PriceOrigDiv_) != 0) {
            // 下單要求的 pri 有誤, 或 symb->PriceScale_ 有誤;
            runner->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_Pri, nullptr);
            return SendResult::RejectRequest;
         }
         f9twf::TmpPutValue(pkr1back->Price_, static_cast<f9twf::TmpPrice_t>(pri.GetOrigValue() / symb->PriceOrigDiv_));
         break;
      case f9fmkt_PriType_Market:
         pkr1back->PriType_ = f9twf::TmpPriType::Market;
         pkr1back->Price_.Clear();
         break;
      case f9fmkt_PriType_Mwp:
         pkr1back->PriType_ = f9twf::TmpPriType::Mwp;
         pkr1back->Price_.Clear();
         break;
      default:
         pkr1back->PriType_ = f9twf::TmpPriType{};
         pkr1back->Price_.Clear();
         break;
      }
      switch (tif) {
      case f9fmkt_TimeInForce_ROD:
         pkr1back->TimeInForce_ = f9twf::TmpTimeInForce::ROD;
         break;
      case f9fmkt_TimeInForce_IOC:
         pkr1back->TimeInForce_ = f9twf::TmpTimeInForce::IOC;
         break;
      case f9fmkt_TimeInForce_FOK:
         pkr1back->TimeInForce_ = f9twf::TmpTimeInForce::FOK;
         break;
      case f9fmkt_TimeInForce_QuoteAutoCancel:
         pkr1back->TimeInForce_ = f9twf::TmpTimeInForce::QuoteAutoCancel;
         break;
      default:
         pkr1back->TimeInForce_ = f9twf::TmpTimeInForce{};
         break;
      }
      fon9_WARN_POP;
      f9twf::TmpPutValue(pkr1back->Qty_, qty);
      f9twf::TmpPutValue(pkr1back->IvacNo_, iniReq1->IvacNo_);
      pkr1back->IvacFlag_ = iniReq1->IvacNoFlag_;
      if (fon9_UNLIKELY((pkr1back->PosEff_ = lastOrd->PosEff_) == OmsTwfPosEff{}))
         pkr1back->PosEff_ = iniReq1->PosEff_;
      pkr1back->Source_ = iniReq1->TmpSource_;
   }
   // ------------------------------------
   else if (fon9_LIKELY(pkr9back)) { // R09/R39
      assert(pkr1bf != nullptr);
      assert(dynamic_cast<const OmsTwfRequestIni9*>(iniReq0) != nullptr);
      assert(dynamic_cast<const OmsTwfOrderRaw9*>(order.Tail()) != nullptr);
      const OmsTwfRequestIni9* iniReq9 = static_cast<const OmsTwfRequestIni9*>(iniReq0);
      const auto*              lastOrd = static_cast<const OmsTwfOrderRaw9*>(order.Tail());
      OmsTwfQty                bidQty{}, offerQty{};
      OmsTwfPri                bidPri, offerPri;
      pkr9back->TimeInForce_ = f9twf::TmpTimeInForce{};
      fon9_WARN_DISABLE_SWITCH;
      switch (curReq->RxKind()) {
      case f9fmkt_RxKind_RequestChgQty:
         if (auto* chgQtyReq = dynamic_cast<const OmsTwfRequestChg9*>(curReq)) {
            bidQty = runner->GetRequestChgQty(chgQtyReq->BidQty_, true/* isWantToKill */, lastOrd->Bid_.LeavesQty_, lastOrd->Bid_.CumQty_);
            offerQty = runner->GetRequestChgQty(chgQtyReq->OfferQty_, true/* isWantToKill */, lastOrd->Offer_.LeavesQty_, lastOrd->Offer_.CumQty_);
         }
         else if (auto* chgQtyIniReq = dynamic_cast<const OmsTwfRequestIni9*>(curReq)) {
            bidQty = chgQtyIniReq->BidQty_;
            offerQty = chgQtyIniReq->OfferQty_;
         }
         else {
            runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_UnknownRequestType, nullptr);
            return SendResult::RejectRequest;
         }
         if (fon9_UNLIKELY(fon9::signed_cast(bidQty) < 0 || fon9::signed_cast(offerQty) < 0)) {
            runner->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_ChgQty, nullptr);
            return SendResult::RejectRequest;
         }
         if (fon9_LIKELY(bidQty > 0 || offerQty > 0)) {
            pkr1bf->ExecType_ = f9twf::TmpExecType::ChgQty;
            break;
         }
         /* fall through */ // assert(bidQty == 0 && offerQty == 0); 不用 break; 使用刪單功能.
      case f9fmkt_RxKind_RequestDelete:
         pkr1bf->ExecType_ = f9twf::TmpExecType::Delete;
         break;
      case f9fmkt_RxKind_RequestQuery:
         pkr1bf->ExecType_ = f9twf::TmpExecType::Query;
         break;
      case f9fmkt_RxKind_RequestChgPri: // 報價單邊改價.
         if (auto* chgPriReq9 = dynamic_cast<const OmsTwfRequestChg9*>(curReq)) {
            pkr1bf->ExecType_ = f9twf::TmpExecType::ChgPrim; // 改價 & 不改成交回報線路.
            bidPri = chgPriReq9->BidPri_;
            offerPri = chgPriReq9->OfferPri_;
         }
         else if (auto* chgPriIniReq9 = dynamic_cast<const OmsTwfRequestIni9*>(curReq)) {
            bidPri = chgPriIniReq9->BidPri_;
            offerPri = chgPriIniReq9->OfferPri_;
         }
         else {
            runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_UnknownRequestType, nullptr);
            return SendResult::RejectRequest;
         }
         if (bidPri.IsNull())
            bidPri.Assign0();
         if (offerPri.IsNull())
            offerPri.Assign0();
         break;
      default:
         runner->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_RxKind, nullptr);
         return SendResult::RejectRequest;
      case f9fmkt_RxKind_RequestNew:
         switch (iniReq9->TimeInForce_) {
         case f9fmkt_TimeInForce_ROD:
            pkr9back->TimeInForce_ = f9twf::TmpTimeInForce::ROD;
            break;
         case f9fmkt_TimeInForce_QuoteAutoCancel:
            pkr9back->TimeInForce_ = f9twf::TmpTimeInForce::QuoteAutoCancel;
            break;
         default:
            runner->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_TimeInForce, nullptr);
            break;
         }
         pkr1bf->ExecType_ = f9twf::TmpExecType::New;
         if (fon9_UNLIKELY(!lineMgr.AllocOrdNo(*runner, *iniReq9)))
            return SendResult::RejectRequest;
         bidQty   = lastOrd->Bid_.LeavesQty_;
         offerQty = lastOrd->Offer_.LeavesQty_;
         bidPri   = iniReq9->BidPri_;
         offerPri = iniReq9->OfferPri_;
         break;
      }
      fon9_WARN_POP;
      if ((bidPri.GetOrigValue() % symb->PriceOrigDiv_) != 0 || (offerPri.GetOrigValue() % symb->PriceOrigDiv_) != 0) {
         runner->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_Pri, nullptr);
         return SendResult::RejectRequest;
      }
      f9twf::TmpPutValue(pkr9back->BidPx_,   static_cast<f9twf::TmpPrice_t>(bidPri.GetOrigValue() / symb->PriceOrigDiv_));
      f9twf::TmpPutValue(pkr9back->OfferPx_, static_cast<f9twf::TmpPrice_t>(offerPri.GetOrigValue() / symb->PriceOrigDiv_));

      f9twf::TmpPutValue(pkr9back->BidSize_, bidQty);
      f9twf::TmpPutValue(pkr9back->OfferSize_, offerQty);
      f9twf::TmpPutValue(pkr9back->IvacNo_, iniReq9->IvacNo_);
      pkr9back->IvacFlag_ = iniReq9->IvacNoFlag_;
      pkr9back->Source_ = iniReq9->TmpSource_;
      pkr9back->PosEff_ = f9twf::ExgPosEff::Quote;
   }
   // ====================================
   if (fon9_LIKELY(pkr1bf)) { // R01/R31/R09/R39
      pkr1bf->OrderNo_ = ordraw.OrdNo_;
      f9twf::TmpPutValue(pkr1bf->OrdId_, static_cast<uint32_t>(curReq->RxSNO()));
      f9twf::TmpPutValue(pkr1bf->CmId_, brk->CmId_);
      f9twf::TmpPutValue(pkr1bf->IvacFcmId_, brk->FcmId_);
      pkr1bf->SymbolType_ = symType;
      // UserDefine_: UserId? SalesNo? SubacNo? or ...
      // pkr1bf->UserDefine_.AssignFrom(ToStrView(curReq->UserId_));
      pkr1bf->UserDefine_.AssignFrom(ToStrView(iniReq0->SubacNo_));
   }
   // ====================================
   // this 已經有自己的 log, 為了速度的考量, 不再使用 runner->ExLogForUpd_; 記錄送出的封包.
   // 詢價、查詢 SeqNum 為 0;
   if (fon9_LIKELY(isNeedMsgSeqNum))
      this->SendTmpAddSeqNum(now, std::move(buf));
   else
      this->SendTmpSeqNum0(now, std::move(buf));
   static_cast<OmsTwfOrderRaw0*>(&ordraw)->OutPvcId_ = this->OutPvcId_;
   runner->Update(f9fmkt_TradingRequestSt_Sending, ToStrView(this->StrSendingBy_));
   this->Fc_.ForceUsed(now);
   return SendResult::Sent;
}

} // namespaces
