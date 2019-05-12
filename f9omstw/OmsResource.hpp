// \file f9omstw/OmsResource.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsResource_hpp__
#define __f9omstw_OmsResource_hpp__
#include "f9omstw/OmsSymbTree.hpp"
#include "f9omstw/OmsBrkTree.hpp"
#include "f9omstw/OmsRequestMgr.hpp"
#include "fon9/seed/MaTree.hpp"

namespace f9omstw {

/// OMS 所需的資源, 集中在此處理.
/// - 這裡的資源都 **不是** thread safe!
class OmsResource : public fon9::seed::NamedSapling {
   fon9_NON_COPY_NON_MOVE(OmsResource);

public:
   OmsCore&    Core_;

   using SymbTreeSP = fon9::intrusive_ptr<OmsSymbTree>;
   SymbTreeSP  Symbs_;

   using BrkTreeSP = fon9::intrusive_ptr<OmsBrkTree>;
   BrkTreeSP   Brks_;

   OmsRequestMgr  RequestMgr_;

protected:
   template <class... NamedArgsT>
   OmsResource(OmsCore& core, NamedArgsT&&... namedargs)
      : fon9::seed::NamedSapling(new fon9::seed::MaTree("Tables"), std::forward<NamedArgsT>(namedargs)...)
      , Core_(core) {
   }
   ~OmsResource();

   void AddNamedSapling(fon9::seed::TreeSP sapling, fon9::Named&& named) {
      static_cast<fon9::seed::MaTree*>(this->Sapling_.get())->
         Add(new fon9::seed::NamedSapling(std::move(sapling), std::move(named)));
   }

   /// 將 this->Symbs_; this->Brks_; 加入 this->Sapling.
   void Plant();
};

} // namespaces
#endif//__f9omstw_OmsResource_hpp__
