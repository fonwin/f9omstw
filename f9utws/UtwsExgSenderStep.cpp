// \file f9utws/UtwsExgSenderStep.cpp
// \author fonwinz@gmail.com
#include "f9utws/UtwsExgSenderStep.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsReportRunner.hpp"

namespace f9omstw {

UtwsExgTradingLineMgr::UtwsExgTradingLineMgr(OmsCoreMgr& coreMgr) : CoreMgr_(coreMgr) {
   coreMgr.TDayChangedEvent_.Subscribe(&this->SubrTDayChanged_,
      std::bind(&UtwsExgTradingLineMgr::OnTDayChanged, this, std::placeholders::_1));
}
UtwsExgTradingLineMgr::~UtwsExgTradingLineMgr() {
   // CoreMgr 擁有 UtwsExgTradingLineMgr, 所以當此時, CoreMgr_ 必定已經正在死亡!
   // 因此沒必要 this->CoreMgr_.TDayChangedEvent_.Unsubscribe(&this->SubrTDayChanged_);
}
void UtwsExgTradingLineMgr::OnTDayChanged(OmsCore& core) {
   core.RunCoreTask([this](OmsResource& resource) {
      if (this->CoreMgr_.CurrentCore().get() == &resource.Core_) {
         assert(this->TseTradingLineMgr_.get() != nullptr);
         assert(this->OtcTradingLineMgr_.get() != nullptr);
         this->TseTradingLineMgr_->OnOmsCoreChanged(resource);
         this->OtcTradingLineMgr_->OnOmsCoreChanged(resource);
      }
   });
}
//--------------------------------------------------------------------------//
UtwsExgSenderStep::UtwsExgSenderStep(UtwsExgTradingLineMgr& lineMgr)
   : LineMgr_(lineMgr) {
}
void UtwsExgSenderStep::RunRequest(OmsRequestRunnerInCore&& runner) {
   fon9_WARN_DISABLE_SWITCH;
   switch (runner.OrderRaw_.Market()) {
   case f9fmkt_TradingMarket_TwSEC:
      this->LineMgr_.TseTradingLineMgr_->RunRequest(std::move(runner));
      break;
   case f9fmkt_TradingMarket_TwOTC:
      this->LineMgr_.OtcTradingLineMgr_->RunRequest(std::move(runner));
      break;
   default:
      runner.Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_Bad_MarketId, nullptr);
      break;
   }
   fon9_WARN_POP;
}
void UtwsExgSenderStep::RerunRequest(OmsReportRunnerInCore&& runner) {
   if (0);// RerunRequest(): runner.ErrCodeAct_->IsUseNewLine_?
   this->RunRequest(std::move(runner));
}

} // namespaces
