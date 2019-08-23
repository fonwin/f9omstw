// \file f9omstws/OmsTwsReport.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsReport.hpp"
#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwsReport::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields(flds);
   base::AddFieldsForReport(flds);
   flds.Add(fon9_MakeField2(OmsTwsReport, ExgTime));
   flds.Add(fon9_MakeField2(OmsTwsReport, BeforeQty));
}
//--------------------------------------------------------------------------//
static void AdjustQtyUnit(OmsOrder& order, OmsResource& res, OmsTwsReport& rpt) {
   if (rpt.SessionId() == f9fmkt_TradingSessionId_OddLot)
      return;
   auto shUnit = fon9::fmkt::GetTwsSymbShUnit(order.GetSymb(res, rpt.Symbol_));
   if ((rpt.Qty_ % shUnit) != 0 || (rpt.BeforeQty_ % shUnit) != 0) {
      rpt.Qty_ *= shUnit;
      rpt.BeforeQty_ *= shUnit;
   }
}
static void AdjustLeavesQty(OmsReportRunnerInCore& inCoreRunner) {
   OmsTwsOrderRaw& ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
   if (ordraw.BeforeQty_ == ordraw.AfterQty_)
      return;
   ordraw.LeavesQty_ = ordraw.LeavesQty_ + ordraw.AfterQty_ - ordraw.BeforeQty_;
   if (fon9_UNLIKELY(fon9::signed_cast(ordraw.LeavesQty_) < 0)) {
      fon9::RevPrint(inCoreRunner.ExLogForUpd_, "Bad.Leaves=", fon9::signed_cast(ordraw.LeavesQty_),
                     fon9_kCSTR_ROWSPL ">" fon9_kCSTR_CELLSPL);
      ordraw.LeavesQty_ = 0;
   }
   if (ordraw.LeavesQty_ == 0)
      ordraw.UpdateOrderSt_ = (ordraw.AfterQty_ == 0
                               ? f9fmkt_OrderSt_UserCanceled
                               : f9fmkt_OrderSt_PartCanceled);
}
static bool CheckReportFields(const OmsTwsRequestIni& ini, const OmsTwsReport& rpt) {
   if (rpt.IvacNo_ != 0 && rpt.IvacNo_ != ini.IvacNo_)
      return false;
   if (rpt.Side_ != f9fmkt_Side_Unknown && rpt.Side_ != ini.Side_)
      return false;
   if (!rpt.Symbol_.empty1st() && rpt.Symbol_ != ini.Symbol_)
      return false;
   return true;
}
static void AssignTimeAndPri(OmsTwsOrderRaw& ordraw, const OmsTwsReport& rpt) {
   if (rpt.ExgTime_.IsNull())
      return;
   ordraw.LastExgTime_ = rpt.ExgTime_;
   if (ordraw.UpdateOrderSt_ == f9fmkt_OrderSt_ReportStale
       || f9fmkt_TradingRequestSt_IsRejected(rpt.ReportSt())
       || (rpt.PriType_ == f9fmkt_PriType{} && rpt.Pri_.IsNull()))
      return;
   if (!ordraw.LastPriTime_.IsNull() && rpt.RxKind() != f9fmkt_RxKind_RequestNew) {
      if (ordraw.LastPriType_ == rpt.PriType_ && ordraw.LastPri_ == rpt.Pri_)
         return;
      if (ordraw.LastPriTime_ >= rpt.ExgTime_)
         return;
   }
   // rpt 是有填價格的成功回報(新刪改查) => 一律將 Order.LastPri* 更新成交易所的最後價格.
   ordraw.LastPriTime_ = rpt.ExgTime_;
   ordraw.LastPriType_ = rpt.PriType_;
   ordraw.LastPri_ = rpt.Pri_;
}
static void AssignOrderRawFromRpt(OmsReportRunnerInCore& inCoreRunner, OmsTwsReport& rpt) {
   OmsTwsOrderRaw&   ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
   // ----- 查單回報 ---------------------------------------------
   if (rpt.RxKind() == f9fmkt_RxKind_RequestQuery) {
      // 查詢結果回報, 直接更新(不論新單是否已經成功),
      // 不需要考慮「保留」或 stale(由使用者自行判斷).
      ordraw.AfterQty_ = ordraw.BeforeQty_ = rpt.Qty_;
   }
   // ----- 未收到新單結果的刪改 ----------------------------------
   else if (ordraw.Order().LastOrderSt() < f9fmkt_OrderSt_NewDone) {
      // 尚未收到新單結果: 將改單結果存於 ordraw, 等收到新單結果時再處理.
   __REPORT_PENDING:
      ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_ReportPending;
      ordraw.BeforeQty_ = rpt.BeforeQty_;
      ordraw.AfterQty_ = rpt.Qty_;
   }
   // ----- 已收到新單結果的刪改 ----------------------------------
   else {
      // 回報的改後數量=0, 但有「在途成交」或「遺漏改量」, 應先等補齊後再處理.
      if (f9fmkt_TradingRequestSt_IsRejected(rpt.ReportSt())) {
         if (ordraw.RequestSt_ == f9fmkt_TradingRequestSt_ExchangeNoLeavesQty) {
            if (ordraw.LeavesQty_ > 0)
               goto __REPORT_PENDING;
         }
      }
      else if (rpt.Qty_ == 0 && rpt.BeforeQty_ < ordraw.LeavesQty_)
         goto __REPORT_PENDING;
      // ----- 改價回報 ------------------------------------------
      if (rpt.RxKind() == f9fmkt_RxKind_RequestChgPri) {
         ordraw.AfterQty_ = ordraw.BeforeQty_ = rpt.Qty_;
         if (!ordraw.LastPriTime_.IsNull() && ordraw.LastPriTime_ >= rpt.ExgTime_)
            // 此次的改價回報 = 過時的回報 => 相關欄位不異動(包含價格欄位).
            ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_ReportStale;
      }
      // ----- 刪減回報 ------------------------------------------
      else if (rpt.RxKind() == f9fmkt_RxKind_RequestChgQty
               || rpt.RxKind() == f9fmkt_RxKind_RequestDelete) {
         if (!f9fmkt_TradingRequestSt_IsRejected(rpt.ReportSt())) {
            ordraw.BeforeQty_ = rpt.BeforeQty_;
            ordraw.AfterQty_ = rpt.Qty_;
            AdjustLeavesQty(inCoreRunner);
         }
      }
      else {
         assert(!"TwsReport: Unknown report kind.");
         rpt.Message_.append("TwsReport: Unknown report kind.");
      }
   }
   // -----------------------------------------------------------
   AssignTimeAndPri(ordraw, rpt);
}
static void AssignOrderRawSt(OmsReportRunnerInCore& inCoreRunner, OmsTwsReport& rpt) {
   OmsOrderRaw&   ordraw = inCoreRunner.OrderRaw_;
   if (&ordraw.Request() == &rpt)
      ordraw.Message_ = rpt.Message_;
   else { // ordraw 異動的是已存在的 request, this 即將被刪除, 所以使用 std::move(this->Message_)
      ordraw.Message_ = std::move(rpt.Message_);
      if (IsEnumContains(rpt.RequestFlags(), OmsRequestFlag_ReportNeedsLog)) {
         fon9::RevPrint(inCoreRunner.ExLogForUpd_, fon9_kCSTR_ROWSPL ">" fon9_kCSTR_CELLSPL);
         rpt.RevPrint(inCoreRunner.ExLogForUpd_);
      }
   }
   if (ordraw.UpdateOrderSt_ == f9fmkt_OrderSt_ReportStale)
      ordraw.Message_.append("(stale)");
   inCoreRunner.Update(ordraw.RequestSt_);
}
//--------------------------------------------------------------------------//
// origReq 的後續回報.
void OmsTwsReport::RunReportFromOrig(OmsReportChecker&& checker, const OmsRequestBase& origReq) {
   if (this->RxKind() == f9fmkt_RxKind_Unknown)
      this->RxKind_ = origReq.RxKind();
   else if (this->RxKind_ != origReq.RxKind()) {
      if (this->RxKind_ == f9fmkt_RxKind_RequestDelete) {
         if (origReq.RxKind() == f9fmkt_RxKind_RequestChgQty) {
            // origReq 是改量, 但回報是刪單: 有可能發生,
            // 因為改量要求如果會讓剩餘量為0; 則下單時可能會用「刪單訊息」.
            goto __RX_KIND_OK;
         }
      }
      assert(this->RxKind_ == origReq.RxKind());
      checker.ReportAbandon("TwsReport: Report kind not match orig request.");
      return;
   __RX_KIND_OK:;
   }
   if (checker.CheckerSt_ != OmsReportCheckerSt::NotReceived) {
      // 不確定是否有收過此回報: 檢查是否重複回報.
      if (fon9_UNLIKELY(this->ReportSt() <= origReq.LastUpdated()->RequestSt_)) {
         checker.ReportAbandon("TwsReport: Duplicate or Obsolete report.");
         return;
      }
      // origReq 已存在的回報, 不用考慮 BeforeQty, AfterQty 重複? 有沒有底下的可能?
      // => f9oms.A => TwsRpt.1:Sending         => f9oms.Local (TwsRpt.1:Sending)
      // => f9oms.A => TwsRpt.1:Bf=10,Af=9
      //            => BrokerHost(C:Bf=10,Af=9) => f9oms.Local (TwsRpt.C:Bf=10,Af=9)
      //                                           因無法識別此回報為 TwsRpt.1 所以建立新的 TwsRpt.C
      //            => TwsRpt.1:Bf=10,Af=9      => f9oms.Local (TwsRpt.1:Bf=10,Af=9)
      //                                           此時收到的為重複回報!
      // => 為了避免發生此情況, 這裡要再一次強調(已於 ReportIn.md 說明):
      // * 如果券商主機的回報格式, 可以辨別下單要求來源是否為 f9oms, 則可將其過濾, 只處理「非 f9oms」的回報.
      // * 如果無法辨別, 則 f9oms 之間「不可以」互傳回報.
   }
   OmsOrder& order = origReq.LastUpdated()->Order();
   AdjustQtyUnit(order, checker.Resource_, *this);

   OmsReportRunnerInCore  inCoreRunner{std::move(checker), *order.BeginUpdate(origReq)};
   // ----- 新單回報 ---------------------------------------------
   if (fon9_LIKELY(this->RxKind() == f9fmkt_RxKind_RequestNew)) {
      // 新單回報(已排除重複), 直接更新, 不需要考慮「保留」或 stale.
      OmsTwsOrderRaw& ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
      if (ordraw.OType_ == OmsTwsOType{})
         ordraw.OType_ = this->OType_;
      ordraw.AfterQty_ = ordraw.LeavesQty_ = this->Qty_;
      AssignTimeAndPri(ordraw, *this);
   }
   // ----- 刪改查回報 -------------------------------------------
   else if (fon9_LIKELY(this->ReportSt() > f9fmkt_TradingRequestSt_LastRunStep)) {
      // 刪改查的下單過程(Queuing,Sending)回報: 直接更新 RequestSt 即可, 不影響 Order 最後內容.
      // 只有「流程結束的回報 this->ReportSt() > f9fmkt_TradingRequestSt_LastRunStep」才有可能「暫時保留」.
      AssignOrderRawFromRpt(inCoreRunner, *this);
   }
   AssignOrderRawSt(inCoreRunner, *this);
}
// 委託不存在(或 initiator 不存在)的新單回報.
static void RunReportNew(OmsReportRunnerInCore&& inCoreRunner, OmsTwsReport& rpt) {
   assert(dynamic_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_) != nullptr);
   inCoreRunner.Resource_.Backend_.FetchSNO(rpt);
   OmsTwsOrderRaw& ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
   AdjustQtyUnit(ordraw.Order(), inCoreRunner.Resource_, rpt);
   ordraw.OType_ = rpt.OType_;
   ordraw.OrdNo_ = rpt.OrdNo_;
   ordraw.AfterQty_ = ordraw.LeavesQty_ = rpt.Qty_;
   AssignTimeAndPri(ordraw, rpt);
   AssignOrderRawSt(inCoreRunner, rpt);
}
// 刪改查回報.
static void RunReportDCQ(OmsReportRunnerInCore&& inCoreRunner, OmsTwsReport& rpt) {
   assert(rpt.RxKind() != f9fmkt_RxKind_Unknown);
   assert(dynamic_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_) != nullptr);
   inCoreRunner.Resource_.Backend_.FetchSNO(rpt);
   AdjustQtyUnit(inCoreRunner.OrderRaw_.Order(), inCoreRunner.Resource_, rpt);
   if (!rpt.OrdNo_.empty1st())
      inCoreRunner.OrderRaw_.OrdNo_ = rpt.OrdNo_;
   if (fon9_LIKELY(rpt.ReportSt() > f9fmkt_TradingRequestSt_LastRunStep))
      AssignOrderRawFromRpt(inCoreRunner, rpt);
   AssignOrderRawSt(inCoreRunner, rpt);
}

