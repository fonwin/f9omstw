// \file f9omstw/OmsRequestMatch.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestMatch.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsRequestMatch::MakeFieldsImpl(fon9::seed::Fields& flds) {
   flds.Add(fon9_MakeField2(OmsRequestMatch, IniSNO));
   flds.Add(fon9_MakeField2(OmsRequestMatch, MatchKey));
   base::MakeFields<OmsRequestMatch>(flds);
}
void OmsRequestMatch::NoReadyLineReject(fon9::StrView) {
}
const OmsRequestMatch* OmsRequestMatch::Insert(const OmsRequestMatch** ppHead,
                                               const OmsRequestMatch** ppLast,
                                               const OmsRequestMatch* curr) {
   if (const OmsRequestMatch* chk = *ppLast) {
      assert(*ppHead != nullptr);
      if (fon9_LIKELY(chk->MatchKey_ < curr->MatchKey_)) {
         *ppLast = curr;
         chk->InsertAfter(curr);
      }
      else if (curr->MatchKey_ < (chk = *ppHead)->MatchKey_) {
         *ppHead = curr;
         curr->InsertAfter(chk);
      }
      else {
         for (;;) {
            if (chk->MatchKey_ == curr->MatchKey_)
               return chk;
            assert(chk->Next_ != nullptr);
            if (chk->MatchKey_ < curr->MatchKey_ && curr->MatchKey_ < chk->Next_->MatchKey_) {
               chk->InsertAfter(curr);
               break;
            }
            chk = chk->Next_;
         }
      }
   }
   else {
      assert(*ppHead == nullptr);
      *ppHead = *ppLast = curr;
   }
   return nullptr;
}

} // namespaces
