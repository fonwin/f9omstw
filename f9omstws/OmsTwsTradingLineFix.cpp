// \file f9omstws/OmsTwsTradingLineFix.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsTradingLineFix.hpp"
#include "f9omstws/OmsTwsTradingLineMgr.hpp"
#include "f9omstws/OmsTwsReport.hpp"
#include "f9omstws/OmsTwsReportTools.hpp"
#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "fon9/fix/FixBusinessReject.hpp"
#include "fon9/fix/FixAdminDef.hpp"

namespace f9omstw {

inline void SetupReportForceInternalMessage(OmsTwsReport& rpt) {
   rpt.SetForceInternal();
   rpt.SesName_.AssignFrom(fon9_kCSTR_OmsForceInternal);
   rpt.Message_.assign(fon9_kCSTR_OmsForceInternal);
}
//--------------------------------------------------------------------------//
void TwsTradingLineFixFactory::OnFixReject(const f9fix::FixRecvEvArgs& rxargs, const f9fix::FixOrigArgs& orig) {
   // SessionReject or BusinessReject:
   // 這種失敗比較單純, 因為是針對下單要求的內容不正確:
   // 所以用 orig 找原始下單要求, 如果找不到, 就拋棄此筆回報.
   const auto* fixfld = orig.Msg_.GetField(f9fix_kTAG_ClOrdID);
   if (fixfld == nullptr)
      return;
   OmsRequestRunner runner{rxargs.MsgStr_};
   fon9::RevPrint(runner.ExLog_, orig.MsgStr_, fon9_kCSTR_ROWSPL ">" fon9_kCSTR_CELLSPL);
   runner.Request_ = this->RptFactory_.MakeReportIn(f9fmkt_RxKind_Unknown, fon9::UtcNow());

   assert(dynamic_cast<OmsTwsReport*>(runner.Request_.get()) != nullptr);
   OmsTwsReport&  rpt = *static_cast<OmsTwsReport*>(runner.Request_.get());
   SetupReportForceInternalMessage(rpt);
   rpt.SetReportSt(f9fmkt_TradingRequestSt_ExchangeRejected);
   AssignReportReqUID(rpt, *fixfld);
   if ((fixfld = orig.Msg_.GetField(f9fix_kTAG_OrderID)) != nullptr)
      rpt.OrdNo_.AssignFrom(fixfld->Value_);
   if ((fixfld = orig.Msg_.GetField(f9fix_kTAG_SenderSubID)) != nullptr)
      rpt.BrkId_.CopyFrom(fixfld->Value_);
   rpt.SetSessionId(GetFixSessionId(orig.Msg_.GetField(f9fix_kTAG_TargetSubID)));
   rpt.SetMarket(static_cast<f9tws::ExgTradingLineFix*>(rxargs.FixSession_)->LineArgs_.Market_);
   rpt.OutPvcId_ = static_cast<f9tws::ExgTradingLineFix*>(rxargs.FixSession_)->LineArgs_.SocketId_;
   assert((fixfld = orig.Msg_.GetField(f9fix_kTAG_SenderCompID)) != nullptr
          && fixfld->Value_.Get1st() == rpt.Market());
   AssignTwsReportMessage(rpt, rxargs.Msg_);
   if ((fixfld = orig.Msg_.GetField(f9fix_kTAG_MsgType)) != nullptr
       && fixfld->Value_.Get1st() == *f9fix_kMSGTYPE_SessionReject) {
      // 09xxx = FIX SessionReject.
      rpt.SetErrCode(static_cast<OmsErrCode>((rpt.ErrCode() % 1000) + OmsErrCode_FromExgSessionReject));
   }
   this->CoreMgr_.CurrentCore()->MoveToCore(std::move(runner));
}
void TwsTradingLineFixFactory::OnFixCancelReject(const f9fix::FixRecvEvArgs& rxargs) {
   // 如果是 CancelReject: 必要欄位僅提供 Account, 沒有提供 Symbol, Side, OType...
   // 所以, 如果沒找到原下單要求, 就直接拋棄此「刪改失敗回報」?
   //const auto* fixfld = rxargs.Msg_.GetField(f9fix_kTAG_CxlRejResponseTo);
   //if (fixfld == nullptr)
   //   return;
   f9fmkt_RxKind  rptKind = f9fmkt_RxKind_Unknown;
   // f9fmkt_RxKind_Unknown: 由 OmsTwsReport::RunReportFromOrig() 自動設定.
   // switch (fixfld->Value_.Get1st()) {
   // case *f9fix_kVAL_CxlRejResponseTo_Cancel:
   //    rptKind = f9fmkt_RxKind_RequestDelete;
   //    break;
   // case *f9fix_kVAL_CxlRejResponseTo_Replace:
   //    rptKind = f9fmkt_RxKind_Unknown;// (? f9fmkt_RxKind_RequestChgPri : f9fmkt_RxKind_RequestChgQty);
   //    break;
   // default:
   //    return;
   // }

   auto             core = this->CoreMgr_.CurrentCore();
   OmsRequestRunner runner{rxargs.MsgStr_};
   runner.Request_ = this->RptFactory_.MakeReportIn(rptKind, fon9::UtcNow());

   assert(dynamic_cast<OmsTwsReport*>(runner.Request_.get()) != nullptr);
   OmsTwsReport&  rpt = *static_cast<OmsTwsReport*>(runner.Request_.get());
   SetupReportForceInternalMessage(rpt);
   rpt.SetReportSt(f9fmkt_TradingRequestSt_ExchangeRejected);
   AssignTwsReportBase(rpt, rxargs, core->TDay());
   core->MoveToCore(std::move(runner));
}
void TwsTradingLineFixFactory::OnFixExecReport(const f9fix::FixRecvEvArgs& rxargs) {
   const auto* fixfld = rxargs.Msg_.GetField(f9fix_kTAG_ExecType);
   if (fixfld == nullptr)
      return;
   auto chExecType = fixfld->Value_.Get1st();
   switch (chExecType) {
   case *f9fix_kVAL_ExecType_PartialFill_42: // "1"
   case *f9fix_kVAL_ExecType_Fill_42:        // "2"
   case *f9fix_kVAL_ExecType_Trade_44:       // "F" FIX.4.4: Trade (partial fill or fill)
      this->OnFixExecFilled(rxargs);
      return;
   }

   // LeavesQty = [AFTER-QUANTITY]
   // OrderQty  = [BEFORE-QUANTITY]-[AFTER-QUANTITY]後取絕對值.
   //             改價: OrderQty(38)同委託剩餘有效量LeavesQty(151)，其欄位值為改價成功之委託數量。
   const OmsTwsQty         afterQty    = GetFixFieldQty(rxargs.Msg_, f9fix_kTAG_LeavesQty);
   const OmsTwsQty         fixOrderQty = GetFixFieldQty(rxargs.Msg_, f9fix_kTAG_OrderQty);
   OmsTwsQty               beforeQty = fixOrderQty;
   f9fmkt_RxKind           rptKind;
   f9fmkt_TradingRequestSt rptSt;
   switch (chExecType) {
   case *f9fix_kVAL_ExecType_New:            // "0" 新單成功.
      // 可能部分委託數量有效: OrdRejReason#103=99; Text#58=0031-QUANTITY WAS CUT;
      rptKind = f9fmkt_RxKind_RequestNew;
      rptSt = f9fmkt_TradingRequestSt_ExchangeAccepted;
      break;
   case *f9fix_kVAL_ExecType_Rejected:       // "8" 新單失敗.
      rptKind = f9fmkt_RxKind_RequestNew;
      rptSt = f9fmkt_TradingRequestSt_ExchangeRejected;
      break;
   case *f9fix_kVAL_ExecType_Replaced:       // "5" 改單成功.
      // 改價回覆: OrderQty#38 == LeavesQty#151，其欄位值為改價成功之委託數量。
      // 但是: 如果交易所剩餘6, 要求減量3, 成功剩餘3; 則此時:
      //       #38=3:  [BEFORE-QUANTITY=6]-[AFTER-QUANTITY=3]後取絕對值.
      //       #151=3: [AFTER-QUANTIT=3]
      //       此時改量成功的 OrderQty#38 == LeavesQty#151;
      // 因此: 無法根據 (OrderQty#38 == LeavesQty#151) 判斷交易所回覆的 RxKind;
      beforeQty = (fixOrderQty + afterQty);
      rptKind = f9fmkt_RxKind_Unknown;//(fixOrderQty == afterQty ? f9fmkt_RxKind_RequestChgPri : f9fmkt_RxKind_RequestChgQty);
      rptSt = f9fmkt_TradingRequestSt_ExchangeAccepted;
      break;
   case *f9fix_kVAL_ExecType_Canceled:       // "4" 刪單成功.
      rptKind = f9fmkt_RxKind_RequestDelete;
      rptSt = f9fmkt_TradingRequestSt_ExchangeAccepted;
      break;
   case *f9fix_kVAL_ExecType_OrderStatus:    // "I" 查詢成功 FIX.4.4: (formerly an ExecTransType <20>)
      rptKind = f9fmkt_RxKind_RequestQuery;
      rptSt = f9fmkt_TradingRequestSt_ExchangeAccepted;
      break;
   case *f9fix_kVAL_ExecType_Restated:       // "D" Restated
      // 證券進入價格穩定措施或尾盤集合競價時段，交易所主動取消留存委託簿之「市價」委託單資料並回報.
      // 此時 OrderStatus(39) 則為 Canceled。
      if ((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_OrdStatus)) == nullptr
          || fixfld->Value_.Get1st() != *f9fix_kVAL_OrdStatus_Canceled)
         return;
      // 因為只有取消「市價」, 所以只有「新單的 ClOrdID」, 不會有「OrigClOrdID」.
      // 針對原新單要求: ExchangeCanceled;
      rptKind = f9fmkt_RxKind_RequestNew;
      rptSt = f9fmkt_TradingRequestSt_ExchangeCanceling;
      break;
   // case *f9fix_kVAL_ExecType_PendingNew:     // "A"
   // case *f9fix_kVAL_ExecType_PendingCancel:  // "6"
   // case *f9fix_kVAL_ExecType_PendingReplace: // "E"
   // case *f9fix_kVAL_ExecType_Stopped:        // "7"
   // case *f9fix_kVAL_ExecType_Suspended:      // "9"
   // case *f9fix_kVAL_ExecType_Calculated:     // "B"
   // case *f9fix_kVAL_ExecType_Expired:        // "C"
   // case *f9fix_kVAL_ExecType_DoneForDay:     // "3"
   default:
      return;
   }

