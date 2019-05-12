// \file utws/UtwsIvSc.cpp
// \author fonwinz@gmail.com
#include "utws/UtwsIvSc.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void UtwsIvSc::MakeFields(fon9::seed::Fields& scflds) {
   scflds.Add(fon9_MakeField(fon9::Named{"TotalBuy"},  UtwsIvSc, TotalBuy_));
   scflds.Add(fon9_MakeField(fon9::Named{"TotalSell"}, UtwsIvSc, TotalSell_));
}

} // namespaces
