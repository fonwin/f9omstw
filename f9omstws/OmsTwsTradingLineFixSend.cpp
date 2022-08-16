// \file f9omstws/OmsTwsTradingLineFixSend.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsTradingLineFix.hpp"
#include "f9omstws/OmsTwsTradingLineMgr.hpp"
#include "f9omstws/OmsTwsRequestTools.hpp"
#include "fon9/fix/FixApDef.hpp"

namespace f9omstw {

// ----------
static inline char* RevCopyFill(char* pout, const size_t szFixed, fon9::StrView val) {
   memset(pout -= szFixed, ' ', szFixed);
   const size_t szVal = val.size();
   memcpy(pout, val.begin(), szVal <= szFixed ? szVal : szFixed);
   return pout;
}
//template <size_t kDstSize, size_t kSrcSize>
//static inline char* RevCopyFill(char* pout, const char* src) {
//   static_assert(kDstSize <= kSrcSize, "kDstSize must small or equal then kSrcSize");
//   if (void* pNul = memchr(memcpy(pout - kDstSize, src, kDstSize), '\0', kDstSize))
//      memset(pNul, ' ', static_cast<size_t>(pout - static_cast<char*>(pNul)));
//   return pout - kDstSize;
//}
//template <size_t kDstSize, size_t kSrcSize>
//static inline char* RevCopyFill(char* pout, const fon9::CharAry<kSrcSize>& src) {
//   return RevCopyFill<kDstSize, kSrcSize>(pout, src.begin());
//}
// ----------
static inline char* RevPutStr(char* pout, const void* pstr, size_t sz) {
   return reinterpret_cast<char*>(memcpy(pout -= sz, pstr, sz));
}
static inline char* RevPutStr(char* pout, fon9::StrView val) {
   return RevPutStr(pout, val.begin(), val.size());
}
// ----------
bool TwsTradingLineFix::IsOrigSender(const f9fmkt::TradingRequest& req) const {
   assert(dynamic_cast<const OmsRequestTrade*>(&req) != nullptr);
   if (const OmsOrderRaw* ordraw = static_cast<const OmsRequestTrade*>(&req)->LastUpdated()) {
      assert(dynamic_cast<const OmsTwsOrderRaw*>(ordraw) != nullptr);
      return(static_cast<const OmsTwsOrderRaw*>(ordraw)->OutPvcId_ == this->LineArgs_.SocketId_);
   }
   return false;
}
TwsTradingLineFix::SendResult TwsTradingLineFix::SendRequest(f9fmkt::TradingRequest& req) {
   assert(dynamic_cast<OmsRequestTrade*>(&req) != nullptr);
   assert(dynamic_cast<TwsTradingLineMgr*>(&this->FixManager_) != nullptr);

   const auto now = fon9::UtcNow();
   if (fon9_UNLIKELY(IsOverVaTimeMS(req, now))) {
      fon9::DyObj<OmsRequestRunnerInCore> tmpRunner;
      OmsRequestRunnerInCore* runner = static_cast<TwsTradingLineMgr*>(&this->FixManager_)
         ->MakeRunner(tmpRunner, *static_cast<OmsRequestTrade*>(&req), 0u);
      runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_OverVaTimeMS, nullptr);
      return SendResult::RejectRequest;
   }

   const auto fcInterval = this->FlowCounter_.Check(now);
   if (fon9_UNLIKELY(fcInterval.GetOrigValue() > 0))
      return f9fmkt::ToFlowControlResult(fcInterval);

   fon9::DyObj<OmsRequestRunnerInCore> tmpRunner;
   const OmsRequestTrade*  curReq = static_cast<OmsRequestTrade*>(&req);
   OmsRequestRunnerInCore* runner = static_cast<TwsTradingLineMgr*>(&this->FixManager_)
      ->MakeRunner(tmpRunner, *curReq, 256u);