   auto             core = this->CoreMgr_.CurrentCore();
   OmsRequestRunner runner{rxargs.MsgStr_};
   runner.Request_ = this->RptFactory_.MakeReportIn(rptKind, fon9::UtcNow());

   assert(dynamic_cast<OmsTwsReport*>(runner.Request_.get()) != nullptr);
   OmsTwsReport&  rpt = *static_cast<OmsTwsReport*>(runner.Request_.get());
   SetupReportForceInternalMessage(rpt);
   rpt.SetReportSt(rptSt);
   rpt.Qty_ = afterQty;
   rpt.BeforeQty_ = beforeQty;

   // 無法從 FIX 取得的 rpt 欄位:
   // fon9::CharAry<10>    SubacNo_;
   // fon9::CharAry<10>    SalesNo_;
   // fon9::CharAry<16>    UsrDef_;
   // fon9::CharAry<16>    ClOrdId_;
   // fon9::CharAry<8>     SesName_;
   // fon9::CharAry<12>    UserId_;
   // fon9::CharAry<16>    FromIp_;
   // fon9::CharAry<4>     Src_;
   AssignReportSymbol(rpt.Symbol_, rxargs.Msg_);
   rpt.Side_ = GetFixSide(rxargs.Msg_);

   if ((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_TimeInForce)) != nullptr) {
      switch (fixfld->Value_.Get1st()) {
      case *f9fix_kVAL_TimeInForce_IOC:   rpt.TimeInForce_ = f9fmkt_TimeInForce_IOC;   break;
      case *f9fix_kVAL_TimeInForce_FOK:   rpt.TimeInForce_ = f9fmkt_TimeInForce_FOK;   break;
      }
   }
   if ((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_OrdType)) != nullptr) {
      switch (fixfld->Value_.Get1st()) {
      case *f9fix_kVAL_OrdType_Limit:  rpt.PriType_ = f9fmkt_PriType_Limit;   break;
      case *f9fix_kVAL_OrdType_Market: rpt.PriType_ = f9fmkt_PriType_Market;  break;
      }
   }
   if ((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_Price)) != nullptr) {
      rpt.Pri_ = fon9::StrTo(fixfld->Value_, rpt.Pri_);
      if (rpt.Pri_.GetOrigValue() == 0)
         rpt.Pri_.AssignNull();
   }
   if ((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_TwseIvacnoFlag)) != nullptr)
      rpt.IvacNoFlag_.Chars_[0] = static_cast<char>(fixfld->Value_.Get1st());
   if ((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_TwseOrdType)) != nullptr)
      rpt.OType_ = static_cast<OmsTwsOType>(fixfld->Value_.Get1st());
   rpt.OutPvcId_ = static_cast<f9tws::ExgTradingLineFix*>(rxargs.FixSession_)->LineArgs_.SocketId_;
   AssignTwsReportBase(rpt, rxargs, core->TDay());
   core->MoveToCore(std::move(runner));
}
void TwsTradingLineFixFactory::OnFixExecFilled(const f9fix::FixRecvEvArgs& rxargs) {
   auto             core = this->CoreMgr_.CurrentCore();
   OmsRequestRunner runner{rxargs.MsgStr_};
   runner.Request_ = this->FilFactory_.MakeReportIn(f9fmkt_RxKind_Filled, fon9::UtcNow());
   runner.Request_->SetForceInternal();

   assert(dynamic_cast<OmsTwsFilled*>(runner.Request_.get()) != nullptr);
   OmsTwsFilled&  rpt = *static_cast<OmsTwsFilled*>(runner.Request_.get());
   AssignRptBase(rpt, rxargs);
   AssignReportSymbol(rpt.Symbol_, rxargs.Msg_);
   rpt.Side_ = GetFixSide(rxargs.Msg_);
   rpt.Time_ = GetFixTransactTime(rxargs.Msg_, core->TDay());
   const fon9::fix::FixParser::FixField* fixfld;
   if ((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_Account)) != nullptr)
      rpt.IvacNo_ = fon9::StrTo(fixfld->Value_, 0u);
   if ((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_LastPx)) != nullptr)
      rpt.Pri_ = fon9::StrTo(fixfld->Value_, rpt.Pri_);
   rpt.Qty_ = GetFixFieldQty(rxargs.Msg_, f9fix_kTAG_LastQty);
   if ((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_ExecID)) != nullptr) {
      // fixfld->Value_[0]==Side;
      rpt.MatchKey_ = fon9::StrTo(fon9::StrView{fixfld->Value_.begin() + 1, fixfld->Value_.end()},
                                  rpt.MatchKey_);
   }
   core->MoveToCore(std::move(runner));
}
//--------------------------------------------------------------------------//
TwsTradingLineFixFactory::TwsTradingLineFixFactory(
                                 OmsCoreMgr&          coreMgr,
                                 OmsTwsReportFactory& rptFactory,
                                 OmsTwsFilledFactory& filFactory,
                                 std::string          fixLogPathFmt,
                                 Named&&              name)
   : base(std::move(fixLogPathFmt), std::move(name))
   , TwsTradingLineFactoryBase{coreMgr, rptFactory, filFactory} {
   using namespace std::placeholders;
   auto onFixReject = std::bind(&TwsTradingLineFixFactory::OnFixReject, this, _1, _2);
   this->FixConfig_.Fetch(f9fix_kMSGTYPE_NewOrderSingle)     .FixRejectHandler_ = onFixReject;
   this->FixConfig_.Fetch(f9fix_kMSGTYPE_OrderReplaceRequest).FixRejectHandler_ = onFixReject;
   this->FixConfig_.Fetch(f9fix_kMSGTYPE_OrderCancelRequest) .FixRejectHandler_ = onFixReject;
   this->FixConfig_.Fetch(f9fix_kMSGTYPE_OrderStatusRequest) .FixRejectHandler_ = onFixReject;

   this->FixConfig_.Fetch(f9fix_kMSGTYPE_ExecutionReport).FixMsgHandler_
      = std::bind(&TwsTradingLineFixFactory::OnFixExecReport, this, _1);

   this->FixConfig_.Fetch(f9fix_kMSGTYPE_OrderCancelReject).FixMsgHandler_
      = std::bind(&TwsTradingLineFixFactory::OnFixCancelReject, this, _1);
}
fon9::TimeStamp TwsTradingLineFixFactory::GetTDay() {
   return this->CoreMgr_.CurrentCoreTDay();
}
fon9::io::SessionSP TwsTradingLineFixFactory::CreateTradingLineFix(
   f9tws::ExgTradingLineMgr&           lineMgr,
   const f9tws::ExgTradingLineFixArgs& args,
   f9fix::IoFixSenderSP                fixSender) {
   return new TwsTradingLineFix(lineMgr, this->FixConfig_, args, std::move(fixSender));
}
//--------------------------------------------------------------------------//
TwsTradingLineFix::TwsTradingLineFix(f9fix::IoFixManager&                mgr,
                                     const f9fix::FixConfig&             fixcfg,
                                     const f9tws::ExgTradingLineFixArgs& lineargs,
                                     f9fix::IoFixSenderSP&&              fixSender)
   : base{mgr, fixcfg, lineargs, std::move(fixSender)}
   , TwsTradingLineBase{lineargs} {
}
void TwsTradingLineFix::OnFixSessionApReady() {
   if (auto omsCore = static_cast<TwsTradingLineMgr*>(&this->FixManager_)->OmsCore()) {
      fon9::intrusive_ptr<TwsTradingLineFix> pthis{this};
      fon9::CharVector info = fon9::RevPrintTo<fon9::CharVector>(
         "LineReady.",
         this->LineArgs_.Market_,
         this->LineArgs_.BrkId_,
         this->LineArgs_.SocketId_);
      omsCore->SetSendingRequestFail(ToStrView(info), [pthis](const OmsOrderRaw& ordraw) {
         if (auto twsord = dynamic_cast<const OmsTwsOrderRaw*>(&ordraw))
            return (pthis->LineArgs_.SocketId_ == twsord->OutPvcId_);
         return false;
      });
   }
   base::OnFixSessionApReady();
}

} // namespaces
