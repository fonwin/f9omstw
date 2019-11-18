// \file f9omstwf/OmsTwfReport3.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfReport3.hpp"
#include "f9omstwf/OmsTwfOrder1.hpp"
#include "f9omstwf/OmsTwfOrder9.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "f9omstw/OmsReportTools.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwfReport3::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwfReport3>(flds);
   base::AddFieldsForReport(flds);
   flds.Add(fon9_MakeField2(OmsTwfReport3, ExgTime));
   flds.Add(fon9_MakeField2(OmsTwfReport3, Side));
}
//--------------------------------------------------------------------------//
// 失敗單不用重算剩餘量.
static inline void OmsRecalcLeavesQtyFromReport(OmsReportRunnerInCore& inCoreRunner, OmsTwfOrderQtys& ordQtys) {
   (void)inCoreRunner; (void)ordQtys;
}
static inline void OmsRecalcLeavesQtyFromReport(OmsReportRunnerInCore& inCoreRunner, OmsTwfOrderRaw1& ordQtys) {
   (void)inCoreRunner; (void)ordQtys;
}

void OmsTwfReport3::RunReportInCore_FromOrig(OmsReportChecker&& checker, const OmsRequestBase& origReq) {
   // 可能為 OmsTwfRequestChg9; OmsTwfRequestChg1;
   // if (dynamic_cast<const OmsTwfRequestIni0*>(&origReq) == nullptr) {
   //    checker.ReportAbandon("TwfReport3: OrigRequest is not TwfRequest.");
   //    return;
   // }
   const OmsOrderRaw& lastupd = *origReq.LastUpdated();
   if (lastupd.RequestSt_ >= f9fmkt_TradingRequestSt_Done) {
   __ALREADY_DONE:;
      checker.ReportAbandon("TwfReport3: Duplicate or Obsolete report.");
      return;
   }
   if (this->RxKind() == f9fmkt_RxKind_Unknown)
      this->RxKind_ = origReq.RxKind();
   if (checker.CheckerSt_ != OmsReportCheckerSt::NotReceived) {
      // 不確定是否有收過此回報: 檢查是否重複回報.
      if (fon9_UNLIKELY(this->ReportSt() <= lastupd.RequestSt_))
         goto __ALREADY_DONE;
   }
   OmsOrder&               order = lastupd.Order();
   OmsReportRunnerInCore   inCoreRunner{std::move(checker), *order.BeginUpdate(origReq)};
   assert(dynamic_cast<OmsTwfOrderRaw0*>(&inCoreRunner.OrderRaw_) != nullptr);
   static_cast<OmsTwfOrderRaw0*>(&inCoreRunner.OrderRaw_)->LastExgTime_ = this->ExgTime_;
   if (OmsTwfOrderRaw1* ordraw1 = dynamic_cast<OmsTwfOrderRaw1*>(&inCoreRunner.OrderRaw_)) {
      if (this->RxKind_ == f9fmkt_RxKind_RequestNew)
         ordraw1->AfterQty_ = ordraw1->LeavesQty_ = 0;
      else
         OmsAssignQtysFromReportBfAf(inCoreRunner, *ordraw1, *this, ordraw1->LeavesQty_, ordraw1->LeavesQty_);
   }
   else if (OmsTwfOrderRaw9* ordraw9 = dynamic_cast<OmsTwfOrderRaw9*>(&inCoreRunner.OrderRaw_)) {
      ordraw9->QuoteReportSide_ = this->Side_;
      if (this->RxKind_ == f9fmkt_RxKind_RequestNew) {
         if (this->Side_ != f9fmkt_Side_Sell)
            ordraw9->Bid_.AfterQty_ = ordraw9->Bid_.LeavesQty_ = 0;
         if (this->Side_ != f9fmkt_Side_Buy)
            ordraw9->Offer_.AfterQty_ = ordraw9->Offer_.LeavesQty_ = 0;
      }
      else {
         #define kOmsTwfQty0  static_cast<OmsTwfQty>(0)
         if (this->Side_ != f9fmkt_Side_Sell)
            OmsAssignQtysFromReportBfAf(inCoreRunner, ordraw9->Bid_, *this, kOmsTwfQty0, kOmsTwfQty0);
         if (this->Side_ != f9fmkt_Side_Buy)
            OmsAssignQtysFromReportBfAf(inCoreRunner, ordraw9->Offer_, *this, kOmsTwfQty0, kOmsTwfQty0);
         OmsTwfMergeQuotePartReport(*ordraw9);
      }
   }
   inCoreRunner.UpdateReport(*this);
}
void OmsTwfReport3::RunReportInCore_Order(OmsReportChecker&& checker, OmsOrder& order) {
   // 用委託書號有找到委託, 則只處理新單的 R03;
   // - 刪改查的R03必須透過 ReqUID 尋找, 才能避免找錯「原始下單要求」.
   //   - 例如: 要求A:減1; 要求B:減2; 當收到沒有提供 ReqUID 的 R03 時, 到底是 A 失敗? 還是 B 失敗呢?
   // - 而且, 遺漏「刪改查」的R03, 對委託書的最後狀態影響不大.
   // - 所以這裡只考慮新單的R03, 不再尋找「刪改查」的「原始下單要求」.
   if (this->RxKind() == f9fmkt_RxKind_RequestNew) {
      if (const auto* iniReq = order.Initiator()) {
         this->RunReportInCore_FromOrig(std::move(checker), *iniReq);
         return;
      }
   }
   checker.ReportAbandon("TwfReport3: request not found.");
}
void OmsTwfReport3::RunReportInCore_MakeReqUID() {
   // 因為僅在「原始下單要求」存在時才處理 R03,
   // => 所以 Report3 不會寫入 omslog, 也不會留在 OmsCore,
   // => 所以沒必要編製 ReqUID.
   // this->MakeReportReqUID(this->ExgTime_, 0);
}
void OmsTwfReport3::RunReportInCore_OrderNotFound(OmsReportChecker&& checker, OmsOrdNoMap& ordnoMap) {
   (void)ordnoMap;
   // R03 不考慮亂序回報, 所以若沒找到委託, 就放棄此次回報.
   // - 如果是新單的R03, 之後收到新單時, 則可透過查詢, 期交所會回「委託不存在」, 到時再將剩餘量清空即可.
   // - 如果是刪改查的R03, 則不影響委託的最後狀態,
   //   所以拋棄此次的亂序R03, 不會造成太大的問題,
   //   雖然刪改查的狀態可能仍在 Sending, 使用者必須自行判斷, 該次「刪改查」是失敗的.
   checker.ReportAbandon("TwfReport3: order not found.");
}
void OmsTwfReport3::ProcessPendingReport(OmsResource& res) const {
   (void)res;
   // R03本身不支援亂序, 所以不可能執行到這兒.
   // 但被 R03 處理的 Request(OmsTwfRequestChg9,OmsTwfRequestChg1) 有可能會有 ReportPending.
   // 交給 Request::ProcessPendingReport() 處理.
   assert("OmsTwfReport3::ProcessPendingReport() Not impl.");
}

} // namespaces
