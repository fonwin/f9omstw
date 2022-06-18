// \file f9omstwf/OmsTwfSenderStepLgMgr.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfSenderStepLgMgr.hpp"
#include "fon9/ConfigFileBinder.hpp"
#include "fon9/FilePath.hpp"

namespace f9omstw {

TwfTradingLgMgr::TwfTradingLgMgr(OmsCoreMgr& coreMgr, std::string name)
   : base(coreMgr, std::move(name)) {
}
TwfTradingLgMgr::~TwfTradingLgMgr() {
}
TwfTradingLgMgrSP TwfTradingLgMgr::Plant(OmsCoreMgr&                coreMgr,
                                         fon9::seed::PluginsHolder& holder,
                                         fon9::IoManagerArgs&       iocfgs,
                                         f9twf::ExgMapMgrSP         exgMapMgr) {
   fon9::ConfigFileBinder cfgb;
   TwfTradingLgMgrSP      retval = Make<TwfTradingLgMgr>(coreMgr, holder, iocfgs, "TwfTradingLgMgr", cfgb);
   if (!retval)
      return nullptr;

   struct ArgsIo {
      fon9::IoManagerArgs  FutN_, OptN_, FutA_, OptA_;
      ArgsIo(fon9::IoManagerArgs& iocfgs) {
         this->FutN_.DeviceFactoryPark_ = this->OptN_.DeviceFactoryPark_ =
         this->FutA_.DeviceFactoryPark_ = this->OptA_.DeviceFactoryPark_ = iocfgs.DeviceFactoryPark_;
         this->FutN_.SessionFactoryPark_ = this->OptN_.SessionFactoryPark_ =
         this->FutA_.SessionFactoryPark_ = this->OptA_.SessionFactoryPark_ = iocfgs.SessionFactoryPark_;
      }
      void SetRef(fon9::StrView curName, fon9::IoManagerSP curIoMgr) {
         #define SET_REF_IoMgr(ioName)                               \
         if (fon9::iequals(&this->ioName.IoServiceCfgstr_, curName)) \
            this->ioName.IoServiceSrc_ = curIoMgr
         //-----
         SET_REF_IoMgr(FutN_);
         SET_REF_IoMgr(OptN_);
         SET_REF_IoMgr(FutA_);
         SET_REF_IoMgr(OptA_);
      }
   };
   ArgsIo            argsIo{iocfgs};
   fon9::StrView     cfgstr{&cfgb.GetConfigStr()};
   const std::string cfgpath = fon9::FilePath::ExtractPathName(&iocfgs.CfgFileName_);
   while (!fon9::StrTrimHead(&cfgstr).empty()) {
      fon9::StrView cfgln = fon9::StrFetchNoTrim(cfgstr, '\n');
      if (cfgln.Get1st() == '#')
         continue;
      fon9::StrView tag = fon9::StrFetchTrim(cfgln, '=');
      if (fon9::iequals(tag, "IoFutN"))
         argsIo.FutN_.SetIoServiceCfg(holder, cfgln);
      else if (fon9::iequals(tag, "IoOptN"))
         argsIo.OptN_.SetIoServiceCfg(holder, cfgln);
      else if (fon9::iequals(tag, "IoFutA"))
         argsIo.FutA_.SetIoServiceCfg(holder, cfgln);
      else if (fon9::iequals(tag, "IoOptA"))
         argsIo.OptA_.SetIoServiceCfg(holder, cfgln);
      else if (auto lgItem = retval->MakeLgItem(tag, cfgln)) {
         fon9::seed::MaTree& mgrTree = *lgItem->Sapling_;
         #define CREATE_TwfLineMgr(ioName, sysName)                                                \
         argsIo.ioName##_.Name_.assign(lgItem->Name_ + "_" #sysName);                              \
         argsIo.ioName##_.IoServiceSrc_                                                            \
            = lgItem->TradingLineMgr_[f9twf::ExgSystemTypeToIndex(f9twf::ExgSystemType::sysName)]  \
            = CreateTwfTradingLineMgr(mgrTree, cfgpath, argsIo.ioName##_, exgMapMgr, f9twf::ExgSystemType::sysName); \
         argsIo.SetRef("Io" #ioName, argsIo.ioName##_.IoServiceSrc_)
         // -----
         CREATE_TwfLineMgr(FutN, FutNormal);
         CREATE_TwfLineMgr(OptN, OptNormal);
         CREATE_TwfLineMgr(FutA, FutAfterHour);
         CREATE_TwfLineMgr(OptA, OptAfterHour);
      }
   }
   return retval;
}

} // namespaces
