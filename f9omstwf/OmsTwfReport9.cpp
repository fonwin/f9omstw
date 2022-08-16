// \file f9omstwf/OmsTwfReport9.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfReport9.hpp"
#include "f9omstwf/OmsTwfOrder9.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwfReport9::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields(flds);
   base::AddFieldsForReport(flds);
   flds.Add(fon9_MakeField2(OmsTwfReport9, ExgTime));
   flds.Add(fon9_MakeField2(OmsTwfReport9, Side));
   flds.Add(fon9_MakeField2(OmsTwfReport9, BidBeforeQty));
   flds.Add(fon9_MakeField2(OmsTwfReport9, OfferBeforeQty));
   flds.Add(fon9_MakeField2(OmsTwfReport9, OutPvcId));
}
//--------------------------------------------------------------------------//
static inline void OmsAssignLastPrisFromReport(OmsTwfOrderRaw9* ordraw, const OmsTwfReport9* rpt) {
   if (ordraw->UpdateOrderSt_ == f9fmkt_OrderSt_ReportStale
       || f9fmkt_TradingRequestSt_IsAnyRejected(rpt->ReportSt()))
      return;
   if (!ordraw->LastPriTime_.IsNull() && rpt->RxKind() != f9fmkt_RxKind_RequestNew) {
      if (ordraw->LastPriTime_ >= rpt->ExgTime_)
         return;
      if (rpt->BidPri_.IsNullOrZero() && rpt->OfferPri_.IsNullOrZero())
         return;
      if (ordraw->LastBidPri_ == rpt->BidPri_ && ordraw->LastOfferPri_ == rpt->OfferPri_)
         return;
   }
   // rpt 是有填價格的成功回報(新刪改查) => 一律將 Order.LastPri* 更新成交易所的最後價格.
   ordraw->LastPriTime_ = rpt->ExgTime_;
   if (!rpt->BidPri_.IsNullOrZero())
      ordraw->LastBidPri_ = rpt->BidPri_;
   if (!rpt->OfferPri_.IsNullOrZero())
      ordraw->LastOfferPri_ = rpt->OfferPri_;
}
// OmsAssignQtysFromReportDCQ()=>OmsAssignQtysFromReportBfAf()=>若為改價則來到此處:
static inline bool OmsCheckReportChgPriStale(OmsReportRunnerInCore* inCoreRunner, OmsTwfOrderQtys* ordPris, OmsTwfReport9* rpt) {
   (void)ordPris;
   OmsTwfOrderRaw9&  ordraw = inCoreRunner->OrderRawT<OmsTwfOrderRaw9>();
   if (!ordraw.LastPriTime_.IsNull() && ordraw.LastPriTime_ >= rpt->ExgTime_)
      ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_ReportStale;
   return true;
}

