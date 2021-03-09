// \file f9omstws/OmsTwsReport.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsReport.hpp"
#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwsReport::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields(flds);
   base::AddFieldsForReport(flds);
   flds.Add(fon9_MakeField2(OmsTwsReport, ExgTime));
   flds.Add(fon9_MakeField2(OmsTwsReport, BeforeQty));
}
//--------------------------------------------------------------------------//
static inline void AdjustReportQtys(OmsOrder& order, OmsResource& res, OmsTwsReport& rpt) {
   if (auto shUnit = OmsGetReportQtyUnit(order, res, rpt)) {
      rpt.QtyStyle_ = OmsReportQtyStyle::OddLot;
      rpt.Qty_ *= shUnit;
      rpt.BeforeQty_ *= shUnit;
   }
}
//--------------------------------------------------------------------------//
static inline void OmsAssignOrderFromReportNewOrig(OmsTwsOrderRaw& ordraw, OmsTwsReport& rpt) {
   if (ordraw.OType_ == OmsTwsOType{})
      ordraw.OType_ = rpt.OType_;
   ordraw.AfterQty_ = ordraw.LeavesQty_ = rpt.Qty_;
}
void OmsTwsReport::RunReportInCore_FromOrig(OmsReportChecker&& checker, const OmsRequestBase& origReq) {
   if (fon9_LIKELY(this->RunReportInCore_FromOrig_Precheck(checker, origReq))) {
      OmsOrder& order = origReq.LastUpdated()->Order();
      AdjustReportQtys(order, checker.Resource_, *this);

      OmsOrderRaw& ordraw = *order.BeginUpdate(origReq);
      assert(dynamic_cast<OmsTwsOrderRaw*>(&ordraw) != nullptr);
      OmsRunReportInCore_FromOrig(std::move(checker), *static_cast<OmsTwsOrderRaw*>(&ordraw), *this);
   }
}
//--------------------------------------------------------------------------//
void OmsTwsReport::RunReportInCore_MakeReqUID() {
   this->MakeReportReqUID(this->ExgTime_, this->BeforeQty_);
}
bool OmsTwsReport::RunReportInCore_IsBfAfMatch(const OmsOrderRaw& ordu) {
   return static_cast<const OmsTwsOrderRaw*>(&ordu)->BeforeQty_ == this->BeforeQty_
      && static_cast<const OmsTwsOrderRaw*>(&ordu)->AfterQty_ == this->Qty_;
}
bool OmsTwsReport::RunReportInCore_IsExgTimeMatch(const OmsOrderRaw& ordu) {
   return static_cast<const OmsTwsOrderRaw*>(&ordu)->LastExgTime_ == this->ExgTime_;
}
//--------------------------------------------------------------------------//
void OmsTwsReport::RunReportInCore_InitiatorNew(OmsReportRunnerInCore&& inCoreRunner) {
   assert(dynamic_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_) != nullptr);
   OmsTwsOrderRaw&  ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
   AdjustReportQtys(ordraw.Order(), inCoreRunner.Resource_, *this);
   ordraw.AfterQty_ = ordraw.LeavesQty_ = this->Qty_;
   ordraw.OType_ = this->OType_;
   OmsRunReportInCore_InitiatorNew(std::move(inCoreRunner), ordraw, *this);
}
void OmsTwsReport::RunReportInCore_DCQ(OmsReportRunnerInCore&& inCoreRunner) {
   assert(this->RxKind() != f9fmkt_RxKind_Unknown);
   assert(dynamic_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_) != nullptr);
   OmsTwsOrderRaw&  ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
   AdjustReportQtys(ordraw.Order(), inCoreRunner.Resource_, *this);
   OmsRunReportInCore_DCQ(std::move(inCoreRunner), ordraw, *this);
}
//--------------------------------------------------------------------------//
static void OrderRawFromPendingReport(OmsResource& res, const OmsRequestBase& rpt, const OmsTwsReport* chkFields) {
   OmsProcessPendingReport<OmsTwsOrderRaw, OmsTwsRequestIni>(res, rpt, chkFields);
}
void OmsTwsReport::ProcessPendingReport(OmsResource& res) const {
   OrderRawFromPendingReport(res, *this, this);
}
void OmsTwsRequestChg::ProcessPendingReport(OmsResource& res) const {
   OrderRawFromPendingReport(res, *this, nullptr);
}

} // namespaces
