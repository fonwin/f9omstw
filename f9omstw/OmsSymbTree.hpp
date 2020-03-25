// \file f9omstw/OmsSymbTree.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsSymbTree_hpp__
#define __f9omstw_OmsSymbTree_hpp__
#include "fon9/fmkt/SymbTree.hpp"
#include "f9omstw/OmsSymb.hpp"
#include "f9omstw/OmsTree.hpp"

namespace f9omstw {

using OmsSymbTreeBase = fon9::fmkt::SymbTreeT<fon9::fmkt::SymbHashMap, fon9::DummyMutex>;

/// OMS 使用 single thread core:
/// - OMS 商品資料表: single thread(DummyMutex) + unordered
class OmsSymbTree : public OmsSapling<OmsSymbTreeBase> {
   fon9_NON_COPY_NON_MOVE(OmsSymbTree);
   using base = OmsSapling<OmsSymbTreeBase>;
public:
   typedef OmsSymbSP (*FnSymbMaker)(const fon9::StrView& symbid);
   const FnSymbMaker SymbMaker_;

   OmsSymbTree(OmsCore& omsCore, fon9::seed::LayoutSP layout, FnSymbMaker fnSymbMaker);

   fon9::fmkt::SymbSP MakeSymb(const fon9::StrView& symbid) override;

   OmsSymbSP GetOmsSymb(const fon9::StrView& symbid) {
      return fon9::static_pointer_cast<OmsSymb>(this->GetSymb(symbid));
   }
   OmsSymbSP FetchOmsSymb(const fon9::StrView& symbid) {
      return fon9::static_pointer_cast<OmsSymb>(this->FetchSymb(symbid));
   }

   constexpr static fon9::seed::TreeFlag DefaultTreeFlag() {
      return fon9::seed::TreeFlag::AddableRemovable | fon9::seed::TreeFlag::Unordered;
   }
};

} // namespaces
#endif//__f9omstw_OmsSymbTree_hpp__
