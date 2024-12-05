// \file f9omstw/OmsCxSymbTree.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCxSymbTree.hpp"

namespace f9omstw {

// ======================================================================== //
using AllSymbTreeImpl = std::vector<OmsSymbTreeSP>;
using AllSymbTree = fon9::MustLock<AllSymbTreeImpl>;
static AllSymbTree AllSymbTree_;
//--------------------------------------------------------------------//
void OmsAddMarketSymbTree(OmsSymbTreeSP symbMap) {
   assert(symbMap);
   if (!symbMap)
      return;
   auto map = AllSymbTree_.Lock();
   auto ifind = std::find(map->begin(), map->end(), symbMap);
   if (ifind == map->end())
      map->push_back(symbMap);
}
void OmsDelMarketSymbTree(OmsSymbTreeSP symbMap) {
   auto map = AllSymbTree_.Lock();
   auto ifind = std::find(map->begin(), map->end(), symbMap);
   if (ifind != map->end())
      map->erase(ifind);
}
//--------------------------------------------------------------------//
OmsSymbSP OmsGetCrossMarketSymb(OmsSymbTree* preferSymbMap, fon9::StrView symbId) {
   if (preferSymbMap) {
      if (OmsSymbSP retval = preferSymbMap->GetOmsSymb(symbId))
         return retval;
   }
   auto map = AllSymbTree_.Lock();
   auto ibeg = map->begin();
   auto iend = map->end();
   while (ibeg != iend) {
      if (fon9_LIKELY(ibeg->get() != preferSymbMap)) {
         if (auto retval = ibeg->get()->GetOmsSymb(symbId))
            return retval;
      }
      ++ibeg;
   }
   return nullptr;
}
// ======================================================================== //

} // namespaces
