// \file f9omstw/OmsRequestFilled.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestFilled.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsRequestFilled::MakeFieldsImpl(fon9::seed::Fields& flds) {
   flds.Add(fon9_MakeField2(OmsRequestFilled, IniSNO));
   flds.Add(fon9_MakeField2(OmsRequestFilled, MatchKey));
   base::MakeFields<OmsRequestFilled>(flds);
}
void OmsRequestFilled::NoReadyLineReject(fon9::StrView) {
}
const OmsRequestFilled* OmsRequestFilled::Insert(const OmsRequestFilled** ppHead,
                                                 const OmsRequestFilled** ppLast,
                                                 const OmsRequestFilled* curr) {
   if (const OmsRequestFilled* chk = *ppLast) {
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
