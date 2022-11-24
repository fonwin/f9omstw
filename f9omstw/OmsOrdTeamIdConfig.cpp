/// \file f9omstw/OmsOrdTeamIdConfig.cpp
/// \author fonwinz@gmail.com
#include "f9omstw/OmsOrdTeamIdConfig.hpp"
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {

void OmsOrdTeamIdConfigSeed::ResetParentRequestTgId(OmsResource& res) {
   this->TgId_ = 0;
   if (auto* tgCfg = res.OrdTeamGroupMgr_.SetTeamGroup(&this->Name_, this->GetConfigValue(this->OwnerTree_.Lock())))
      this->TgId_ = tgCfg->TeamGroupId_;
}
void OmsOrdTeamIdConfigSeed::OnConfigValueChanged(fon9::seed::MaConfigTree::Locker&& lk, fon9::StrView val) {
   base::OnConfigValueChanged(std::move(lk), val);
   if (!this->OmsCore_)
      return;
   this->OmsCore_->RunCoreTask([this](OmsResource& res) {
      this->ResetParentRequestTgId(res);
   });
}
void OmsOrdTeamIdConfigSeed::OnAfter_InitCoreTables(OmsResource& res) {
   this->OmsCore_ = &res.Core_;
   this->ResetParentRequestTgId(res);
}

} // namespaces
