// \file f9utws/UtwsIvSc.cpp
// \author fonwinz@gmail.com
#include "f9utws/UtwsIvSc.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void UtwsIvSc::MakeFields(int ofsadj, fon9::seed::Fields& scflds) {
   scflds.Add(fon9_MakeField2_OfsAdj(ofsadj, UtwsIvSc, TotalBuy));
   scflds.Add(fon9_MakeField2_OfsAdj(ofsadj, UtwsIvSc, TotalSell));
}

} // namespaces
