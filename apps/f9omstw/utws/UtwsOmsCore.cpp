// \file utws/UtwsOmsCore.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/Plugins.hpp"
#include "utws/UtwsSymb.hpp"
#include "utws/UtwsBrk.hpp"

namespace f9omstw {

struct UtwsOmsCore : public OmsCore {
   fon9_NON_COPY_NON_MOVE(UtwsOmsCore);
   using base = OmsCore;
   UtwsOmsCore() : base{"omstws"} {
      this->Symbs_.reset(new OmsSymbTree(*this, UtwsSymb::MakeLayout(OmsSymbTree::DefaultTreeFlag()), &UtwsSymb::SymbMaker));
      this->Brks_.reset(new OmsBrkTree(*this, UtwsBrk::MakeLayout(OmsBrkTree::DefaultTreeFlag()), &OmsBrkTree::TwsBrkIndex1));
      this->Brks_->Initialize(&UtwsBrk::BrkMaker, "8610", 5u, &IncStrAlpha);
      this->Start();
   }
   ~UtwsOmsCore() {
      this->WaitForEndNow();
   }

   static bool Create(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
      (void)args;
      return holder.Root_->Add(new UtwsOmsCore{});
   }
};

} // namespaces

static fon9::seed::PluginsDesc f9p_UtwsOmsCore{"", &f9omstw::UtwsOmsCore::Create, nullptr, nullptr,};
static fon9::seed::PluginsPark f9pRegister{"UtwsOmsCore", &f9p_UtwsOmsCore};
