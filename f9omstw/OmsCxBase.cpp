// \file f9omstw/OmsCxBase.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCxBase.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsCxBaseChangeable::MakeFields(fon9::seed::Fields& flds, int ofsadj) {
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxBaseChangeable, CondPri));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxBaseChangeable, CondQty));
}

} // namespaces
