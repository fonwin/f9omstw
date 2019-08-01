// \file f9omstws/OmsTwsReport.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsReport.hpp"
#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwsFilled::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwsFilled>(flds);
   flds.Add(fon9_MakeField2(OmsTwsFilled, Time));
   flds.Add(fon9_MakeField2(OmsTwsFilled, Pri));
   flds.Add(fon9_MakeField2(OmsTwsFilled, Qty));
}
void OmsTwsFilled::RunReportInCore(OmsReportRunner&& runner) {
   runner.ReportAbandon("TwsFilled.RunReportInCore: Not impl.");
}
void OmsTwsFilled::ProcessPendingReport(OmsResource& res) const {
}
//--------------------------------------------------------------------------//
void OmsTwsReport::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields(flds);
   base::AddFieldsForReport(flds);
   flds.Add(fon9_MakeField2(OmsTwsReport, ExgTime));
   flds.Add(fon9_MakeField2(OmsTwsReport, BeforeQty));
}
void OmsTwsReport::RunReportInCore(OmsReportRunner&& runner) {
   runner.ReportAbandon("OmsTwsReport.RunReportInCore: Not impl.");
}
//--------------------------------------------------------------------------//
void OmsTwsReport::ProcessPendingReport(OmsResource& res) const {
}
void OmsTwsRequestChg::ProcessPendingReport(OmsResource& res) const {
}

} // namespaces
