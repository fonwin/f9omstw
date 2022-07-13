// \file f9omstwf/OmsTwfTradingLineMgr.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfTradingLineMgr.hpp"
#include "f9omstw/OmsTradingLineMgrCfg.hpp"
#include "fon9/FilePath.hpp"

namespace f9omstw {

TwfTradingLineGroup::~TwfTradingLineGroup() {
}
void TwfTradingLineGroup::OnTDayChangedInCore(OmsResource& resource) {
   for (unsigned L = 0; L < numofele(this->TradingLineMgr_); ++L) {
      assert(this->TradingLineMgr_[L].get() != nullptr);
      this->TradingLineMgr_[L]->OnOmsCoreChanged(resource);
   }
}
TwfTradingLineMgr* TwfTradingLineGroup::GetLineMgr(const OmsOrderRaw& ordraw, OmsRequestRunnerInCore* runner) const {
   unsigned idx;
   switch (ordraw.SessionId()) {
   case f9fmkt_TradingSessionId_AfterHour: idx = 1; break;
   case f9fmkt_TradingSessionId_Normal:    idx = 0; break;
   default:
      if (runner)
         runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_Bad_SessionId, nullptr);
      return nullptr;
   }
   switch (ordraw.Market()) {
   case f9fmkt_TradingMarket_TwFUT:
      idx = f9twf::ExgSystemTypeToIndex(idx ? f9twf::ExgSystemType::FutAfterHour : f9twf::ExgSystemType::FutNormal);
      break;
   case f9fmkt_TradingMarket_TwOPT:
      idx = f9twf::ExgSystemTypeToIndex(idx ? f9twf::ExgSystemType::OptAfterHour : f9twf::ExgSystemType::OptNormal);
      break;
   default:
      if (runner)
         runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_Bad_MarketId, nullptr);
      return nullptr;
   }
   return this->TradingLineMgr_[idx].get();
}

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
