// \file f9omstws/OmsTwsTradingLineMgrCfg.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsTradingLineMgrCfg.hpp"
#include "f9omstws/OmsTwsTradingLineMgr.hpp"

namespace f9omstw {
using namespace fon9;
using namespace fon9::seed;

struct TwsTradingLineMgrCfgSeed::CfgTree : public MaTreeConfig {
   fon9_NON_COPY_NON_MOVE(CfgTree);
   using base = MaTreeConfig;
   TwsTradingLineMgrCfgSeed&  Owner_;

   CfgTree(TwsTradingLineMgrCfgSeed& owner)
      : base{MaTreeConfigItem::MakeLayout("TwsTradingLineMgrCfg")}
      , Owner_(owner) {
   }
   void OnMaTree_AfterUpdated(Locker& container, NamedSeed& item) override {
      if (&item == this->Owner_.CfgOrdTeams_) {
         fon9::CharVector val = this->Owner_.CfgOrdTeams_->Value_;
         container.unlock();
         this->Owner_.LineMgr_.OnOrdTeamConfigChanged(val);
         container.lock();
      }
      base::OnMaTree_AfterUpdated(container, item);
   }
};
//--------------------------------------------------------------------------//
fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list
TwsTradingLineMgrCfgSeed::TwsTradingLineMgrCfgSeed(TwsTradingLineMgr& linemgr,
                                                   const std::string& cfgpath,
                                                   std::string name)
   : base(new CfgTree{*this}, std::move(name))
   , CfgOrdTeams_{new MaTreeConfigItem("OrdTeams", "可用櫃號", "使用「,」分隔櫃號, 使用「-」設定範圍")}
   , LineMgr_(linemgr) {
   static_cast<CfgTree*>(this->Sapling_.get())->Add(CfgOrdTeams_);

   std::string cfgfn = cfgpath + this->Name_ + ".f9gv";
   this->SetDescription(static_cast<CfgTree*>(this->Sapling_.get())->BindConfigFile(cfgfn));
   this->SetTitle(std::move(cfgfn));
   linemgr.OnOrdTeamConfigChanged(this->CfgOrdTeams_->Value_);
}
fon9_MSC_WARN_POP;

} // namespaces
