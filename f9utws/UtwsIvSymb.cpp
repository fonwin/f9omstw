// \file f9utws/UtwsIvSymb.cpp
// \author fonwinz@gmail.com
#include "f9utws/UtwsIvSymb.hpp"
#include "fon9/seed/PodOp.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

static fon9::seed::Fields UtwsIvSymb_MakeFields(int ofsadj) {
   fon9::seed::Fields   flds;
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, UtwsIvSymbSc, GnQty));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, UtwsIvSymbSc, CrQty));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, UtwsIvSymbSc, CrAmt));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, UtwsIvSymbSc, DbQty));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, UtwsIvSymbSc, DbAmt));
   return flds;
}
fon9::seed::LayoutSP UtwsIvSymb::MakeLayout() {
   using namespace fon9::seed;
   return new LayoutN(fon9_MakeField2(UtwsIvSymb, SymbId), TreeFlag::AddableRemovable,
      new Tab{fon9::Named{"Bal"}, UtwsIvSymb_MakeFields(fon9_OffsetOfRawPointer(UtwsIvSymb, Bal_)), TabFlag::NoSapling_NoSeedCommand_Writable},
      new Tab{fon9::Named{"Ord"}, UtwsIvSymb_MakeFields(fon9_OffsetOfRawPointer(UtwsIvSymb, Ord_)), TabFlag::NoSapling_NoSeedCommand_Writable},
      new Tab{fon9::Named{"Mat"}, UtwsIvSymb_MakeFields(fon9_OffsetOfRawPointer(UtwsIvSymb, Mat_)), TabFlag::NoSapling_NoSeedCommand_Writable}
   );
}

} // namespaces
