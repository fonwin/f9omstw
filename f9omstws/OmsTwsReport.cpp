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
   flds.Add(fon9_MakeField2(OmsTwsReport, OutPvcId));
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
static inline void OmsTwsReport_Update(OmsTwsOrderRaw& ordraw) {
   // 台灣證券交易的 f9fmkt_TradingRequestSt_ExchangeCanceling 用來表示:
   // 證券進入價格穩定措施或尾盤集合競價時段，交易所主動取消留存委託簿之「市價」委託單資料並回報.
   // 此時 OrderSt 為 f9fmkt_OrderSt_ExchangeCanceled;
   if (ordraw.RequestSt_ == f9fmkt_TradingRequestSt_ExchangeCanceling
       && ordraw.UpdateOrderSt_ <= f9fmkt_OrderSt_Canceling)
      ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_ExchangeCanceled;
}
static inline void OmsAssignOrderFromReportNewOrig(OmsTwsOrderRaw& ordraw, OmsTwsReport& rpt) {
   if (ordraw.OType_ == OmsTwsOType{})
      ordraw.OType_ = rpt.OType_;
   ordraw.AfterQty_ = ordraw.LeavesQty_ = rpt.Qty_;
   OmsTwsReport_Update(ordraw);
}
void OmsTwsReport::RunReportInCore_FromOrig(OmsReportChecker&& checker, const OmsRequestBase& origReq) {
   if (fon9_LIKELY(this->RunReportInCore_FromOrig_Precheck(checker, origReq))) {
      OmsOrder& order = origReq.LastUpdated()->Order();
      // AdjustReportQtys(order, checker.Resource_, *this); 已在 RunReportInCore_Order() 處理過了.

      OmsOrderRaw& ordraw = *order.BeginUpdate(origReq);
      assert(dynamic_cast<OmsTwsOrderRaw*>(&ordraw) != nullptr);
      OmsRunReportInCore_FromOrig(std::move(checker), *static_cast<OmsTwsOrderRaw*>(&ordraw), *this);
   }
}
void OmsTwsReport::RunReportInCore_Order(OmsReportChecker&& checker, OmsOrder& order) {
   // 必須在此先調整好 this 的數量, 在 RunReportInCore_IsBfAfMatch() 才能正確判斷;
   AdjustReportQtys(order, checker.Resource_, *this);
   base::RunReportInCore_Order(std::move(checker), order);
}
void OmsTwsReport::RunReportInCore_NewOrder(OmsReportRunnerInCore&& runner) {
   AdjustReportQtys(runner.OrderRaw_.Order(), runner.Resource_, *this);
   base::RunReportInCore_NewOrder(std::move(runner));
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
   // AdjustReportQtys(ordraw.Order(), inCoreRunner.Resource_, *this); 已在 RunReportInCore_Order() 處理過了.
   ordraw.AfterQty_ = ordraw.LeavesQty_ = this->Qty_;
   ordraw.OType_ = this->OType_;
   OmsRunReportInCore_InitiatorNew(std::move(inCoreRunner), ordraw, *this);
   OmsTwsReport_Update(ordraw);
}

static void OmsTwsReport_AssignFromIni(OmsTwsReport& rpt, const OmsTwsRequestIni& ini) {
   rpt.Side_ = ini.Side_;
   rpt.Symbol_ = ini.Symbol_;
   rpt.IvacNo_ = ini.IvacNo_;
   rpt.SubacNo_ = ini.SubacNo_;
   rpt.IvacNoFlag_ = ini.IvacNoFlag_;
   if (rpt.OType_ == OmsTwsOType{})
      rpt.OType_ = ini.OType_;
}
void OmsTwsReport::RunReportInCore_DCQ(OmsReportRunnerInCore&& inCoreRunner) {
   assert(this->RxKind() != f9fmkt_RxKind_Unknown);
   assert(dynamic_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_) != nullptr);
   OmsTwsOrderRaw&  ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
   // AdjustReportQtys(ordraw.Order(), inCoreRunner.Resource_, *this); 已在 RunReportInCore_Order() 處理過了.
   OmsRunReportInCore_DCQ(std::move(inCoreRunner), ordraw, *this);
   OmsTwsReport_Update(ordraw);
   if (auto* ini = static_cast<const OmsTwsRequestIni*>(ordraw.Order().Initiator())) {
      assert(dynamic_cast<const OmsTwsRequestIni*>(ordraw.Order().Initiator()) != nullptr);
      OmsTwsReport_AssignFromIni(*this, *ini);
   }
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
//--------------------------------------------------------------------------//
void OmsTwsReport::OnSynReport(const OmsRequestBase* ref, fon9::StrView message) {
   base::OnSynReport(ref, message);
   this->Message_.assign(message);
   this->QtyStyle_ = OmsReportQtyStyle::OddLot;
}

} // namespaces
