// \file f9omstw/OmsIvacTree.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsIvacTree_hpp__
#define __f9omstw_OmsIvacTree_hpp__
#include "f9omstw/OmsBrk.hpp"
#include "f9omstw/OmsTree.hpp"

namespace f9omstw {

/// 為何當有需要時才建立新的 OmsIvacTree:
/// - 因為 seed 機制僅用來管理(查看), 一般而言透過 seed 機制查看 OmsIvac 不會太常使用,
///   因此沒必要讓每個 OmsBrk 負擔一個 seed::Tree 的空間.
/// - OmsBrk 繼承自 OmsIvBase 包含了一個 intrusive_ref_counter,
///   而 Tree 也有 intrusive_ref_counter, 因此會有衝突, 比較難處理.
class OmsIvacTree : public OmsTree {
   fon9_NON_COPY_NON_MOVE(OmsIvacTree);
   using base = OmsTree;
   struct TreeOp;

protected:
   void InThr_OnTreeOp(fon9::seed::FnTreeOp fnCallback) override;

public:
   const OmsBrkSP Brk_;

   OmsIvacTree(OmsCore& omsCore, fon9::seed::LayoutSP ivacLayout, OmsBrkSP brk)
      : base(omsCore, std::move(ivacLayout))
      , Brk_{std::move(brk)} {
   }

   static constexpr fon9::seed::TreeFlag DefaultTreeFlag() {
      return fon9::seed::TreeFlag::AddableRemovable | fon9::seed::TreeFlag::Unordered;
   }
};

} // namespaces
#endif//__f9omstw_OmsIvacTree_hpp__
