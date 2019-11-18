// \file f9omstwf/OmsTwfTradingLineMgr.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfTradingLineMgr.hpp"
#include "f9omstw/OmsTradingLineMgrCfg.hpp"
#include "fon9/FilePath.hpp"

namespace f9omstw {

TwfTradingLineMgrSP CreateTwfTradingLineMgr(fon9::seed::MaTree&  owner,
                                            std::string          cfgpath,
                                            fon9::IoManagerArgs& ioargs,
                                            f9twf::ExgMapMgrSP   exgMapMgr,
                                            f9twf::ExgSystemType exgSysType) {
   cfgpath = fon9::FilePath::AppendPathTail(&cfgpath);

   std::string orgName = std::move(ioargs.Name_);
   ioargs.Name_ = orgName + "_io";
   ioargs.CfgFileName_ = cfgpath + ioargs.Name_ + ".f9gv";

   TwfTradingLineMgrSP linemgr{new TwfTradingLineMgr(ioargs, fon9::TimeInterval::Null(),
                                                     std::move(exgMapMgr), exgSysType)};
   // 上一行的 fon9::TimeInterval::Null(): 建構時不應啟動 device, 必須在 OnOmsCoreChanged(); 啟動.
   ioargs.Name_ = std::move(orgName);

   OmsTradingLineMgrCfgSeed* cfgmgr;
   if (!owner.Add(new fon9::seed::NamedSapling(linemgr, linemgr->Name_))
    || !owner.Add(cfgmgr = new OmsTradingLineMgrCfgSeed(*linemgr, ioargs.Name_ + "_cfg")))
      return nullptr;
   cfgmgr->BindConfigFile(&cfgpath);
   return linemgr;
}

} // namespaces
