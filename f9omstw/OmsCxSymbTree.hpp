// \file f9omstw/OmsCxSymbTree.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCxSymbTree_hpp__
#define __f9omstw_OmsCxSymbTree_hpp__
#include "f9omstw/OmsSymbTree.hpp"
#include "f9omstw/OmsResource.hpp"

namespace f9omstw {

using OmsSymbTreeSP = fon9::intrusive_ptr<OmsSymbTree>;

void OmsAddMarketSymbTree(OmsSymbTreeSP symbTree);
void OmsDelMarketSymbTree(OmsSymbTreeSP symbTree);

/// 尋找全部市場的商品.
/// 可提供給條件判斷單元, 使用不同市場的商品.
/// 在建立 OmsCoreMgrSeed.InitCoreTables() 時, 必須主動呼叫 OmsCreateSymbTreeResouce() 註冊;
OmsSymbSP OmsGetCrossMarketSymb(OmsSymbTree* preferSymbMap, fon9::StrView symbId);

template <class SymbTree, class Symb, class... LayoutArgsT>
void OmsCreateSymbTreeResouce(OmsResource& omsResource, LayoutArgsT&&... args) {
   OmsDelMarketSymbTree(omsResource.Symbs_);
   omsResource.Symbs_.reset(new SymbTree(omsResource.Core_, Symb::MakeLayout(SymbTree::DefaultTreeFlag(), std::forward<LayoutArgsT>(args)...), &Symb::SymbMaker));
   OmsAddMarketSymbTree(omsResource.Symbs_);

}

} // namespaces
#endif//__f9omstw_OmsCxSymbTree_hpp__
