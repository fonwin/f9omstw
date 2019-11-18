// \file f9utw/UtwsBrk.cpp
// \author fonwinz@gmail.com
#include "f9utw/UtwsBrk.hpp"
#include "f9utw/UtwsIvac.hpp"
#include "f9omstw/OmsIvacTree.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

OmsIvacSP UtwsBrk::MakeIvac(IvacNo ivacNo) {
   return new UtwsIvac(ivacNo, this);
}

fon9::seed::LayoutSP UtwsBrk::MakeLayout(fon9::seed::TreeFlag treeflags) {
   using namespace fon9::seed;
   return new Layout1(fon9_MakeField2(UtwsBrk, BrkId), treeflags,
               new Tab{fon9::Named{"Base"}, Fields{},
                      UtwsIvac::MakeLayout(OmsIvacTree::DefaultTreeFlag()),
                      TabFlag::NoSeedCommand}
               );
}

} // namespaces
