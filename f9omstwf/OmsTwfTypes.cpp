// \file f9omstwf/OmsTwfTypes.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfTypes.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "f9omstw/OmsResource.hpp"

namespace f9omstw {

int32_t OmsGetSymbolPriMul(OmsReportChecker& checker, fon9::StrView symbid) {
   if (auto symb = checker.Resource_.Symbs_->GetOmsSymb(symbid)) {
   __CHECK_PRI_MUL:;
      if (symb->PriceOrigDiv_ > 0)
         return fon9::signed_cast(symb->PriceOrigDiv_);
      checker.ReportAbandon("OmsGetSymbolPriMul: Bad symbol PriceOrigDiv.");
   }
   else {
      f9twf::ExgCombSymbId combId;
      if (combId.Parse(symbid)) {
         if (combId.CombSide_ != f9twf::ExgCombSide::None) {
            symbid = ToStrView(combId.LegId1_);
            if ((symb = checker.Resource_.Symbs_->GetOmsSymb(symbid)).get() != nullptr)
               goto __CHECK_PRI_MUL;
         }
      }
      checker.ReportAbandon("OmsGetSymbolPriMul: Symbol not found.");
   }
   return -1;
}
int32_t OmsGetOrderPriMul(OmsOrder& order, OmsReportChecker& checker, fon9::StrView symbid) {
   if (auto symb = order.GetSymb(checker.Resource_, symbid)) {
      if (symb->PriceOrigDiv_ > 0)
         return fon9::signed_cast(symb->PriceOrigDiv_);
      checker.ReportAbandon("OmsGetReportPriMul: Order bad symbol PriceOrigDiv.");
   }
   else
      checker.ReportAbandon("OmsGetReportPriMul: Order symbol not found.");
   return -1;
}

} // namespaces
