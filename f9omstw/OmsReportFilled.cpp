// \file f9omstw/OmsReportFilled.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsReportFilled.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsReportFilled::MakeFieldsImpl(fon9::seed::Fields& flds) {
   flds.Add(fon9_MakeField2(OmsReportFilled, IniSNO));
   flds.Add(fon9_MakeField2(OmsReportFilled, MatchKey));
   base::MakeFields<OmsReportFilled>(flds);
}
const OmsReportFilled* OmsReportFilled::Insert(const OmsReportFilled** ppHead,
                                               const OmsReportFilled** ppLast,
                                               const OmsReportFilled* curr) {
   assert(curr->Next_ == nullptr);
   if (const OmsReportFilled* chk = *ppLast) {
      assert(*ppHead != nullptr);
      if (fon9_LIKELY(chk->MatchKey_ < curr->MatchKey_)) { // 加到串列尾.
         *ppLast = curr;
         chk->InsertAfter(curr);
      }
      else if (curr->MatchKey_ < (chk = *ppHead)->MatchKey_) { // 加到串列頭.
         *ppHead = curr;
         curr->Next_ = chk;
      }
      else { // 加到串列中間.
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
