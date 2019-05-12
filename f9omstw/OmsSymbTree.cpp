// \file f9omstw/OmsSymbTree.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsSymbTree.hpp"
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {

OmsSymbTree::OmsSymbTree(OmsCore& omsCore, fon9::seed::LayoutSP layout, FnSymbMaker fnSymbMaker)
   : base(omsCore, std::move(layout))
   , SymbMaker_{fnSymbMaker} {
}
fon9::fmkt::SymbSP OmsSymbTree::MakeSymb(const fon9::StrView& symbid) {
   return this->SymbMaker_(symbid);
}

} // namespaces
