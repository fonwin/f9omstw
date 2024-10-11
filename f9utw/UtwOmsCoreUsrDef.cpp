// \file f9utw/UtwOmsCoreUsrDef.cpp
// \author fonwinz@gmail.com
#include "f9utw/UtwOmsCoreUsrDef.hpp"
#include "f9omstw/OmsRequestFactory.hpp"

namespace f9omstw {

UtwOmsCoreUsrDef::UtwOmsCoreUsrDef() {
}
UtwOmsCoreUsrDef::~UtwOmsCoreUsrDef() {
}
void UtwOmsCoreUsrDef::AddCondFiredReq(OmsCore& omsCore, const OmsCxBaseIniFn& cxReq, OmsRequestRunStep& runStep, fon9::StrView logstr) {
   bool isNeedsRunCoreTask;
   {
      auto plist = this->PendingReqList_.Lock();
      isNeedsRunCoreTask = plist->empty();
      plist->emplace_back(cxReq, runStep, logstr);
   }
   if (isNeedsRunCoreTask) {
      omsCore.RunCoreTask([](OmsResource& res) {
         UtwOmsCoreUsrDef*   pthis = static_cast<UtwOmsCoreUsrDef*>(res.UsrDef_.get());
         PendingReqListImpl  plist = std::move(*pthis->PendingReqList_.Lock());
         for (auto& prec : plist) {
            if (auto* txReq = prec.CxReq_->GetWaitingRequestTrade()) {
               auto& order = txReq->LastUpdated()->Order();
               OmsRequestRunnerInCore runner{res, *order.BeginUpdate(*txReq)};
               fon9::RevPrint(runner.ExLogForUpd_, prec.Log_, '\n');
               runner.OrderRaw().RequestSt_ = f9fmkt_TradingRequestSt_WaitingCond; // 告知下一步驟,此次執行是因為條件成立.
               prec.NextStep_->RunRequest(std::move(runner));
            }
         }
      });
   }
}

} // namespaces
