// \file f9omstws/OmsTwsReport.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsReport.hpp"
#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

static void AdjustLeavesQty(OmsRequestRunnerInCore& inCoreRunner) {
   OmsTwsOrderRaw& ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
   ordraw.LeavesQty_ = ordraw.LeavesQty_ + ordraw.AfterQty_ - ordraw.BeforeQty_;
   if (fon9_UNLIKELY(fon9::signed_cast(ordraw.LeavesQty_) < 0)) {
      fon9::RevPrint(inCoreRunner.ExLogForUpd_, "Bad.Leaves=", fon9::signed_cast(ordraw.LeavesQty_),
                     fon9_kCSTR_ROWSPL fon9_kCSTR_CELLSPL);
      ordraw.LeavesQty_ = 0;
   }
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
//--------------------------------------------------------------------------//
void OmsTwsReport::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields(flds);
   base::AddFieldsForReport(flds);
   flds.Add(fon9_MakeField2(OmsTwsReport, ExgTime));
   flds.Add(fon9_MakeField2(OmsTwsReport, BeforeQty));
}

void OmsTwsReport::RunReportFromOrig(OmsReportRunner&& runner, const OmsRequestBase& origReq) {
   assert(this->RxKind_ == origReq.RxKind());
   if (this->RxKind_ != origReq.RxKind()) {
      runner.ReportAbandon("TwsReport: Report kind not match orig request.");
      return;
   }
   OmsOrder& order = origReq.LastUpdated()->Order();
   if (runner.RunnerSt_ != OmsReportRunnerSt::NotReceived) {
      // 不確定是否有收過此回報: 檢查是否重複回報.
      if (fon9_UNLIKELY(this->ReportSt() <= origReq.LastUpdated()->RequestSt_)) {
         runner.ReportAbandon("TwsReport: Duplicate or Obsolete report.");
         return;
      }
      // origReq 已存在的回報, 不用考慮 BeforeQty, AfterQty 重複? 有沒有底下的可能?
      // => f9oms.A => TwsRpt.1:Sending         => f9oms.Local (TwsRpt.1:Sending)
      // => f9oms.A => TwsRpt.1:Bf=10,Af=9
      //            => BrokerHost(C:Bf=10,Af=9) => f9oms.Local (TwsRpt.C:Bf=10,Af=9)
      //                                           因無法識別此回報為 TwsRpt.1 所以建立新的 TwsRpt.C
      //            => TwsRpt.1:Bf=10,Af=9      => f9oms.Local (TwsRpt.1:Bf=10,Af=9)
      //                                           此時收到的為重複回報!
      // => 為了避免發生此情況, 這裡要在一次強調(ReportIn.md 已說過):
      // * 如果券商主機的回報格式, 可以辨別下單要求來源是否為 f9oms, 則可將其過濾, 只處理「非 f9oms」的回報.
      // * 如果無法辨別, 則 f9oms 之間「不可以」互傳回報.
   }
   OmsRequestRunnerInCore  inCoreRunner{std::move(runner), *order.BeginUpdate(origReq)};
   OmsTwsOrderRaw&         ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
   f9fmkt_TradingRequestSt rptst = this->ReportSt();
   if (fon9_LIKELY(rptst > f9fmkt_TradingRequestSt_LastRunStep)) {
      // 下單過程(Queuing,Sending)的回報: 直接更新 RequestSt 即可, 不影響 Order 最後內容.
      // 只有「流程結束的回報 rptst > f9fmkt_TradingRequestSt_LastRunStep」才有可能「暫時保留」.
      // ----- 新單回報 ---------------------------------------------
      if (fon9_LIKELY(this->RxKind() == f9fmkt_RxKind_RequestNew)) {
         // 新單回報(已排除重複), 直接更新, 不需要考慮「保留」或 stale.
         ordraw.AfterQty_ = ordraw.LeavesQty_ = this->Qty_;
         ordraw.LastExgTime_ = ordraw.LastPriTime_ = this->ExgTime_;
         ordraw.LastPriType_ = this->PriType_;
         ordraw.LastPri_ = this->Pri_;
      }
      // ----- 查單回報 ---------------------------------------------
      else if (this->RxKind() == f9fmkt_RxKind_RequestQuery) {
         // 查詢結果回報, 直接更新(不論新單是否已經成功),
         // 不需要考慮「保留」或 stale(由使用者自行判斷).
         ordraw.AfterQty_ = ordraw.BeforeQty_ = this->Qty_;
      }
      // ----- 未收到新單結果的刪改 ----------------------------------
      else if (order.LastOrderSt() < f9fmkt_OrderSt_NewDone) {
         // 尚未收到新單結果: 將改單結果存於 ordraw, 等收到新單結果時再處理.
__REPORT_PENDING:
         ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_ReportPending;
         ordraw.BeforeQty_ = this->BeforeQty_;
         ordraw.AfterQty_ = this->Qty_;
      }
      // ----- 已收到新單結果的刪改 ----------------------------------
      else {
         // 回報的改後數量=0, 但有「在途成交」或「遺漏改量」, 應先等補齊後再處理.
         if (f9fmkt_TradingRequestSt_IsRejected(rptst)) {
            if (rptst == f9fmkt_TradingRequestSt_ExchangeNoLeavesQty) {
               if (ordraw.LeavesQty_ > 0)
                  goto __REPORT_PENDING;
            }
         }
         else if(this->Qty_ == 0 && this->BeforeQty_ < ordraw.LeavesQty_)
            goto __REPORT_PENDING;
         // ----- 改價回報 ------------------------------------------
         if (this->RxKind() == f9fmkt_RxKind_RequestChgPri) {
            if (!ordraw.LastPriTime_.IsNull() && ordraw.LastPriTime_ >= this->ExgTime_)
               // 此次的改價回報 = 過時的回報 => 相關欄位不異動(包含價格欄位).
               ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_ReportStale;
         }
         // ----- 刪減回報 ------------------------------------------
         else if(this->RxKind() == f9fmkt_RxKind_RequestChgQty
              || this->RxKind() == f9fmkt_RxKind_RequestDelete) {
            if (!f9fmkt_TradingRequestSt_IsRejected(rptst)) {
               ordraw.BeforeQty_ = this->BeforeQty_;
               ordraw.AfterQty_ = this->Qty_;
               AdjustLeavesQty(inCoreRunner);
            }
         }
         else {
            assert(!"TwsReport: Unknown report kind.");
            runner.ReportAbandon("TwsReport: Unknown report kind.");
            return;
         }
      }
      // -----------------------------------------------------------
      if (!this->ExgTime_.IsNull()) {
         ordraw.LastExgTime_ = this->ExgTime_;
         if (ordraw.UpdateOrderSt_ != f9fmkt_OrderSt_ReportStale
             && !f9fmkt_TradingRequestSt_IsRejected(rptst)
             && (this->PriType_ != f9fmkt_PriType{} || !this->Pri_.IsNull())
             && (this->PriType_ != ordraw.LastPriType_ || this->Pri_ != ordraw.LastPri_)
             ) {
            // this 是有填價格的成功回報(新刪改查) => 一律將 Order.LastPri* 更新成交易所的最後價格.
            if (ordraw.LastPriTime_.IsNull() || ordraw.LastPriTime_ < this->ExgTime_) {
               ordraw.LastPriTime_ = this->ExgTime_;
               ordraw.LastPriType_ = this->PriType_;
               ordraw.LastPri_ = this->Pri_;
            }
         }
      }
   }

   ordraw.ErrCode_ = this->ErrCode();
   if (&ordraw.Request() == this)
      ordraw.Message_ = this->Message_;
   else { // ordraw 異動的是已存在的 request, this 即將被刪除, 所以使用 std::move(this->Message_)
      ordraw.Message_ = std::move(this->Message_);
      if (IsEnumContains(this->RequestFlags(), OmsRequestFlag_ReportNeedsLog)) {
         fon9::RevPrint(runner.ExLog_, fon9_kCSTR_ROWSPL fon9_kCSTR_CELLSPL);
         this->RevPrint(runner.ExLog_);
      }
   }
   if (ordraw.UpdateOrderSt_ == f9fmkt_OrderSt_ReportStale)
      ordraw.Message_.append("(stale)");
   inCoreRunner.Update(rptst);
}
void OmsTwsReport::RunReportInCore(OmsReportRunner&& runner) {
   assert(runner.Report_.get() == this);
   if (const OmsRequestBase* origReq = runner.SearchOrigRequestId()) {
      this->RunReportFromOrig(std::move(runner), *origReq);
      return;
   }
   // runner.SearchOrigRequestId(); 失敗,
   // 若已呼叫過 runner.ReportAbandon(), 則直接結束.
   if (runner.RunnerSt_ == OmsReportRunnerSt::Abandoned)
      return;
   // ReqUID 找不到原下單要求, 使用 OrdKey 來找.
   if (this->OrdNo_.empty1st()) {
      if (this->RxKind() == f9fmkt_RxKind_RequestNew) {
         // TODO: this 沒有委託書號, 如何取得 origReq? 如何判斷重複?
         //    - 建立 ReqUID map?
         // or - f9oms 首次異動就必須提供 OrdNo (即使是 Queuing), 那 Abandon request 呢(不匯入?)?
         //      沒有提供 OrdNo 就拋棄此次回報?
         if (0); runner.ReportAbandon("OmsTwsReport: OrdNo is empty?");
         return;
      }
      runner.ReportAbandon("OmsTwsReport: OrdNo is empty, but kind isnot 'N'");
      return;
   }
   OmsOrdNoMap* ordnoMap = runner.GetOrdNoMap();
   if (ordnoMap == nullptr) // runner.GetOrdNoMap(); 失敗.
      return;               // 已呼叫過 runner.ReportAbandon(), 所以直接結束.
   OmsOrder* order;
   if ((order = ordnoMap->GetOrder(this->OrdNo_)) != nullptr) {
      // ----- 委託已存在 -----
      assert(order->Head() != nullptr);
      const OmsOrderRaw* ordu = order->Head();
      do {
         if (ordu->Request().ReqUID_ == this->ReqUID_) {
            // 透過 OrdKey 找到了 this 要回報的 origReq.
            this->RunReportFromOrig(std::move(runner), ordu->Request());
            return;
         }
      } while ((ordu = ordu->Next()) != nullptr);
      // order 裡面沒找到 origReq: this = 新收到的回報?
   }
   else {
      // ----- 委託不存在 -----
      assert(this->Creator().OrderFactory_.get() != nullptr);
      if (OmsOrderFactory* ordfac = this->Creator().OrderFactory_.get()) {
         if (this->RxKind() == f9fmkt_RxKind_RequestNew) {
            // 委託不存在的新單回報(或補單).
            if (this->ReqUID_.empty1st()) {
               this->ReqUID_.Chars_[0] = this->Market();
               this->ReqUID_.Chars_[1] = this->SessionId();
               this->ReqUID_.Chars_[2] = this->BrkId_[sizeof(f9tws::BrkId) - 2];
               this->ReqUID_.Chars_[3] = this->BrkId_[sizeof(f9tws::BrkId) - 1];
               memcpy(this->ReqUID_.Chars_ + 4, this->OrdNo_.Chars_, sizeof(this->OrdNo_));
            }
            OmsRequestRunnerInCore inCoreRunner{std::move(runner), *ordfac->MakeOrder(*this, nullptr)};
            assert(dynamic_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_) != nullptr);
            runner.Resource_.Backend_.FetchSNO(*this);
            OmsTwsOrderRaw& ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
            ordraw.OrdNo_ = this->OrdNo_;
            ordraw.AfterQty_ = ordraw.LeavesQty_ = this->Qty_;
            ordraw.LastPri_ = this->Pri_;
            ordraw.LastPriType_ = this->PriType_;
            ordraw.OType_ = this->OType_;
            ordraw.LastExgTime_ = ordraw.LastPriTime_ = this->ExgTime_;
            ordraw.ErrCode_ = this->ErrCode();
            ordraw.Message_ = this->Message_;
            inCoreRunner.Update(this->ReportSt());
            ordnoMap->EmplaceOrder(ordraw);
            return;
         }
         // 委託不存在的刪改查回報: 
      }
      else {
         runner.ReportAbandon("OmsTwsReport: No OrderFactory.");
         return;
      }
   }
   if (0); runner.ReportAbandon("Search orig order by OrdKey: Not Impl.");
}
//--------------------------------------------------------------------------//
static void ProcessPendingReportFromOrderRaw(OmsResource& res, const OmsRequestBase& rpt, const OmsTwsReport* chk) {
   assert(rpt.LastUpdated() && rpt.LastUpdated()->UpdateOrderSt_ == f9fmkt_OrderSt_ReportPending);
   OmsOrder& order = rpt.LastUpdated()->Order();
   if (order.LastOrderSt() < f9fmkt_OrderSt_NewDone)
      return;
   const OmsTwsOrderRaw&   ordlast = *static_cast<const OmsTwsOrderRaw*>(order.Tail());
   const OmsTwsOrderRaw&   rptraw = *static_cast<const OmsTwsOrderRaw*>(rpt.LastUpdated());
   // rpt 要等 Order.LeavesQty 符合要求?
   if (rptraw.AfterQty_ == 0 && ordlast.LeavesQty_ != rptraw.BeforeQty_)
      return;
   OmsRequestRunnerInCore  inCoreRunner{res, *order.BeginUpdate(rpt)};
   OmsTwsOrderRaw&         ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
   if (chk) {
      if (auto ini = dynamic_cast<const OmsTwsRequestIni*>(order.Initiator())) {
         if (fon9_UNLIKELY(!CheckReportFields(*ini, *chk))) {
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
   ordraw.RequestSt_   = rptraw.RequestSt_;
   ordraw.ErrCode_     = rptraw.ErrCode_;
   ordraw.LastExgTime_ = rptraw.LastExgTime_;
   if (!rptraw.LastPriTime_.IsNull()) {
      ordraw.LastPri_ = rptraw.LastPri_;
      ordraw.LastPriType_ = rptraw.LastPriType_;
      ordraw.LastPriTime_ = rptraw.LastPriTime_;
   }
   fon9_WARN_DISABLE_SWITCH;
   switch (rpt.RxKind()) {
   case f9fmkt_RxKind_RequestChgPri:
      break;
   case f9fmkt_RxKind_RequestChgQty:
   case f9fmkt_RxKind_RequestDelete:
      if (ordraw.BeforeQty_ == ordraw.AfterQty_)
         break;
      AdjustLeavesQty(inCoreRunner);
      if (ordraw.LeavesQty_ == 0)
         ordraw.UpdateOrderSt_ = (ordraw.AfterQty_ == 0
                                  ? f9fmkt_OrderSt_UserCanceled
                                  : f9fmkt_OrderSt_PartCanceled);
      break;
   default: // 「新、查」不會 ReportPending.
      assert(!"TwsPendingReport: Unknown req.RxKind()");
      ordraw.Message_.assign("TwsPendingReport: Unknown RxKind.");
      break;
   }
   fon9_WARN_POP;
}
void OmsTwsReport::ProcessPendingReport(OmsResource& res) const {
   ProcessPendingReportFromOrderRaw(res, *this, this);
}
void OmsTwsRequestChg::ProcessPendingReport(OmsResource& res) const {
   ProcessPendingReportFromOrderRaw(res, *this, nullptr);
}

} // namespaces
