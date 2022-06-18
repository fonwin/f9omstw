// \file f9omstws/OmsTwsSenderStepLgMgr.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsSenderStepLgMgr.hpp"
#include "f9omstw/OmsEventSessionSt.hpp"
#include "fon9/FilePath.hpp"

namespace f9omstw {

TwsTradingLgMgr::TwsTradingLgMgr(OmsCoreMgr& coreMgr, std::string name)
   : base(coreMgr, std::move(name)) {
   coreMgr.OmsSessionStEvent_.Subscribe(// &this->SubrOmsEvent_,
      std::bind(&TwsTradingLgMgr::OnOmsSessionStEvent, this, std::placeholders::_1, std::placeholders::_2));
}
TwsTradingLgMgr::~TwsTradingLgMgr() {
}
void TwsTradingLgMgr::OnOmsSessionStEvent(OmsResource& res, const OmsEventSessionSt& evSesSt) {
   this->SetTradingSessionSt(res.TDay(), evSesSt.Market(), evSesSt.SessionId(), evSesSt.SessionSt());
}
void TwsTradingLgMgr::SetTradingSessionSt(fon9::TimeStamp tday, f9fmkt_TradingMarket mkt, f9fmkt_TradingSessionId sesId, f9fmkt_TradingSessionSt sesSt) {
   for (auto& lgItem : this->LgItems_) {
      if (lgItem) {
         if (mkt == f9fmkt_TradingMarket_TwSEC)
            lgItem->TseTradingLineMgr_->SetTradingSessionSt(tday, sesId, sesSt);
         if (mkt == f9fmkt_TradingMarket_TwOTC)
            lgItem->OtcTradingLineMgr_->SetTradingSessionSt(tday, sesId, sesSt);
      }
   }
}
TwsTradingLgMgrSP TwsTradingLgMgr::Plant(OmsCoreMgr&                coreMgr,
                                         fon9::seed::PluginsHolder& holder,
                                         fon9::IoManagerArgs&       iocfgs) {
   fon9::ConfigFileBinder cfgb;
   TwsTradingLgMgrSP      retval = Make<TwsTradingLgMgr>(coreMgr, holder, iocfgs, "TwsTradingLgMgr", cfgb);
   if (!retval)
      return nullptr;

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
      if (fon9::iequals(tag, "IoTse"))
         argsIoTse.SetIoServiceCfg(holder, cfgln);
      else if (fon9::iequals(tag, "IoOtc"))
         argsIoOtc.SetIoServiceCfg(holder, cfgln);
      else if (auto lgItem = retval->MakeLgItem(tag, cfgln)) {
         fon9::seed::MaTree& mgrTree = *lgItem->Sapling_;
         argsIoTse.Name_.assign(lgItem->Name_ + "_TSE");
         argsIoTse.IoServiceSrc_
            = lgItem->TseTradingLineMgr_
            = CreateTwsTradingLineMgr(mgrTree, cfgpath, argsIoTse, f9fmkt_TradingMarket_TwSEC);

         if (fon9::iequals(&argsIoOtc.IoServiceCfgstr_, "IoTse"))
            argsIoOtc.IoServiceSrc_ = argsIoTse.IoServiceSrc_;
         argsIoOtc.Name_.assign(lgItem->Name_ + "_OTC");
         argsIoOtc.IoServiceSrc_
            = lgItem->OtcTradingLineMgr_
            = CreateTwsTradingLineMgr(mgrTree, cfgpath, argsIoOtc, f9fmkt_TradingMarket_TwOTC);
      }
   }
   return retval;
}

} // namespaces