   f9tws::TwsApCode  twsApCode;
   fon9_WARN_DISABLE_SWITCH;
   switch (req.SessionId()) {
   case f9fmkt_TradingSessionId_Normal:      twsApCode = f9tws::TwsApCode::Regular;    break;
   case f9fmkt_TradingSessionId_FixedPrice:  twsApCode = f9tws::TwsApCode::FixedPrice; break;
   case f9fmkt_TradingSessionId_OddLot:
      twsApCode = (static_cast<TwsTradingLineMgr*>(&this->FixManager_)->IsNormalTrading()
                   ? f9tws::TwsApCode::OddLotC : f9tws::TwsApCode::OddLot2);
      break;
   default:
      runner->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_SessionId, nullptr);
      return SendResult::RejectRequest;
   }
   fon9_WARN_POP;

   auto& ordraw = runner->OrderRawT<OmsTwsOrderRaw>();
   auto& order  = ordraw.Order();
   auto* iniReq = static_cast<const OmsTwsRequestIni*>(order.Initiator());
   assert(iniReq != nullptr);

   f9fix::FixBuilder fixb;
   //                   NewSingle   Replace  Cancel   Status
   // OrigClOrdID                      v        v
   // ClOrdID              v           v        v        v
   // OrderID              v           v        v        v
   // Symbol               v           v        v        v
   // Side                 v           v        v        v
   // Account              v           v        v
   // OrderQty             v           v
   // OrdType              v           v
   // TimeInForce          v            
   // TwseOrdType          v           v
   // Price                v           v
   // TwseRejStaleOrd      v           v        v
   // TwseExCode           v           v        v        v
   // TwseIvacnoFlag       v           v        v        v
   // TransactTime         v           v        v

   // "|TransactTime=yyyymmdd-hh:mm:ss.mmm"
   fixb.PutUtcTime(now);
   char* pout = RevPutStr(fixb.GetBuffer().AllocPrefix(256), f9fix_SPLTAGEQ(TransactTime));

   switch (iniReq->Side_) {
   case f9fmkt_Side_Buy:   pout = RevPutStr(pout, f9fix_SPLTAGEQ(Side) f9fix_kVAL_Side_Buy);    break;
   case f9fmkt_Side_Sell:  pout = RevPutStr(pout, f9fix_SPLTAGEQ(Side) f9fix_kVAL_Side_Sell);   break;
   case f9fmkt_Side_Unknown:
   default: // 買賣別應在風控時檢查, 但如果有疏漏, 這裡不可有預設值, 所以就拒絕吧!
      runner->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_Side, nullptr);
      return SendResult::RejectRequest;
   }

   pout = (req.SessionId() == f9fmkt_TradingSessionId_OddLot
           ? RevPutStr(pout, f9fix_SPLTAGEQ(TwseExCode) "2")   // 2: 盤後零股、盤中零股.
           : RevPutStr(pout, f9fix_SPLTAGEQ(TwseExCode) "0")); // 0: 一般、盤後定價.
   // |RejStaleOrd=|IvacNoFlag=
   if ((*--pout = iniReq->IvacNoFlag_.Chars_[0]) == '\0') // IvacNoFlag 也許要從 curReq 取得?
      *pout = ' ';                                        // 暫時不考慮 iniReq, curReq 來源不同.

   fon9::StrView       msgType;
   const f9fmkt_RxKind rxKind = curReq->RxKind();
   if (fon9_UNLIKELY(rxKind == f9fmkt_RxKind_RequestQuery)) {
      pout = RevPutStr(pout, f9fix_SPLTAGEQ(TwseIvacnoFlag));
      msgType = f9fix_SPLFLDMSGTYPE(OrderStatusRequest);
   }
   else {
      pout = RevPutStr(pout, f9fix_SPLTAGEQ(TwseRejStaleOrd) "N"
                             f9fix_SPLTAGEQ(TwseIvacnoFlag));
      if (fon9_UNLIKELY(rxKind == f9fmkt_RxKind_RequestDelete)) {
__REQUEST_DELETE:
         msgType = f9fix_SPLFLDMSGTYPE(OrderCancelRequest);
      }
      else {
         const auto* lastOrd = static_cast<const OmsTwsOrderRaw*>(order.Tail());
         // OmsTwsOrderRaw.OType_ 必須在風控檢查階段填入, 這裡不檢查, 如果有錯就讓交易所退單吧!
         *--pout = static_cast<char>(lastOrd->OType_);
         pout = RevPutStr(pout, f9fix_SPLTAGEQ(TwseOrdType));

         OmsTwsQty      fixQty;
         OmsTwsPri      fixPri;
         f9fmkt_PriType fixPriType;
         if (fon9_LIKELY(rxKind == f9fmkt_RxKind_RequestNew)) {
            msgType = f9fix_SPLFLDMSGTYPE(NewOrderSingle);
            switch (iniReq->TimeInForce_) {
            case f9fmkt_TimeInForce_ROD:
               pout = RevPutStr(pout, f9fix_SPLTAGEQ(TimeInForce) f9fix_kVAL_TimeInForce_Day);
               break;
            case f9fmkt_TimeInForce_IOC:
               pout = RevPutStr(pout, f9fix_SPLTAGEQ(TimeInForce) f9fix_kVAL_TimeInForce_IOC);
               break;
            case f9fmkt_TimeInForce_FOK:
               pout = RevPutStr(pout, f9fix_SPLTAGEQ(TimeInForce) f9fix_kVAL_TimeInForce_FOK);
               break;
            default:
            case f9fmkt_TimeInForce_QuoteAutoCancel:
               runner->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_TimeInForce, nullptr);
               return SendResult::RejectRequest;
            }
            fixQty = lastOrd->LeavesQty_;
            fixPri = lastOrd->LastPri_;
            fixPriType = lastOrd->LastPriType_;
         }
         else if (fon9_LIKELY(rxKind == f9fmkt_RxKind_RequestChgQty)) {
            // OrderQty = 整股:欲刪數量, 零股:欲刪數量(20200323之前為剩餘數量).
            const bool isWantToKill = true;
            fixQty = static_cast<OmsTwsQty>(GetTwsRequestChgQty(*runner, curReq, lastOrd, isWantToKill));
            if(fixQty == 0)
               goto __REQUEST_DELETE;
            if (fon9::signed_cast(fixQty) < 0)
               return SendResult::RejectRequest;
            fixPriType = f9fmkt_PriType_Limit;
            msgType = f9fix_SPLFLDMSGTYPE(OrderReplaceRequest);
         }
         else if (fon9_LIKELY(rxKind == f9fmkt_RxKind_RequestChgPri)) {
            if (auto* chgPriReq = dynamic_cast<const OmsTwsRequestChg*>(curReq)) {
               fixPri = chgPriReq->Pri_;
               fixPriType = chgPriReq->PriType_;
            }
            else if (auto* chgPriIniReq = dynamic_cast<const OmsTwsRequestIni*>(curReq)) {
               fixPri = chgPriIniReq->Pri_;
               fixPriType = chgPriIniReq->PriType_;
            }
            else {
               runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_UnknownRequestType, nullptr);
               return SendResult::RejectRequest;
            }
            fixQty = 0;
            msgType = f9fix_SPLFLDMSGTYPE(OrderReplaceRequest);
         }
         else {
            runner->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_RxKind, nullptr);
            return SendResult::RejectRequest;
         }

         switch (fixPriType) {
         case f9fmkt_PriType_Unknown:
         case f9fmkt_PriType_Limit:
            // 20200323: Price: Max 5 digits + 4 decimals, 修改前為 Max 6 digits + 3 decimals
            // uint32_t pri = fon9::Decimal<uint32_t, 2>{iniReq->Pri_.GetOrigValue(), iniReq->Pri_.Scale}.GetOrigValue();
            // if (fon9_UNLIKELY(pri > 999999)) {
            //    runner->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_Pri, nullptr);
            //    return SendResult::RejectRequest;
            // }
            // pout = fon9::Pic9ToStrRev<2>(pout, pri % 100);
            // *--pout = '.';
            // pout = fon9::Pic9ToStrRev<4>(pout, pri / 100);
            pout = fon9::ToStrRev(pout, fixPri);
            pout = RevPutStr(pout,
                             f9fix_SPLTAGEQ(OrdType) f9fix_kVAL_OrdType_Limit
                             f9fix_SPLTAGEQ(Price));
            break;
         case f9fmkt_PriType_Market:
            pout = RevPutStr(pout,
                             f9fix_SPLTAGEQ(OrdType) f9fix_kVAL_OrdType_Market
                             f9fix_SPLTAGEQ(Price)   "0");
            break;
         default:
         case f9fmkt_PriType_Mwp:
            runner->Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_PriType, nullptr);
            return SendResult::RejectRequest;
         }

         if (fon9_LIKELY(req.SessionId() != f9fmkt_TradingSessionId_OddLot))
            fixQty /= f9fmkt::GetSymbTwsShUnit(order.GetSymb(runner->Resource_, iniReq->Symbol_));
         pout = fon9::UIntToStrRev(pout, fixQty);
         pout = RevPutStr(pout, f9fix_SPLTAGEQ(OrderQty));
      }
      if (fon9_LIKELY(rxKind == f9fmkt_RxKind_RequestNew)) {
         if (fon9_UNLIKELY(!static_cast<TwsTradingLineMgr*>(&this->FixManager_)->AllocOrdNo(*runner)))
            return SendResult::RejectRequest;
      }
      else {
         // OrigClOrdID.
         pout = RevCopyFill(pout, f9tws::ClOrdID::size(), ToStrView(iniReq->ReqUID_));
         pout = RevPutStr(pout, f9fix_SPLTAGEQ(OrigClOrdID));
      }
      // 帳號必須填滿7碼, 前方補 '0'.
      pout = fon9::Pic9ToStrRev<7>(pout, iniReq->IvacNo_);
      pout = RevPutStr(pout, f9fix_SPLTAGEQ(Account));
   }
   pout = RevCopyFill(pout, f9tws::StkNo::size(), ToStrView(iniReq->Symbol_));
   pout = RevPutStr(pout, f9fix_SPLTAGEQ(Symbol));
   assert(ToStrView(ordraw.OrdNo_).size() == sizeof(OmsOrdNo));
   pout = RevPutStr(pout, ordraw.OrdNo_.begin(), sizeof(OmsOrdNo));
   pout = RevPutStr(pout, f9fix_SPLTAGEQ(OrderID));
   ordraw.OutPvcId_ = this->LineArgs_.SocketId_;
   // ClOrdID
   pout = RevCopyFill(pout, f9tws::ClOrdID::size(), ToStrView(curReq->ReqUID_));
   pout = RevPutStr(pout, f9fix_SPLTAGEQ(ClOrdID));
   // -----------
   // fix header:
   // "SenderSubID=BrkId" 下單帳號所屬券商.
   pout = RevPutStr(pout, iniReq->BrkId_.begin(), iniReq->BrkId_.size());
   pout = RevPutStr(pout, f9fix_SPLTAGEQ(SenderSubID));
   // "TargetSubID=ApCode"
   *--pout = static_cast<char>(twsApCode);
   pout = RevPutStr(pout, f9fix_SPLTAGEQ(TargetSubID));
   // -----------
   fixb.GetBuffer().SetPrefixUsed(pout);
   this->FlowCounter_.ForceUsed(now);
   this->FixSender_->Send(msgType, std::move(fixb), &runner->ExLogForUpd_);
   runner->Update(f9fmkt_TradingRequestSt_Sending, ToStrView(this->StrSendingBy_));
   return SendResult::Sent;
}

} // namespaces
