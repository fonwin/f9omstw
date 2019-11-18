// \file f9omstw/OmsReportTools.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsReportTools.hpp"
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {

void OmsBadLeavesWriteLog(OmsReportRunnerInCore& inCoreRunner, int leavesQty) {
   fon9::RevPrint(inCoreRunner.ExLogForUpd_, "Bad.Leaves=", leavesQty,
                  fon9_kCSTR_ROWSPL ">" fon9_kCSTR_CELLSPL);
}
int32_t OmsGetSymbolPriMul(OmsReportChecker& checker, fon9::StrView symbid) {
   if (auto symb = checker.Resource_.Symbs_->GetSymb(symbid)) {
      if (symb->PriceOrigDiv_ > 0)
         return fon9::signed_cast(symb->PriceOrigDiv_);
      checker.ReportAbandon("OmsGetSymbolPriMul: Bad symbol PriceOrigDiv.");
   }
   else
      checker.ReportAbandon("OmsGetSymbolPriMul: Symbol not found.");
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
