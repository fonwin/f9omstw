/// \file f9omstw/OmsPoIvList.cpp
/// \author fonwinz@gmail.com
#include "f9omstw/OmsPoIvList.hpp"
#include "f9omstw/IvacNo.hpp"
#include "fon9/ToStr.hpp"
#include "fon9/StrTo.hpp"

namespace f9omstw {

OmsIvKey::KeyItems::KeyItems(fon9::StrView key)
   : BrkId_{fon9::StrFetchTrim(key, '-')}
   , IvacNo_{fon9::StrFetchTrim(key, '-')}
   , SubacNo_{key} {
}
OmsIvKey OmsIvKey::Normalize(fon9::StrView v) {
   KeyItems items{v};
   size_t   szSubacNo = items.SubacNo_.size();
   if (szSubacNo)
      ++szSubacNo; // 增加一個 '-';

   size_t   szIvacNo = items.IvacNo_.size();
   char     strIvacNoBuf[kIvacNoWidth + 1];
   if (szIvacNo == 0) {
      if (szSubacNo) {
         items.IvacNo_ = "*";
         szIvacNo = 2;
      }
   }
   else if (fon9::FindWildcard(items.IvacNo_)) {
      if (szIvacNo > kIvacNoWidth)
         items.IvacNo_.SetEnd(items.IvacNo_.begin() + (szIvacNo = kIvacNoWidth));
      ++szIvacNo;
   }
   else {
      fon9::Pic9ToStrRev<kIvacNoWidth>(strIvacNoBuf + kIvacNoWidth, fon9::StrTo(items.IvacNo_, IvacNo{0}));
      items.IvacNo_.Reset(strIvacNoBuf, strIvacNoBuf + kIvacNoWidth);
      szIvacNo = kIvacNoWidth + 1;
   }
   size_t   szBrkId = items.BrkId_.size();
   if (szBrkId == 0) {
      items.BrkId_ = "*";
      szBrkId = 1;
   }
   else if (szBrkId > 7) { // 期貨商代號 = 7碼.
      items.BrkId_.SetEnd(items.BrkId_.begin() + (szBrkId = 7));
   }
   fon9::CharVector  retval;
   char* pout = reinterpret_cast<char*>(retval.alloc(szBrkId + szIvacNo + szSubacNo));
   memcpy(pout, items.BrkId_.begin(), szBrkId);
   if (szIvacNo) {
      *(pout += szBrkId) = '-';
      memcpy(pout + 1, items.IvacNo_.begin(), szIvacNo - 1);
      if (szSubacNo) {
         *(pout += szIvacNo) = '-';
         memcpy(pout + 1, items.SubacNo_.begin(), szSubacNo - 1);
      }
   }
   return OmsIvKey{std::move(retval)};
}

} // namespaces
