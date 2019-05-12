// \file f9omstw/IvacNo.cpp
// \author fonwinz@gmail.com
#include "f9omstw/IvacNo.hpp"
#include "fon9/StrTo.hpp"

namespace f9omstw {

IvacNo StrToIvacNo(fon9::StrView ivacStr, const char** pend) {
   IvacNo      ivacNo = fon9::StrTo(&ivacStr, 0u);
   const char* pcur = ivacStr.begin();
   if (pcur < ivacStr.end() && *pcur == '-') {
      // "NNN-" or "NNN-C"
      ivacNo *= 10;
      if (++pcur < ivacStr.end())
         ivacNo += (*pcur++ - '0');
   }
   if (pend)
      *pend = pcur;
   return ivacNo;
}

} // namespaces
