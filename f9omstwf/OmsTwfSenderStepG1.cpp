// \file f9omstwf/OmsTwfTradingLineMgrG1.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfSenderStepG1.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsReportRunner.hpp"

namespace f9omstw {

TwfTradingLineMgrG1::TwfTradingLineMgrG1(OmsCoreMgr& coreMgr) : CoreMgr_(coreMgr) {
   coreMgr.TDayChangedEvent_.Subscribe(&this->SubrTDayChanged_,
      std::bind(&TwfTradingLineMgrG1::OnTDayChanged, this, std::placeholders::_1));
}
TwfTradingLineMgrG1::~TwfTradingLineMgrG1() {
   // CoreMgr 擁有 TwfTradingLineMgrG1, 所以當此時, CoreMgr_ 必定正在死亡!
   // 因此沒必要 this->CoreMgr_.TDayChangedEvent_.Unsubscribe(&this->SubrTDayChanged_);
}
void TwfTradingLineMgrG1::OnTDayChanged(OmsCore& core) {
   core.RunCoreTask([this](OmsResource& resource) {
      if (this->CoreMgr_.CurrentCore().get() == &resource.Core_) {
         for (unsigned L = 0; L < numofele(this->TradingLineMgr_); ++L) {
            assert(this->TradingLineMgr_[L].get() != nullptr);
            this->TradingLineMgr_[L]->OnOmsCoreChanged(resource);
         }
      }
   });
}
//--------------------------------------------------------------------------//
OmsTwfSenderStepG1::OmsTwfSenderStepG1(TwfTradingLineMgrG1& lineMgr)
   : LineMgr_(lineMgr) {
}
TwfTradingLineMgr* TwfTradingLineMgrG1::GetLineMgr(OmsRequestRunnerInCore& runner) const {
   unsigned idx;
   switch (runner.OrderRaw_.SessionId()) {
   case f9fmkt_TradingSessionId_AfterHour: idx = 1; break;
   case f9fmkt_TradingSessionId_Normal:    idx = 0; break;
   default:
      runner.Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_Bad_SessionId, nullptr);
      return nullptr;
   }
   switch (runner.OrderRaw_.Market()) {
   case f9fmkt_TradingMarket_TwFUT:
      idx = f9twf::ExgSystemTypeToIndex(idx ? f9twf::ExgSystemType::FutAfterHour : f9twf::ExgSystemType::FutNormal);
      break;
   case f9fmkt_TradingMarket_TwOPT:
      idx = f9twf::ExgSystemTypeToIndex(idx ? f9twf::ExgSystemType::OptAfterHour : f9twf::ExgSystemType::OptNormal);
      break;
   default:
      runner.Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_Bad_MarketId, nullptr);
      return nullptr;
   }
   return this->TradingLineMgr_[idx].get();
}
void OmsTwfSenderStepG1::RunRequest(OmsRequestRunnerInCore&& runner) {
   if (TwfTradingLineMgr* lmgr = this->LineMgr_.GetLineMgr(runner))
      lmgr->RunRequest(std::move(runner));
}
void OmsTwfSenderStepG1::RerunRequest(OmsReportRunnerInCore&& runner) {
   if (TwfTradingLineMgr* lmgr = this->LineMgr_.GetLineMgr(runner)) {
      if (runner.ErrCodeAct_->IsUseNewLine_) {
         if (auto* inireq = runner.OrderRaw_.Order().Initiator())
            lmgr->SelectPreferNextLine(*inireq);
      }
      lmgr->RunRequest(std::move(runner));
   }
}

} // namespaces
