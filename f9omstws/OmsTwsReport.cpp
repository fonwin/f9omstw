// \file f9omstws/OmsTwsReport.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsReport.hpp"
#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwsFilled::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwsFilled>(flds);
   flds.Add(fon9_MakeField2(OmsTwsFilled, Time));
   flds.Add(fon9_MakeField2(OmsTwsFilled, Pri));
   flds.Add(fon9_MakeField2(OmsTwsFilled, Qty));
}
void OmsTwsFilled::RunReportInCore(OmsReportRunner&& runner) {
   runner.ReportAbandon("TwsFilled.RunReportInCore: Not impl.");
}
void OmsTwsFilled::ProcessPendingReport(OmsResource& res) const {
}
//--------------------------------------------------------------------------//
void OmsTwsReport::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields(flds);
   base::AddFieldsForReport(flds);
   flds.Add(fon9_MakeField2(OmsTwsReport, ExgTime));
   flds.Add(fon9_MakeField2(OmsTwsReport, BeforeQty));
}
void OmsTwsReport::RunReportInCore(OmsReportRunner&& runner) {
   assert(runner.Report_.get() == this);
   const OmsRequestBase* origReq = runner.SearchOrigRequestId();
   OmsOrder*             order;
   if (fon9_UNLIKELY(origReq == nullptr)) {
      // runner.SearchOrigRequestId(); 失敗, 已呼叫過 runner.ReportAbandon(), 所以直接結束.
      if (runner.RunnerSt_ == OmsReportRunnerSt::Abandoned)
         return;

      // ReqUID 找不到原下單要求, 使用 OrdKey 來找.
      if (this->OrdNo_.empty1st()) {
         if (this->RxKind() == f9fmkt_RxKind_RequestNew) {
            // TODO: this 沒有委託書號, 如何取得 origReq? 如何判斷重複?
            //       - 建立 ReqUID map?
            //    or - f9oms 首次異動就必須提供 OrdNo (即使是 Queuing), 那 Abandon request 呢(不匯入?)?
            //         沒有提供 OrdNo 就拋棄此次回報?
            if (0); runner.ReportAbandon("OmsTwsReport: OrdNo is empty?");
            return;
         }
         runner.ReportAbandon("OmsTwsReport: OrdNo is empty, but kind isnot 'N'");
         return;
      }
      OmsOrdNoMap* ordnoMap = runner.GetOrdNoMap();
      if (ordnoMap == nullptr) // runner.GetOrdNoMap(); 失敗.
         return;               // 已呼叫過 runner.ReportAbandon(), 所以直接結束.
      if ((order = ordnoMap->GetOrder(this->OrdNo_)) != nullptr) {
         // ----- 委託已存在 -----
         assert(order->Head() != nullptr);
         const OmsOrderRaw* ordu = order->Head();
         do {
            if (ordu->Request_->ReqUID_ == this->ReqUID_) {
               // 透過 OrdKey 找到了 this 要回報的 origReq.
               origReq = ordu->Request_;
               goto __ORIG_REQ_FOUND;
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
               ordraw.ErrCode_ = this->AbandonErrCode();
               ordraw.Message_ = this->Message_;
               inCoreRunner.Update(this->ReportSt());
               ordnoMap->EmplaceOrder(ordraw);
               return;
            }
            else {
               // 委託不存在的刪改查回報: 
            }
         }
         else {
            runner.ReportAbandon("OmsTwsReport: No OrderFactory.");
            return;
         }
      }
      if (0); runner.ReportAbandon("Search orig order by OrdKey: Not Impl.");
      return;
   }
   // -------------------------------------------------------------------------
   order = origReq->LastUpdated()->Order_;
__ORIG_REQ_FOUND:
   // 有找到 origReq => origReq 的後續回報.
   if (fon9_UNLIKELY(this->ReportSt() <= origReq->LastUpdated()->RequestSt_)) {
      runner.ReportAbandon("Duplicate or Obsolete report for TwsReport.");
      return;
   }
   if (fon9_LIKELY(this->RxKind() == f9fmkt_RxKind_RequestNew)) {
   }
   else { // this 可能是其他 f9oms 送來的「使用 OmsTwsRequestIni 的刪改查」回報.
      if (runner.RunnerSt_ != OmsReportRunnerSt::NotReceived) {
         if (0); // 檢查是否重複回報.
                 // 下單流程的回報: 不影響委託內容, 直接更新即可.
                 // 只有「流程結束的回報」才有可能「暫時保留」
      }
   }
   OmsRequestRunnerInCore  inCoreRunner{std::move(runner), *order->BeginUpdate(*origReq)};
   OmsTwsOrderRaw&         ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
   f9fmkt_TradingRequestSt rptst = this->ReportSt();
   if (fon9_LIKELY(rptst > f9fmkt_TradingRequestSt_LastRunStep)) {
      if (!this->ExgTime_.IsNull())
         ordraw.LastExgTime_ = this->ExgTime_;
      if (fon9_LIKELY(this->RxKind() == f9fmkt_RxKind_RequestNew))
         ordraw.AfterQty_ = ordraw.LeavesQty_ = this->Qty_;
      else if (ordraw.OrderSt_ < f9fmkt_OrderSt_NewDone) {
         // 尚未收到新單之前, 是否要過濾重複回報?
         ordraw.OrderSt_ = f9fmkt_OrderSt_ReportPending;
         ordraw.BeforeQty_ = this->BeforeQty_;
         ordraw.AfterQty_ = this->Qty_;
         ordraw.LastPri_ = this->Pri_;
         ordraw.LastPriType_ = this->PriType_;
      }
      else if (this->RxKind() == f9fmkt_RxKind_RequestChgPri) {
         assert(!this->ExgTime_.IsNull());
         if (f9fmkt_TradingRequestSt_IsRejected(rptst)) {
            if (0);// 改價失敗: 若因交易所LeavesQty==0, 但現在 Order.LeavesQty!=0, 則應保留 this 回報.
         }
         else if (ordraw.LastPriTime_.IsNull() || ordraw.LastPriTime_ <= this->ExgTime_) {
            ordraw.LastPriTime_ = this->ExgTime_;
            ordraw.LastPriType_ = this->PriType_;
            ordraw.LastPri_ = this->Pri_;
         }
         else { // 此次的改價回報 = 過時的回報 => 相關欄位不異動.
            ordraw.OrderSt_ = f9fmkt_OrderSt_ReportStale;
         }
      }
      else if (this->RxKind() == f9fmkt_RxKind_RequestQuery) {
         if (0);// TODO: 檢查查詢結果是否 stale?
         ordraw.BeforeQty_ = ordraw.AfterQty_ = this->Qty_;
      }
      else { // 刪減.
         if (f9fmkt_TradingRequestSt_IsRejected(rptst)) {
            if (0);// TODO: 如果是剩餘0的刪減失敗, 應保留 this 回報.
         }
         else {
            ordraw.BeforeQty_ = this->BeforeQty_;
            ordraw.AfterQty_ = this->Qty_;
            ordraw.LeavesQty_ = ordraw.LeavesQty_ + ordraw.AfterQty_ - ordraw.BeforeQty_;
            if (fon9_UNLIKELY(fon9::signed_cast(ordraw.LeavesQty_) < 0)) {
               fon9::RevPrint(inCoreRunner.ExLogForUpd_, "Bad.Leaves=", fon9::signed_cast(ordraw.LeavesQty_),
                              fon9_kCSTR_ROWSPL fon9_kCSTR_CELLSPL);
               ordraw.LeavesQty_ = 0;
            }
            if (ordraw.AfterQty_ == 0 && ordraw.LeavesQty_ != 0)
               ordraw.OrderSt_ = f9fmkt_OrderSt_ReportPending;
         }
      }
   }
   else {
      // this->ReportSt() <= f9fmkt_TradingRequestSt_LastRunStep
      // 下單過程(Queuing,Sending)的回報: 直接更新 RequestSt 即可, 不影響 Order 最後內容.
   }
   ordraw.ErrCode_ = this->AbandonErrCode();
   if (ordraw.Request_ == this)
      ordraw.Message_ = this->Message_;
   else // ordraw 異動的是已存在的 request, this 即將被刪除, 所以使用 std::move(this->Message_)
      ordraw.Message_ = std::move(this->Message_);
   if (ordraw.OrderSt_ == f9fmkt_OrderSt_ReportStale)
      ordraw.Message_.append("(stale)");
   inCoreRunner.Update(rptst);
}
//--------------------------------------------------------------------------//
void OmsTwsReport::ProcessPendingReport(OmsResource& res) const {
}
void OmsTwsRequestChg::ProcessPendingReport(OmsResource& res) const {
}

} // namespaces
