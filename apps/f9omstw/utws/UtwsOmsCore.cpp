// \file utws/UtwsOmsCore.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCoreByThread.hpp"
#include "utws/UtwsSymb.hpp"
#include "utws/UtwsBrk.hpp"
#include "fon9/seed/Plugins.hpp"
#include "fon9/seed/SysEnv.hpp"
#include "fon9/TimedFileName.hpp"

namespace f9omstw {

struct UtwsOmsCore : public OmsCoreByThread {
   fon9_NON_COPY_NON_MOVE(UtwsOmsCore);
   using base = OmsCoreByThread;
   UtwsOmsCore() : base{"omstws"} {
      this->Symbs_.reset(new OmsSymbTree(*this, UtwsSymb::MakeLayout(OmsSymbTree::DefaultTreeFlag()), &UtwsSymb::SymbMaker));
      this->Brks_.reset(new OmsBrkTree(*this, UtwsBrk::MakeLayout(OmsBrkTree::DefaultTreeFlag()), &OmsBrkTree::TwsBrkIndex1));
      this->Brks_->Initialize(&UtwsBrk::BrkMaker, "8610", 5u, &IncStrAlpha);
      // 建立委託書號表的關聯.
      this->Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwSEC);
      this->Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwOTC);
   }
   ~UtwsOmsCore() {
      this->WaitForEndNow();
   }

   static bool Create(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
      (void)args;
      UtwsOmsCore* core;
      if (!holder.Root_->Add(core = new UtwsOmsCore{}))
         return false;
      fon9::TimeStamp     tday = fon9::UtcNow();
      fon9::TimedFileName logfn(fon9::seed::SysEnv_GetLogFileFmtPath(*holder.Root_), fon9::TimedFileName::TimeScale::Day);
      logfn.RebuildFileName(tday);
      std::string fname = logfn.GetFileName() + core->Name_ + ".log";
      auto res = core->Start(fon9::UtcNow(), fname);
      core->SetTitle(std::move(fname));
      if (res.IsError())
         core->SetDescription(fon9::RevPrintTo<std::string>(res));
      return true;
   }
};

} // namespaces

static fon9::seed::PluginsDesc f9p_UtwsOmsCore{"", &f9omstw::UtwsOmsCore::Create, nullptr, nullptr,};
static fon9::seed::PluginsPark f9pRegister{"UtwsOmsCore", &f9p_UtwsOmsCore};
