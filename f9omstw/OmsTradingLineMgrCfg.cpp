// \file f9omstw/OmsTradingLineMgrCfg.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsTradingLineMgrCfg.hpp"
#include "f9omstw/OmsTradingLineMgr.hpp"
#include "fon9/FilePath.hpp"

namespace f9omstw {
using namespace fon9;
using namespace fon9::seed;

struct OmsTradingLineMgrCfgSeed::CfgOrdTeams : public MaConfigSeed {
   fon9_NON_COPY_NON_MOVE(CfgOrdTeams);
   using base = MaConfigSeed;
   CfgOrdTeams(MaConfigTree& configTree)
      : base(configTree, "OrdTeams", "可用櫃號", "使用「,」分隔櫃號, 使用「-」設定範圍") {
   }
   void OnConfigValueChanged(ConfigLocker&& lk, StrView val) override {
      fon9::CharVector ordTeams{val};
      lk.unlock();
      static_cast<OmsTradingLineMgrCfgSeed*>(&this->OwnerTree_.ConfigMgr_)
         ->LineMgr_.OnOrdTeamConfigChanged(ordTeams);
      base::OnConfigValueChanged(std::move(lk), ToStrView(ordTeams));
   }
};
//--------------------------------------------------------------------------//
OmsTradingLineMgrCfgSeed::OmsTradingLineMgrCfgSeed(OmsTradingLineMgrBase& linemgr, std::string name)
   : base(std::move(name))
   , LineMgr_(linemgr) {
   auto configSapling = new MaConfigTree(*this, MaConfigSeed::MakeLayout("TradingLineMgrCfg"));
   this->SetConfigSapling(configSapling);
   configSapling->Add(new CfgOrdTeams{*configSapling});
}

void OmsTradingLineMgrCfgSeed::BindConfigFile(fon9::StrView cfgpath) {
   base::BindConfigFile(fon9::FilePath::AppendPathTail(cfgpath) + this->Name_ + ".f9gv", true);
}

} // namespaces