static inline bool AdjustReportPrice(int32_t priMul, OmsTwfReport9& rpt) {
   switch (priMul) {
   case 0:  return true;
   case -1: return false;
   default:
      assert(priMul > 0);
      rpt.BidPri_ *= priMul;
      rpt.OfferPri_ *= priMul;
      return true;
   }
}
static inline void AdjustRequestSt_DoNothing(OmsTwfOrderRaw9& ordraw, OmsTwfReport9& rpt) {
   (void)ordraw; (void)rpt;
   // 不必額外處理 RequestSt_; 因為:
   // - 新單回報/刪改查回報.
   //   - 雙邊一次回報, 不用調整 RequestSt_;
   //   - 首次單邊, 由回報接收端填入 f9fmkt_TradingRequestSt_PartExchangeAccepted;
   //   - 最後單邊, 由回報接收端填入 f9fmkt_TradingRequestSt_ExchangeAccepted;
   // - 自動刪單回報.
   //   - 雙邊一次回報, 不用調整 RequestSt_;
   //   - 首次單邊, 由回報接收端填入 f9fmkt_TradingRequestSt_PartExchangeCanceled;
   //   - 最後單邊, 由回報接收端填入 f9fmkt_TradingRequestSt_ExchangeCanceled;
}
static inline void OmsAssignOrderFromReportNewOrig(OmsTwfOrderRaw9& ordraw, OmsTwfReport9& rpt) {
   ordraw.QuoteReportSide_ = rpt.Side_;
   if (rpt.Side_ != f9fmkt_Side_Sell) // rpt.Side_ = Buy or Unknown(Bid+Offer).
      ordraw.Bid_.AfterQty_ = ordraw.Bid_.LeavesQty_ = rpt.BidQty_;
   if (rpt.Side_ != f9fmkt_Side_Buy) // rpt.Side_ = Sell or Unknown(Bid+Offer).
      ordraw.Offer_.AfterQty_ = ordraw.Offer_.LeavesQty_ = rpt.OfferQty_;
   AdjustRequestSt_DoNothing(ordraw, rpt);
}
static bool OmsAssignQtysFromReportDCQ(OmsReportRunnerInCore& inCoreRunner, OmsTwfOrderRaw9& ordraw, OmsTwfReport9& rpt) {
   ordraw.QuoteReportSide_ = rpt.Side_;
   if (rpt.Side_ != f9fmkt_Side_Sell) // rpt.Side_ = Buy or Unknown(Bid+Offer).
      if (!OmsAssignQtysFromReportBfAf(inCoreRunner, ordraw.Bid_, rpt, rpt.BidBeforeQty_, rpt.BidQty_))
         return false;
   if (rpt.Side_ != f9fmkt_Side_Buy) // rpt.Side_ = Sell or Unknown(Bid+Offer).
      if (!OmsAssignQtysFromReportBfAf(inCoreRunner, ordraw.Offer_, rpt, rpt.OfferBeforeQty_, rpt.OfferQty_))
         return false;
   AdjustRequestSt_DoNothing(ordraw, rpt);
   if (ordraw.Request().RxKind() == rpt.RxKind()) {
      // RxKind 不同: rpt 必定是期交所刪單 => 則不用考慮 ReportPending 的合併!
      // 所以只有 RxKind 相同才需要考慮 ReportPending 的合併!
      OmsTwfMergeQuotePartReport(ordraw);
   }
   return true;
}
static void OmsUpdateOrderSt_WhenRecalcLeaves0(OmsReportRunnerInCore& inCoreRunner, OmsTwfOrderQtys& ordQtys) {
   (void)ordQtys;
   auto& ordraw9 = inCoreRunner.OrderRawT<OmsTwfOrderRaw9>();
   assert(&ordraw9.Offer_ == &ordQtys || &ordraw9.Bid_ == &ordQtys);
   OmsUpdateOrderSt_WhenRecalcLeaves0_Quote(inCoreRunner, ordraw9);
   if (ordraw9.RequestSt_ == f9fmkt_TradingRequestSt_ExchangeCanceled
       && ordraw9.UpdateOrderSt_ == f9fmkt_OrderSt_UserCanceled)
      ordraw9.UpdateOrderSt_ = f9fmkt_OrderSt_ExchangeCanceled;
}
//--------------------------------------------------------------------------//
void OmsTwfReport9::RunReportInCore_FromOrig(OmsReportChecker&& checker, const OmsRequestBase& origReq) {
   if (fon9_UNLIKELY(this->RxKind_ == f9fmkt_RxKind_RequestDelete)) {
      if (fon9_UNLIKELY(origReq.RxKind() != f9fmkt_RxKind_RequestDelete)) {
         // 交易所主動刪單: [報價時間到] or [有新報價時,取消舊報價]
         this->SetReportSt(this->ReportSt() == f9fmkt_TradingRequestSt_PartExchangeAccepted
                           ? f9fmkt_TradingRequestSt_PartExchangeCanceled
                           : f9fmkt_TradingRequestSt_ExchangeCanceled);
         if (this->ReportSt() == origReq.LastUpdated()->RequestSt_
             || origReq.LastUpdated()->RequestSt_ == f9fmkt_TradingRequestSt_ExchangeCanceled) {
            checker.ReportAbandon("TwfReport9: Duplicate ExchangeCanceled.");
            return;
         }
         // 如果最後有改價, 則 this->RxKind_ 應維持 Delete,
         // 這樣才能在 OmsAssignQtysFromReportBfAf() 正確處理 Qty;
         if ((this->RxKind_ = origReq.RxKind()) == f9fmkt_RxKind_RequestChgPri) {
            this->RxKind_ = f9fmkt_RxKind_RequestDelete;
         }
         goto __RUN_REPORT;
      }
   }
   if (fon9_LIKELY(this->RunReportInCore_FromOrig_Precheck(checker, origReq))) {
   __RUN_REPORT:;
      OmsOrder& order = origReq.LastUpdated()->Order();
      if (!AdjustReportPrice(OmsGetReportPriMul(order, checker, *this), *this))
         return;
      OmsOrderRaw& ordraw = *order.BeginUpdate(origReq);
      assert(dynamic_cast<OmsTwfOrderRaw9*>(&ordraw) != nullptr);
      OmsRunReportInCore_FromOrig(std::move(checker), *static_cast<OmsTwfOrderRaw9*>(&ordraw), *this);
   }
}
//--------------------------------------------------------------------------//
void OmsTwfReport9::RunReportInCore_MakeReqUID() {
   switch (this->Side_) {
   case f9fmkt_Side_Buy:
      this->MakeReportReqUID(this->ExgTime_, this->BidBeforeQty_ * 10 + 0);
      break;
   case f9fmkt_Side_Sell:
      this->MakeReportReqUID(this->ExgTime_, this->OfferBeforeQty_ * 10 + 1);
      break;
   case f9fmkt_Side_Unknown:
      this->MakeReportReqUID(this->ExgTime_, this->BidBeforeQty_ * 1000 + this->OfferBeforeQty_);
      break;
   }
}
bool OmsTwfReport9::RunReportInCore_IsBfAfMatch(const OmsOrderRaw& ordu) {
   assert(dynamic_cast<const OmsTwfOrderRaw9*>(&ordu) != nullptr);
   const OmsTwfOrderRaw9& ordraw = *static_cast<const OmsTwfOrderRaw9*>(&ordu);
   if (this->Side_ != f9fmkt_Side_Sell) { // this->Side_ = Buy or Unknown(Bid+Offer).
      if (ordraw.Bid_.BeforeQty_ != this->BidBeforeQty_
          || ordraw.Bid_.AfterQty_ != this->BidQty_)
         return false;
   }
   if (this->Side_ != f9fmkt_Side_Buy) { // this->Side_ = Sell or Unknown(Bid+Offer).
      if (ordraw.Offer_.BeforeQty_ != this->OfferBeforeQty_
          || ordraw.Offer_.AfterQty_ != this->OfferQty_)
         return false;
   }
   return true;
}
bool OmsTwfReport9::RunReportInCore_IsExgTimeMatch(const OmsOrderRaw& ordu) {
   return static_cast<const OmsTwfOrderRaw9*>(&ordu)->LastExgTime_ == this->ExgTime_;
}
//--------------------------------------------------------------------------//
void OmsTwfReport9::RunReportInCore_Order(OmsReportChecker&& checker, OmsOrder& order) {
   if (AdjustReportPrice(OmsGetReportPriMul(order, checker, *this), *this)) {
      this->PriStyle_ = OmsReportPriStyle::HasDecimal; // 避免重複計算小數位.
      base::RunReportInCore_Order(std::move(checker), order);
   }
}
void OmsTwfReport9::RunReportInCore_OrderNotFound(OmsReportChecker&& checker, OmsOrdNoMap& ordnoMap) {
   if (AdjustReportPrice(OmsGetReportPriMul(checker, *this), *this)) {
      this->PriStyle_ = OmsReportPriStyle::HasDecimal; // 避免重複計算小數位.
      base::RunReportInCore_OrderNotFound(std::move(checker), ordnoMap);
   }
}
void OmsTwfReport9::RunReportInCore_InitiatorNew(OmsReportRunnerInCore&& inCoreRunner) {
   OmsTwfOrderRaw9&  ordraw = inCoreRunner.OrderRawT<OmsTwfOrderRaw9>();
   OmsAssignOrderFromReportNewOrig(ordraw, *this);
   OmsRunReportInCore_InitiatorNew(std::move(inCoreRunner), ordraw, *this);
}
void OmsTwfReport9::RunReportInCore_DCQ(OmsReportRunnerInCore&& inCoreRunner) {
   assert(this->RxKind() != f9fmkt_RxKind_Unknown);
   OmsTwfOrderRaw9&  ordraw = inCoreRunner.OrderRawT<OmsTwfOrderRaw9>();
   OmsRunReportInCore_DCQ(std::move(inCoreRunner), ordraw, *this);
}
void OmsTwfReport9::OnSynReport(const OmsRequestBase* ref, fon9::StrView message) {
   base::OnSynReport(ref, message);
   this->Message_.assign(message);
   this->PriStyle_ = OmsReportPriStyle::HasDecimal;
}
//--------------------------------------------------------------------------//
static void OmsTwf9_ProcessPendingReport(const OmsRequestRunnerInCore& prevRunner, const OmsRequestBase& rpt, const OmsTwfReport9* chkFields) {
   assert(dynamic_cast<const OmsTwfOrderRaw9*>(rpt.LastUpdated()) != nullptr);
   const OmsTwfOrderRaw9&  rptraw = *static_cast<const OmsTwfOrderRaw9*>(rpt.LastUpdated());
   OmsOrder&               order = rpt.LastUpdated()->Order();
   if (order.LastOrderSt() < f9fmkt_OrderSt_NewDone)
      return;
   const OmsTwfOrderRaw9&  ordlast = *static_cast<const OmsTwfOrderRaw9*>(order.Tail());
   if (rptraw.QuoteReportSide_ != f9fmkt_Side_Sell) { // rpt.Side_ = Buy or Unknown(Bid+Offer).
      if (!OmsIsReadyForPendingReport(ordlast.Bid_, rptraw.Bid_, rptraw))
         return;
   }
   if (rptraw.QuoteReportSide_ != f9fmkt_Side_Buy) { // rpt.Side_ = Sell or Unknown(Bid+Offer).
      if (!OmsIsReadyForPendingReport(ordlast.Offer_, rptraw.Offer_, rptraw))
         return;
   }
   OmsReportRunnerInCore  inCoreRunner{prevRunner, *order.BeginUpdate(rpt)};
   OmsTwfOrderRaw9&       ordraw = inCoreRunner.OrderRawT<OmsTwfOrderRaw9>();
   if (chkFields) {
      if (auto ini = dynamic_cast<const OmsTwfRequestIni9*>(order.Initiator())) {
         if (fon9_UNLIKELY(!OmsIsReportFieldsMatch(*ini, *chkFields))) {
            // chk 欄位與新單回報不同.
            ordraw.ErrCode_ = OmsErrCode_FieldNotMatch;
            ordraw.RequestSt_ = f9fmkt_TradingRequestSt_InternalRejected;
            ordraw.Message_.assign("OmsTwf9_ProcessPendingReport: FieldNotMatch.");
            return;
         }
      }
   }

   ordraw.LastExgTime_ = rptraw.LastExgTime_;

   fon9_WARN_DISABLE_SWITCH;
   switch (rpt.RxKind()) {
   case f9fmkt_RxKind_RequestChgQty:
   case f9fmkt_RxKind_RequestDelete:
      break;
   default: // 「新、查」不會 ReportPending.
      ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_ReportStale;
      ordraw.Message_.assign("OmsTwf9_ProcessPendingReport: Unknown RxKind.");
      return;
   }
   fon9_WARN_POP;

   if (rptraw.QuoteReportSide_ != f9fmkt_Side_Sell) { // rpt.Side_ = Buy or Unknown(Bid+Offer).
      ordraw.Bid_.BeforeQty_ = rptraw.Bid_.BeforeQty_;
      ordraw.Bid_.AfterQty_  = rptraw.Bid_.AfterQty_;
      OmsRecalcLeavesQtyFromReport(inCoreRunner, ordraw.Bid_);
   }
   if (rptraw.QuoteReportSide_ != f9fmkt_Side_Buy) { // rpt.Side_ = Sell or Unknown(Bid+Offer).
      ordraw.Offer_.BeforeQty_ = rptraw.Offer_.BeforeQty_;
      ordraw.Offer_.AfterQty_  = rptraw.Offer_.AfterQty_;
      OmsRecalcLeavesQtyFromReport(inCoreRunner, ordraw.Offer_);
   }
}
void OmsTwfReport9::ProcessPendingReport(const OmsRequestRunnerInCore& prevRunner) const {
   OmsTwf9_ProcessPendingReport(prevRunner, *this, this);
}
void OmsTwfRequestChg9::ProcessPendingReport(const OmsRequestRunnerInCore& prevRunner) const {
   OmsTwf9_ProcessPendingReport(prevRunner, *this, nullptr);
}

} // namespaces