void OmsTwsReport::RunReportInCore(OmsReportChecker&& checker) {
   assert(checker.Report_.get() == this);

   if (const OmsRequestBase* origReq = checker.SearchOrigRequestId()) {
      this->RunReportFromOrig(std::move(checker), *origReq);
      return;
   }
   // checker.SearchOrigRequestId(); 失敗,
   // 若已呼叫過 checker.ReportAbandon(), 則直接結束.
   if (checker.CheckerSt_ == OmsReportCheckerSt::Abandoned)
      return;
   // ReqUID 找不到原下單要求, 使用 OrdKey 來找.
   if (this->OrdNo_.empty1st()) {
      if (this->RxKind() == f9fmkt_RxKind_RequestNew) {
         // TODO: this 沒有委託書號, 如何取得 origReq? 如何判斷重複?
         //    - 建立 ReqUID map?
         // or - f9oms 首次異動就必須提供 OrdNo (即使是 Queuing), 那 Abandon request 呢(不匯入?)?
         //      沒有提供 OrdNo 就拋棄此次回報?
         if (0); checker.ReportAbandon("OmsTwsReport: OrdNo is empty?");
         return;
      }
      checker.ReportAbandon("OmsTwsReport: OrdNo is empty, but kind isnot 'N'");
      return;
   }
   OmsOrdNoMap* ordnoMap = checker.GetOrdNoMap();
   if (ordnoMap == nullptr) // checker.GetOrdNoMap(); 失敗.
      return;               // 已呼叫過 checker.ReportAbandon(), 所以直接結束.

   if (this->ReqUID_.empty1st())
      this->MakeReportReqUID(this->ExgTime_, this->BeforeQty_);

   OmsOrder* order;
   // ----- 委託已存在 --------------------------------------------------
   if ((order = ordnoMap->GetOrder(this->OrdNo_)) != nullptr) {
      assert(order->Head() != nullptr);
      if (this->RxKind() == f9fmkt_RxKind_RequestNew) {
         if (auto origReq = order->Initiator())
            this->RunReportFromOrig(std::move(checker), *origReq);
         else
            RunReportNew(OmsReportRunnerInCore{std::move(checker), *order->BeginUpdate(*this)}, *this);
         return;
      }
      const OmsOrderRaw* ordu = order->Head();
      do {
         const OmsRequestBase& requ = ordu->Request();
         if ((requ.RxKind() == this->RxKind() || this->RxKind() == f9fmkt_RxKind_Unknown)
             && static_cast<const OmsTwsOrderRaw*>(ordu)->BeforeQty_ == this->BeforeQty_
             && static_cast<const OmsTwsOrderRaw*>(ordu)->AfterQty_ == this->Qty_) {
            if (static_cast<const OmsTwsOrderRaw*>(ordu)->LastExgTime_ == this->ExgTime_
                || requ.RxKind() == f9fmkt_RxKind_RequestChgQty
                || requ.RxKind() == f9fmkt_RxKind_RequestDelete) {
               this->RunReportFromOrig(std::move(checker), requ);
               return;
            }
         }
      } while ((ordu = ordu->Next()) != nullptr);
      // order 裡面沒找到 origReq: this = 刪改查回報.
      if (this->RxKind_ != f9fmkt_RxKind_Unknown) {
         RunReportDCQ(OmsReportRunnerInCore{std::move(checker), *order->BeginUpdate(*this)}, *this);
         return;
      }
   }
   // ----- 委託不存在 --------------------------------------------------
   if (this->RxKind_ == f9fmkt_RxKind_Unknown) {
      checker.ReportAbandon("TwsReport: Unknown report kind.");
      return;
   }
   assert(this->Creator().OrderFactory_.get() != nullptr);
   OmsOrderFactory* ordfac = this->Creator().OrderFactory_.get();
   if (fon9_UNLIKELY(ordfac == nullptr)) {
      checker.ReportAbandon("OmsTwsReport: No OrderFactory.");
      return;
   }
   OmsReportRunnerInCore inCoreRunner{std::move(checker), *ordfac->MakeOrder(*this, nullptr)};
   if (this->RxKind() == f9fmkt_RxKind_RequestNew)
      RunReportNew(std::move(inCoreRunner), *this);
   else // 委託不存在的刪改查回報.
      RunReportDCQ(std::move(inCoreRunner), *this);
   ordnoMap->EmplaceOrder(inCoreRunner.OrderRaw_);
}
//--------------------------------------------------------------------------//
static void ReportFromOrderRaw(OmsResource& res, const OmsRequestBase& rpt, const OmsTwsReport* chkFields) {
   assert(rpt.LastUpdated() && rpt.LastUpdated()->UpdateOrderSt_ == f9fmkt_OrderSt_ReportPending);
   OmsOrder& order = rpt.LastUpdated()->Order();
   if (order.LastOrderSt() < f9fmkt_OrderSt_NewDone)
      return;
   const OmsTwsOrderRaw&   ordlast = *static_cast<const OmsTwsOrderRaw*>(order.Tail());
   const OmsTwsOrderRaw&   rptraw = *static_cast<const OmsTwsOrderRaw*>(rpt.LastUpdated());
   // rpt 要等 Order.LeavesQty 符合要求?
   if (rptraw.AfterQty_ == 0) {
      if (ordlast.LeavesQty_ != rptraw.BeforeQty_) {
         if (rptraw.BeforeQty_ != 0)
            return;
         // rptraw.BeforeQty_ == rptraw.AfterQty_ == 0: rpt 為刪改失敗的回報
         // => 可能永遠等不到 LeavesQty 符合要求! (例如: TIME IS LATE 的失敗)
         // 如果 rpt 是 ExchangeNoLeavesQty: 則一定是有遺漏回報 => 此時需要等候 LeavesQty 符合.
         if (rptraw.RequestSt_ == f9fmkt_TradingRequestSt_ExchangeNoLeavesQty)
            return;
         // 如果 rpt 不是 ExchangeNoLeavesQty: 則是一般的刪改失敗(欄位錯誤? TIME IS LATE?...)
         // => 此時應立即處理失敗.
         // => 有可能需要重送.
      }
   }
   OmsReportRunnerInCore  inCoreRunner{res, *order.BeginUpdate(rpt)};
   OmsTwsOrderRaw&        ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
   if (chkFields) {
      if (auto ini = dynamic_cast<const OmsTwsRequestIni*>(order.Initiator())) {
         if (fon9_UNLIKELY(!CheckReportFields(*ini, *chkFields))) {
            // chk 欄位與新單回報不同.
            ordraw.ErrCode_ = OmsErrCode_FieldNotMatch;
            ordraw.RequestSt_ = f9fmkt_TradingRequestSt_InternalRejected;
            ordraw.Message_.assign("TwsPendingReport: FieldNotMatch.");
            return;
         }
      }
   }
   ordraw.BeforeQty_   = rptraw.BeforeQty_;
   ordraw.AfterQty_    = rptraw.AfterQty_;
   ordraw.LastExgTime_ = rptraw.LastExgTime_;
   if (!rptraw.LastPriTime_.IsNull()) { // rpt 有填價格.
      if (ordraw.LastPriTime_.IsNull()  // ord 目前沒有填價格 || rpt 為新的價格
          || (ordraw.LastPriTime_ < rptraw.LastPriTime_
              && (ordraw.LastPriType_ != rptraw.LastPriType_ || ordraw.LastPri_ != rptraw.LastPri_))) {
         ordraw.LastPri_ = rptraw.LastPri_;
         ordraw.LastPriType_ = rptraw.LastPriType_;
         ordraw.LastPriTime_ = rptraw.LastPriTime_;
      }
   }
   fon9_WARN_DISABLE_SWITCH;
   switch (rpt.RxKind()) {
   case f9fmkt_RxKind_RequestChgPri:
      break;
   case f9fmkt_RxKind_RequestChgQty:
   case f9fmkt_RxKind_RequestDelete:
      AdjustLeavesQty(inCoreRunner);
      break;
   default: // 「新、查」不會 ReportPending.
      ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_ReportStale;
      ordraw.Message_.assign("TwsPendingReport: Unknown RxKind.");
      break;
   }
   fon9_WARN_POP;
}
void OmsTwsReport::ProcessPendingReport(OmsResource& res) const {
   ReportFromOrderRaw(res, *this, this);
}
void OmsTwsRequestChg::ProcessPendingReport(OmsResource& res) const {
   ReportFromOrderRaw(res, *this, nullptr);
}

} // namespaces
