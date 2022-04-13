// \file f9omstw/OmsReportTools.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsReportTools.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9twf/ExgTmpTypes.hpp"

namespace f9omstw {

void OmsBadLeavesWriteLog(OmsReportRunnerInCore& inCoreRunner, int leavesQty) {
   fon9::RevPrint(inCoreRunner.ExLogForUpd_, "Bad.Leaves=", leavesQty,
                  fon9_kCSTR_ROWSPL ">" fon9_kCSTR_CELLSPL);
}

} // namespaces
