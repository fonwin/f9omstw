// \file f9omstws/OmsTwsTradingLineMgrLg.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsSenderStepLg.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "f9omstw/OmsEventSessionSt.hpp"
#include "fon9/ConfigFileBinder.hpp"
#include "fon9/FilePath.hpp"

namespace f9omstw {

TwsTradingLineMgrLg::TwsTradingLineMgrLg(OmsCoreMgr& coreMgr, std::string name)
   : base(std::move(name))
   , CoreMgr_(coreMgr) {
   coreMgr.TDayChangedEvent_.Subscribe(//&this->SubrTDayChanged_,
      std::bind(&TwsTradingLineMgrLg::OnTDayChanged, this, std::placeholders::_1));
   coreMgr.OmsSessionStEvent_.Subscribe(// &this->SubrOmsEvent_,
      std::bind(&TwsTradingLineMgrLg::OnOmsSessionStEvent, this, std::placeholders::_1, std::placeholders::_2));
}
TwsTradingLineMgrLg::~TwsTradingLineMgrLg() {
   // CoreMgr 擁有 TwsTradingLineMgrLg, 所以當 TwsTradingLineMgrLg 解構時, CoreMgr_ 必定正在死亡!
   // 因此沒必要 this->CoreMgr_.TDayChangedEvent_.Unsubscribe(&this->SubrTDayChanged_);
   // this->CoreMgr_.OmsSessionStEvent_.Unsubscribe(&this->SubrOmsEvent_);
}
void TwsTradingLineMgrLg::OnTDayChanged(OmsCore& core) {
   core.RunCoreTask([this](OmsResource& resource) {
      if (this->CoreMgr_.CurrentCore().get() != &resource.Core_)
         return;
      for (LgMgrSP& lgMgr : this->LgMgrs_) {
         if (lgMgr) {
            lgMgr->TseTradingLineMgr_->OnOmsCoreChanged(resource);
            lgMgr->OtcTradingLineMgr_->OnOmsCoreChanged(resource);
         }
      }
   });
}
void TwsTradingLineMgrLg::OnOmsSessionStEvent(OmsResource& res, const OmsEventSessionSt& evSesSt) {
   this->SetTradingSessionSt(res.TDay(), evSesSt.Market(), evSesSt.SessionId(), evSesSt.SessionSt());
}
void TwsTradingLineMgrLg::SetTradingSessionSt(fon9::TimeStamp tday, f9fmkt_TradingMarket mkt, f9fmkt_TradingSessionId sesId, f9fmkt_TradingSessionSt sesSt) {
   for (LgMgrSP& lgMgr : this->LgMgrs_) {
      if (lgMgr) {
         if (mkt == f9fmkt_TradingMarket_TwSEC)
            lgMgr->TseTradingLineMgr_->SetTradingSessionSt(tday, sesId, sesSt);
         if (mkt == f9fmkt_TradingMarket_TwOTC)
            lgMgr->OtcTradingLineMgr_->SetTradingSessionSt(tday, sesId, sesSt);
      }
   }
}
TwsTradingLineMgrLgSP TwsTradingLineMgrLg::Plant(OmsCoreMgr&                coreMgr,
                                                 fon9::seed::PluginsHolder& holder,
                                                 fon9::IoManagerArgs&       iocfgs) {
   fon9::ConfigFileBinder cfgb;
   std::string errmsg = cfgb.OpenRead(nullptr, iocfgs.CfgFileName_, false/*isAutoBackup*/);
   if (!errmsg.empty()) {
      holder.SetPluginsSt(fon9::LogLevel::Error, "TwsTradingLineMgrLg.", iocfgs.Name_, '|', errmsg);
      return nullptr;
   }
   TwsTradingLineMgrLgSP retval{new TwsTradingLineMgrLg(coreMgr, iocfgs.Name_)};
   if (!coreMgr.Add(retval, "TwsTradingLineMgrLg")) {
      holder.SetPluginsSt(fon9::LogLevel::Error, "TwsTradingLineMgrLg.", iocfgs.Name_, "|err=AddTo CoreMgr");
      return nullptr;
   }
   fon9::IoManagerArgs  argsIoTse, argsIoOtc;
   argsIoTse.DeviceFactoryPark_ = argsIoOtc.DeviceFactoryPark_ = iocfgs.DeviceFactoryPark_;
   argsIoTse.SessionFactoryPark_ = argsIoOtc.SessionFactoryPark_ = iocfgs.SessionFactoryPark_;

   fon9::StrView        cfgstr{&cfgb.GetConfigStr()};
   const std::string    cfgpath = fon9::FilePath::ExtractPathName(&iocfgs.CfgFileName_);
   while (!fon9::StrTrimHead(&cfgstr).empty()) {
      fon9::StrView cfgln = fon9::StrFetchNoTrim(cfgstr, '\n');
      if (cfgln.Get1st() == '#')
         continue;
      fon9::StrView tag = fon9::StrFetchTrim(cfgln, '=');
      if (tag == "IoTse")
         argsIoTse.SetIoServiceCfg(holder, cfgln);
      else if (tag == "IoOtc")
         argsIoOtc.SetIoServiceCfg(holder, cfgln);

      #define kCSTR_LgTAG_LEAD   "Lg"
      #define kSIZE_LgTAG_LEAD   (sizeof(kCSTR_LgTAG_LEAD) - 1)
      else if (tag.size() == kSIZE_LgTAG_LEAD + 1) {
         const char* ptag = tag.begin();
         if (memcmp(ptag, kCSTR_LgTAG_LEAD, kSIZE_LgTAG_LEAD) != 0)
            // Unknown tag.
            continue;

         const LgOut lgId = static_cast<LgOut>(ptag[kSIZE_LgTAG_LEAD]);
         if (!IsValidateLgOut(lgId))
            // Unknown LgId.
            continue;

         const uint8_t idx = LgOutToIndex(lgId);
         if (retval->LgMgrs_[idx].get() != nullptr)
            // Dup Lg.
            continue;

         fon9::StrView  title = fon9::SbrFetchNoTrim(cfgln, ',');
         LgMgrSP  lgMgr{new LgMgr{fon9::Named(
            tag.ToString(),
            fon9::StrView_ToNormalizeStr(fon9::StrTrimRemoveQuotes(title)),
            fon9::StrView_ToNormalizeStr(fon9::StrTrimRemoveQuotes(cfgln))
            )}};
         retval->Sapling_->Add(lgMgr);

         fon9::seed::MaTree& mgrTree = *lgMgr->Sapling_;
         argsIoTse.Name_.assign(lgMgr->Name_ + "_TSE");
         lgMgr->TseTradingLineMgr_ = CreateTwsTradingLineMgr(mgrTree, cfgpath, argsIoTse, f9fmkt_TradingMarket_TwSEC);
         argsIoTse.IoServiceSrc_ = lgMgr->TseTradingLineMgr_;

         argsIoOtc.Name_.assign(lgMgr->Name_ + "_OTC");
         if (argsIoOtc.IoServiceCfgstr_ == "IoTse")
            argsIoOtc.IoServiceSrc_ = argsIoTse.IoServiceSrc_;
         lgMgr->OtcTradingLineMgr_ = CreateTwsTradingLineMgr(mgrTree, cfgpath, argsIoOtc, f9fmkt_TradingMarket_TwOTC);
         argsIoOtc.IoServiceSrc_ = lgMgr->OtcTradingLineMgr_;

         retval->LgMgrs_[idx] = std::move(lgMgr);
      }
   }
   return retval;
}
//--------------------------------------------------------------------------//
OmsTwsSenderStepLg::OmsTwsSenderStepLg(TwsTradingLineMgrLg& lineMgr)
   : LineMgr_(lineMgr) {
}
//--------------------------------------------------------------------------//
TwsTradingLineMgr* TwsTradingLineMgrLg::GetLineMgr(OmsRequestRunnerInCore& runner) const {
   assert(dynamic_cast<const OmsRequestTrade*>(&runner.OrderRaw_.Request()) != nullptr);
   if (auto* lgMgr = this->GetLgMgr(static_cast<const OmsRequestTrade*>(&runner.OrderRaw_.Request())->LgOut_)) {
      fon9_WARN_DISABLE_SWITCH;
      switch (runner.OrderRaw_.Market()) {
      case f9fmkt_TradingMarket_TwSEC:
         return lgMgr->TseTradingLineMgr_.get();
      case f9fmkt_TradingMarket_TwOTC:
         return lgMgr->OtcTradingLineMgr_.get();
      default:
         runner.Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_Bad_MarketId, nullptr);
         return nullptr;
      }
      fon9_WARN_POP;
   }
   runner.Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_Bad_LgOut, nullptr);
   return nullptr;
}
void OmsTwsSenderStepLg::RunRequest(OmsRequestRunnerInCore&& runner) {
   if (auto* lmgr = this->LineMgr_.GetLineMgr(runner))
      lmgr->RunRequest(std::move(runner));
}
void OmsTwsSenderStepLg::RerunRequest(OmsReportRunnerInCore&& runner) {
   if (TwsTradingLineMgr* lmgr = this->LineMgr_.GetLineMgr(runner)) {
      if (runner.ErrCodeAct_->IsUseNewLine_) {
         if (auto* inireq = runner.OrderRaw_.Order().Initiator())
            lmgr->SelectPreferNextLine(*inireq);
      }
      lmgr->RunRequest(std::move(runner));
   }
}

} // namespaces
