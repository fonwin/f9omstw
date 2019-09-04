// \file f9omstws/OmsTwsTradingLineMgrCfg.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsTradingLineMgrCfg.hpp"
#include "f9omstws/OmsTwsTradingLineMgr.hpp"

namespace f9omstw {
using namespace fon9;
using namespace fon9::seed;

struct TwsTradingLineMgrCfgSeed::CfgOrdTeams : public MaConfigSeed {
   fon9_NON_COPY_NON_MOVE(CfgOrdTeams);
   using base = MaConfigSeed;
   CfgOrdTeams(MaConfigTree& configTree)
      : base(configTree, "OrdTeams", "可用櫃號", "使用「,」分隔櫃號, 使用「-」設定範圍") {
   }
   void OnConfigValueChanged(ConfigLocker&& lk, StrView val) override {
      fon9::CharVector ordTeams{val};
      lk.unlock();
      static_cast<TwsTradingLineMgrCfgSeed*>(&this->ConfigTree_.ConfigMgr_)
         ->LineMgr_.OnOrdTeamConfigChanged(ordTeams);
      base::OnConfigValueChanged(std::move(lk), ToStrView(ordTeams));
   }
};
//--------------------------------------------------------------------------//
fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list
TwsTradingLineMgrCfgSeed::TwsTradingLineMgrCfgSeed(TwsTradingLineMgr& linemgr,
                                                   const std::string& cfgpath,
                                                   std::string name)
   : base(new MaConfigTree(*this, MaConfigSeed::MakeLayout("TwsTradingLineMgrCfg")),
          std::move(name))
   , LineMgr_(linemgr) {
   auto& configTree = this->GetConfigTree();
   configTree.Add(new CfgOrdTeams{configTree});
   this->BindConfigFile(cfgpath + this->Name_ + ".f9gv", true);
}
fon9_MSC_WARN_POP;

} // namespaces
