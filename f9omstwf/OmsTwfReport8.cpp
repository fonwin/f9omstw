// \file f9omstwf/OmsTwfReport8.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfReport8.hpp"
#include "f9omstwf/OmsTwfOrder7.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwfReport8::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields(flds);
   base::AddFieldsForReport(flds);
   flds.Add(fon9_MakeField2(OmsTwfReport8, ExgTime));
   flds.Add(fon9_MakeField2(OmsTwfReport8, OutPvcId));
}
//--------------------------------------------------------------------------//
void OmsTwfReport8::RunReportInCore_FromOrig(OmsReportChecker&& checker, const OmsRequestBase& origReq) {
   if (dynamic_cast<const OmsTwfRequestIni7*>(&origReq) == nullptr) {
      checker.ReportAbandon("TwfReport7: OrigRequest is not QuoteR.");
      return;
   }
   if (origReq.LastUpdated()->RequestSt_ >= f9fmkt_TradingRequestSt_Done) {
   __ALREADY_DONE:;
      checker.ReportAbandon("TwfReport7: Duplicate or Obsolete report.");
      return;
   }
   if (this->RxKind() == f9fmkt_RxKind_Unknown)
      this->RxKind_ = origReq.RxKind();
   if (checker.CheckerSt_ != OmsReportCheckerSt::NotReceived) {
      // 不確定是否有收過此回報: 檢查是否重複回報.
      if (fon9_UNLIKELY(this->ReportSt() <= origReq.LastUpdated()->RequestSt_))
         goto __ALREADY_DONE;
   }
   OmsOrder&               order = origReq.LastUpdated()->Order();
   OmsReportRunnerInCore   inCoreRunner{std::move(checker), *order.BeginUpdate(origReq)};
   inCoreRunner.OrderRawT<OmsTwfOrderRaw7>().LastExgTime_ = this->ExgTime_;
   inCoreRunner.UpdateReport(*this);
}
void OmsTwfReport8::RunReportInCore_MakeReqUID() {
   this->MakeReportReqUID(this->ExgTime_, 0);
}
void OmsTwfReport8::RunReportInCore_Order(OmsReportChecker&& checker, OmsOrder& order) {
   assert(order.Head() != nullptr);
   if (auto origReq = dynamic_cast<const OmsTwfRequestIni7*>(order.Initiator()))
      this->RunReportInCore_FromOrig(std::move(checker), *origReq);
   else
      checker.ReportAbandon("TwfReport7: Order is not QuoteR.");
}
void OmsTwfReport8::RunReportInCore_NewOrder(OmsReportRunnerInCore&& runner) {
   OmsTwfOrderRaw7& ordraw = runner.OrderRawT<OmsTwfOrderRaw7>();
   runner.Resource_.Backend_.FetchSNO(*this);
   ordraw.OrdNo_ = this->OrdNo_;
   ordraw.LastExgTime_ = this->ExgTime_;
   runner.UpdateReport(*this);
}
OmsErrCode OmsTwfReport8::GetOkErrCode() const {
   if (this->RxKind() == f9fmkt_RxKind_RequestNew)
      return OmsErrCode_QuoteROK;
   return base::GetOkErrCode();
}

} // namespaces
