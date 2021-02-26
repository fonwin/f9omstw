// \file f9omstws/OmsTwsTradingLineMgrG1.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsSenderStepG1.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "f9omstw/OmsEventSessionSt.hpp"

namespace f9omstw {

TwsTradingLineMgrG1::TwsTradingLineMgrG1(OmsCoreMgr& coreMgr) : CoreMgr_(coreMgr) {
   coreMgr.TDayChangedEvent_.Subscribe(&this->SubrTDayChanged_,
           std::bind(&TwsTradingLineMgrG1::OnTDayChanged, this, std::placeholders::_1));
   coreMgr.OmsEvent_.Subscribe(// &this->SubrOmsEvent_,
           std::bind(&TwsTradingLineMgrG1::OnOmsEvent, this, std::placeholders::_1, std::placeholders::_2));
}
TwsTradingLineMgrG1::~TwsTradingLineMgrG1() {
   // CoreMgr 擁有 TwsTradingLineMgrG1, 所以當 TwsTradingLineMgrG1 解構時, CoreMgr_ 必定正在死亡!
   // 因此沒必要 this->CoreMgr_.TDayChangedEvent_.Unsubscribe(&this->SubrTDayChanged_);
}
void TwsTradingLineMgrG1::OnTDayChanged(OmsCore& core) {
   core.RunCoreTask([this](OmsResource& resource) {
      if (this->CoreMgr_.CurrentCore().get() == &resource.Core_) {
         assert(this->TseTradingLineMgr_.get() != nullptr);
         assert(this->OtcTradingLineMgr_.get() != nullptr);
         this->TseTradingLineMgr_->OnOmsCoreChanged(resource);
         this->OtcTradingLineMgr_->OnOmsCoreChanged(resource);
      }
   });
}
void TwsTradingLineMgrG1::OnOmsEvent(OmsResource& res, const OmsEvent& omsEvent) {
   if (const OmsEventSessionSt* evSesSt = dynamic_cast<const OmsEventSessionSt*>(&omsEvent))
      this->SetTradingSessionSt(res.TDay(), evSesSt->Market(), evSesSt->SessionId(), evSesSt->SessionSt());
}
void TwsTradingLineMgrG1::SetTradingSessionSt(fon9::TimeStamp tday, f9fmkt_TradingMarket mkt, f9fmkt_TradingSessionId sesId, f9fmkt_TradingSessionSt sesSt) {
   if (mkt == f9fmkt_TradingMarket_TwSEC && this->TseTradingLineMgr_)
      this->TseTradingLineMgr_->SetTradingSessionSt(tday, sesId, sesSt);
   if (mkt == f9fmkt_TradingMarket_TwOTC && this->OtcTradingLineMgr_)
      this->OtcTradingLineMgr_->SetTradingSessionSt(tday, sesId, sesSt);
}
//--------------------------------------------------------------------------//
OmsTwsSenderStepG1::OmsTwsSenderStepG1(TwsTradingLineMgrG1& lineMgr)
   : LineMgr_(lineMgr) {
}
void OmsTwsSenderStepG1::RunRequest(OmsRequestRunnerInCore&& runner) {
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
void OmsTwsSenderStepG1::RerunRequest(OmsReportRunnerInCore&& runner) {
   if (0);// RerunRequest(): runner.ErrCodeAct_->IsUseNewLine_?
   this->RunRequest(std::move(runner));
}

} // namespaces
