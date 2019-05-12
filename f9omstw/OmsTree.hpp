// \file f9omstw/OmsTree.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsTree_hpp__
#define __f9omstw_OmsTree_hpp__
#include "f9omstw/OmsBase.hpp"
#include "fon9/seed/Tree.hpp"

namespace f9omstw {

/// 在 OmsCore 使用 seed 機制管理的 tree,
/// 透過這裡協助 OnTreeOp(); OnParentSeedClear(); 轉入 OmsCore thread 執行,
/// 衍生者必須複寫:
///   void InThr_OnTreeOp(fon9::seed::FnTreeOp&& fnCallback);
///   void InThr_OnParentSeedClear();
class OmsTree : public fon9::seed::Tree {
   fon9_NON_COPY_NON_MOVE(OmsTree);
   using base = fon9::seed::Tree;

   virtual void InThr_OnTreeOp(fon9::seed::FnTreeOp fnCallback) = 0;
   /// 預設 do nothing.
   virtual void InThr_OnParentSeedClear();

public:
   OmsCore& OmsCore_;

   template <class... ArgsT>
   OmsTree(OmsCore& omsCore, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , OmsCore_(omsCore) {
   }

   void OnTreeOp(fon9::seed::FnTreeOp fnCallback) override;
   void OnParentSeedClear() override;

   static bool IsInOmsThread(fon9::seed::Tree* tree);
};

/// 在 OmsCore 使用 seed 機制管理的 tree,
/// 透過這裡協助 OnTreeOp(); OnParentSeedClear(); 轉入 OmsCore thread 執行.
template <class TreeBase>
class OmsSapling : public TreeBase {
   fon9_NON_COPY_NON_MOVE(OmsSapling);
   using base = TreeBase;
public:
   OmsCore& OmsCore_;

   template <class... ArgsT>
   OmsSapling(OmsCore& omsCore, ArgsT&&... args)
      : TreeBase(std::forward<ArgsT>(args)...)
      , OmsCore_(omsCore) {
   }

   void OnTreeOp(fon9::seed::FnTreeOp fnCallback) override {
      fon9::intrusive_ptr<OmsSapling> pthis{this};
      this->OmsCore_.EmplaceMessage([pthis, fnCallback]() {
         pthis->base::OnTreeOp(std::move(fnCallback));
      });
   }
   void OnParentSeedClear() override {
      fon9::intrusive_ptr<OmsSapling> pthis{this};
      this->OmsCore_.EmplaceMessage([pthis]() {
         pthis->base::OnParentSeedClear();
      });
   }

   static bool IsInOmsThread(fon9::seed::Tree* tree) {
      if (OmsSapling* pthis = dynamic_cast<OmsSapling*>(tree))
         return pthis->OmsCore_.IsThisThread();
      return false;
   }
};

} // namespaces
#endif//__f9omstw_OmsTree_hpp__
