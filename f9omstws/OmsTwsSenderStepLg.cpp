// \file f9omstws/OmsTwsTradingLineMgrLg.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsSenderStepLg.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "fon9/ConfigFileBinder.hpp"
#include "fon9/FilePath.hpp"

namespace f9omstw {

TwsTradingLineMgrLg::LgMgr::LgMgr(fon9::Named&& named)
   : base(new fon9::seed::MaTree{"Lg"}, std::move(named)) {
}
//--------------------------------------------------------------------------//
TwsTradingLineMgrLg::TwsTradingLineMgrLg(OmsCoreMgr& coreMgr, std::string name)
   : base(new fon9::seed::MaTree{"LgList"}, std::move(name))
   , CoreMgr_(coreMgr) {
   coreMgr.TDayChangedEvent_.Subscribe(&this->SubrTDayChanged_,
      std::bind(&TwsTradingLineMgrLg::OnTDayChanged, this, std::placeholders::_1));
}
TwsTradingLineMgrLg::~TwsTradingLineMgrLg() {
   this->CoreMgr_.TDayChangedEvent_.Unsubscribe(&this->SubrTDayChanged_);
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
         static_cast<fon9::seed::MaTree*>(retval->Sapling_.get())->Add(lgMgr);

         fon9::seed::MaTree& mgrTree = *static_cast<fon9::seed::MaTree*>(lgMgr->Sapling_.get());
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
void OmsTwsSenderStepLg::RunRequest(OmsRequestRunnerInCore&& runner) {
   assert(dynamic_cast<const OmsRequestTrade*>(&runner.OrderRaw_.Request()) != nullptr);
   if (auto* lgMgr = this->LineMgr_.GetLgMgr(static_cast<const OmsRequestTrade*>(&runner.OrderRaw_.Request())->LgOut_)) {
      fon9_WARN_DISABLE_SWITCH;
      switch (runner.OrderRaw_.Market()) {
      case f9fmkt_TradingMarket_TwSEC:
         lgMgr->TseTradingLineMgr_->RunRequest(std::move(runner));
         return;
      case f9fmkt_TradingMarket_TwOTC:
         lgMgr->OtcTradingLineMgr_->RunRequest(std::move(runner));
         return;
      default:
         runner.Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_Bad_MarketId, nullptr);
         return;
      }
      fon9_WARN_POP;
   }
   runner.Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_Bad_LgOut, nullptr);
}
void OmsTwsSenderStepLg::RerunRequest(OmsReportRunnerInCore&& runner) {
   if (0);// RerunRequest(): runner.ErrCodeAct_->IsUseNewLine_?
   this->RunRequest(std::move(runner));
}

} // namespaces
