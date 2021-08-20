// \file f9omstw/OmsErrCodeActTree.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsErrCodeActTree_hpp__
#define __f9omstw_OmsErrCodeActTree_hpp__
#include "f9omstw/OmsTree.hpp"
#include "f9omstw/OmsErrCodeAct.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "fon9/seed/MaTree.hpp"

namespace f9omstw {

class ErrCodeActSeed : public fon9::seed::NamedSapling {
   fon9_NON_COPY_NON_MOVE(ErrCodeActSeed);
   using base = fon9::seed::NamedSapling;

   OmsErrCodeActAry  ErrCodeActs_;

   /// ErrCodeActMap 僅提供給 ErrCodeActTree 使用.
   class ErrCodeActTree;
   using ErrCodeActKey = fon9::Decimal<uint32_t, 4>;
   struct ErrCodeActMap : public fon9::SortedVector<ErrCodeActKey, OmsErrCodeActSP> {
      // 為了讓底下的 friend 生效, 所以使用 struct ErrCodeActMap;
      // 若用 using ErrCodeActMap = ...; 「可能」會讓 gcc 找不到底下的 friend;
   };
   using TreeBase = fon9::seed::TreeLockContainerT<ErrCodeActMap>;
   friend inline void OnTreeClearSeeds(TreeBase&, ErrCodeActMap&) {
      // 提供給 TreeBase::OnParentSeedClear() 使用.
   }
   friend inline ErrCodeActMap::iterator ContainerLowerBound(ErrCodeActMap& map, fon9::StrView strKeyText) {
      return map.lower_bound(fon9::StrTo(strKeyText, ErrCodeActKey{}));
   }
   friend inline ErrCodeActMap::iterator ContainerFind(ErrCodeActMap& map, fon9::StrView strKeyText) {
      return map.find(fon9::StrTo(strKeyText, ErrCodeActKey{}));
   }

public:
   ErrCodeActSeed(std::string name);

   void OnSeedCommand(fon9::seed::SeedOpResult& res, fon9::StrView cmdln,
                      fon9::seed::FnCommandResultHandler resHandler,
                      fon9::seed::MaTree::Locker&& ulk,
                      fon9::seed::SeedVisitor*) override;

   void ReloadErrCodeAct(fon9::StrView cfgfn, fon9::seed::MaTree::Locker&& lk);

   OmsErrCodeActSP GetErrCodeAct(OmsErrCode ec) const {
      if (fon9_LIKELY(ec == OmsErrCode_NoError))
         return nullptr;
      auto locker{static_cast<TreeBase*>(this->Sapling_.get())->Lock()};
      if (auto* p = this->ErrCodeActs_.Get(ec))
         return *p;
      return nullptr;
   }
};

} // namespaces
#endif//__f9omstw_OmsBrkTree_hpp__
