// \file f9omstw/OmsCxToCore.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCxToCore.hpp"
#include "f9omstw/OmsCxReqBase.hpp"
#include "f9omstw/OmsReportRunner.hpp"

namespace f9omstw {

OmsCxToCore::OmsCxToCore(unsigned reserve) {
   this->PendingReqList_.Lock()->reserve(reserve);
   this->ReqListInCore_.reserve(reserve);
}
OmsCxToCore::~OmsCxToCore() {
}
void OmsCxToCore::AddCondFiredReq(OmsCore& omsCore, const OmsRequestTrade& txReq, OmsRequestRunStep& nextStep, fon9::StrView logstr) {
   bool isNeedsRunCoreTask;
   {
      auto plist = this->PendingReqList_.Lock();
      isNeedsRunCoreTask = plist->empty();
      plist->emplace_back(txReq, nextStep, logstr);
   }
   if (isNeedsRunCoreTask) {
      // 到 OmsCore 執行 this->ToCoreReq_.RunReportInCore();
      // - 因為 OmsCore 處理 Request、Report 的優先權高於 OmsCore.RunCoreTask();
      // - 所以使用 this->ToCoreReq_ 來執行條件成立的下單要求, 才不會被其他 CoreTask 耽擱;
      OmsRequestRunner  runner;
      runner.Request_.reset(&this->ToCoreReq_);
      omsCore.MoveToCore(std::move(runner));
   }
}
void OmsCxToCore::ToCoreReq::RunReportInCore(OmsReportChecker&& checker) {
   OmsCxToCore& rthis = fon9::ContainerOf(*this, &OmsCxToCore::ToCoreReq_);
   rthis.ReqListInCore_.swap(*rthis.PendingReqList_.Lock());
   for (auto& prec : rthis.ReqListInCore_) {
      auto* txReq = prec.TxRequest_;
      if (GetWaitingOrdRaw(*txReq)) {
         auto& order = txReq->LastUpdated()->Order();
         OmsRequestRunnerInCore runner{checker.Resource_, *order.BeginUpdate(*txReq)};
         fon9::RevPrint(runner.ExLogForUpd_, prec.Log_, '\n');
         runner.OrderRaw().RequestSt_ = f9fmkt_TradingRequestSt_WaitingCond; // 告知下一步驟,此次執行是因為條件成立.
         prec.NextStep_->RunRequest(std::move(runner));
      }
   }
   rthis.ReqListInCore_.clear();
}

} // namespaces
