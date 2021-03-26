// \file f9omstwf/OmsTwfOrder9.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfOrder9.hpp"
#include "f9omstw/OmsErrCodeAct.hpp"
#include "f9omstw/OmsReportTools.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwfOrderRaw9::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields(flds);
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw9, LastFilledTime));
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw9, LastPriTime));
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw9, LastBidPri));
   flds.Add(fon9_MakeField2(OmsTwfOrderRaw9, LastOfferPri));
   flds.Add(fon9_MakeField(OmsTwfOrderRaw9, QuoteReportSide_, "ReportSide"));

   #define AddBidOfferFields(bo) \
   flds.Add(fon9_MakeField(OmsTwfOrderRaw9,  bo##_.BeforeQty_, #bo "BeforeQty")); \
   flds.Add(fon9_MakeField(OmsTwfOrderRaw9,  bo##_.AfterQty_,  #bo "AfterQty"));  \
   flds.Add(fon9_MakeField(OmsTwfOrderRaw9,  bo##_.LeavesQty_, #bo "LeavesQty")); \
   flds.Add(fon9_MakeField(OmsTwfOrderRaw9,  bo##_.CumQty_,    #bo "CumQty"));    \
   flds.Add(fon9_MakeField(OmsTwfOrderRaw9,  bo##_.CumAmt_,    #bo "CumAmt"));
   // -----
   AddBidOfferFields(Bid);
   AddBidOfferFields(Offer);
}

void OmsTwfOrderRaw9::ContinuePrevUpdate(const OmsOrderRaw& prev) {
   base::ContinuePrevUpdate(prev);
   OmsTwfOrderRawDat9::ContinuePrevUpdate(*static_cast<const OmsTwfOrderRaw9*>(&prev));
}
void OmsTwfOrderRaw9::OnOrderReject() {
   assert(f9fmkt_OrderSt_IsFinishedRejected(this->UpdateOrderSt_));
   this->Bid_.OnOrderReject();
   this->Offer_.OnOrderReject();
}

//--------------------------------------------------------------------------//

void OmsTwfMergeQuotePartReport(OmsTwfOrderRaw9& ordraw) {
   if (ordraw.QuoteReportSide_ == f9fmkt_Side_Unknown)
      return;
   const OmsOrderRaw* lastupd = ordraw.Request().LastUpdated();
   if (lastupd == nullptr || lastupd->QuoteReportSide_ == f9fmkt_Side_Unknown)
      return;
   assert(lastupd->QuoteReportSide_ != ordraw.QuoteReportSide_);
   if (lastupd->QuoteReportSide_ == ordraw.QuoteReportSide_)
      return;
   if (ordraw.UpdateOrderSt_ == f9fmkt_OrderSt_ReportPending) {
      if (lastupd->UpdateOrderSt_ == f9fmkt_OrderSt_ReportPending) {
         if (lastupd->QuoteReportSide_ != f9fmkt_Side_Sell)
            ordraw.Bid_ = static_cast<const OmsTwfOrderRaw9*>(lastupd)->Bid_;
         if (lastupd->QuoteReportSide_ != f9fmkt_Side_Buy)
            ordraw.Offer_ = static_cast<const OmsTwfOrderRaw9*>(lastupd)->Offer_;
         ordraw.QuoteReportSide_ = f9fmkt_Side_Unknown;
      }
   }
   else if (lastupd->UpdateOrderSt_ == f9fmkt_OrderSt_ReportPending) {
      // 前次異動(lastupd)有 ReportPending, 但這次異動(ordraw)沒有 ReportPending.
      // (1) 複製前次 Pending 的內容.
      // (2) 還原這次的 LeavesQty => 等之後 ProcessPendingReport() 時再處理.
      if (lastupd->QuoteReportSide_ != f9fmkt_Side_Sell) {
         ordraw.Bid_ = static_cast<const OmsTwfOrderRaw9*>(lastupd)->Bid_;
         OmsRestoreLeavesQtyFromReport(ordraw.Offer_);
      }
      if (lastupd->QuoteReportSide_ != f9fmkt_Side_Buy) {
         ordraw.Offer_ = static_cast<const OmsTwfOrderRaw9*>(lastupd)->Offer_;
         OmsRestoreLeavesQtyFromReport(ordraw.Bid_);
      }
      ordraw.QuoteReportSide_ = f9fmkt_Side_Unknown;
      ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_ReportPending;
   }
   // else if (f9fmkt_TradingRequestSt_IsPart(lastupd->RequestSt_)) {
   // }
   // else if (lastupd->RequestSt_ == f9fmkt_TradingRequestSt_ExchangeNoLeavesQty) {
   //    if (ordraw.RequestSt_ == f9fmkt_TradingRequestSt_ExchangeCanceled)
   //       ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_Canceled;
   // }
}

} // namespaces
