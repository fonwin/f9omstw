/// \file f9omstw/OmsOrdTeamIdConfig.hpp
/// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsOrdTeamIdConfig_hpp__
#define __f9omstw_OmsOrdTeamIdConfig_hpp__
#include "f9omstw/OmsBase.hpp"
#include "fon9/seed/MaConfigTree.hpp"

namespace f9omstw {

/// 櫃號設定節點.
class OmsOrdTeamIdConfigSeed : public fon9::seed::MaConfigSeed {
   fon9_NON_COPY_NON_MOVE(OmsOrdTeamIdConfigSeed);
   using base = fon9::seed::MaConfigSeed;
   OmsCore*             OmsCore_{};
   OmsOrdTeamGroupId&   TgId_;
   void ResetParentRequestTgId(OmsResource& res);
public:
   template <class... ArgsT>
   OmsOrdTeamIdConfigSeed(OmsOrdTeamGroupId& tgId, fon9::StrView defaultTeamId, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , TgId_{tgId} {
      this->SetConfigValueImpl(defaultTeamId);
   }
   void OnConfigValueChanged(fon9::seed::MaConfigTree::Locker&& lk, fon9::StrView val) override;
   void OnAfter_InitCoreTables(OmsResource& res);
};

} // namespaces
#endif//__f9omstw_OmsOrdTeamIdConfig_hpp__
