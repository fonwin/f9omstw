// \file f9omstwf/OmsTwfReport2.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfReport2.hpp"
#include "f9omstwf/OmsTwfOrder1.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwfReport2::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields(flds);
   base::AddFieldsForReport(flds);
   flds.Add(fon9_MakeField2(OmsTwfReport2, ExgTime));
   flds.Add(fon9_MakeField2(OmsTwfReport2, BeforeQty));
   flds.Add(fon9_MakeField2(OmsTwfReport2, OutPvcId));
}
//--------------------------------------------------------------------------//
static inline bool AdjustReportPrice(int32_t priMul, OmsTwfReport2& rpt) {
   switch (priMul) {
   case 0:  return true;
   case -1: return false;
   default:
      assert(priMul > 0);
      rpt.Pri_ *= priMul;
      return true;
   }
}
//--------------------------------------------------------------------------//
static inline void OmsAssignOrderFromReportNewOrig(OmsTwfOrderRaw1& ordraw, OmsTwfReport2& rpt) {
   if (ordraw.PosEff_ == OmsTwfPosEff{})
      ordraw.PosEff_ = rpt.PosEff_;
   ordraw.AfterQty_ = ordraw.LeavesQty_ = rpt.Qty_;
}
void OmsTwfReport2::RunReportInCore_FromOrig(OmsReportChecker&& checker, const OmsRequestBase& origReq) {
   if (fon9_LIKELY(this->RunReportInCore_FromOrig_Precheck(checker, origReq))) {
      OmsOrder& order = origReq.LastUpdated()->Order();
      if (!AdjustReportPrice(OmsGetReportPriMul(order, checker, *this), *this))
         return;
      OmsOrderRaw& ordraw = *order.BeginUpdate(origReq);
      assert(dynamic_cast<OmsTwfOrderRaw1*>(&ordraw) != nullptr);
      OmsRunReportInCore_FromOrig(std::move(checker), *static_cast<OmsTwfOrderRaw1*>(&ordraw), *this);
   }
}
//--------------------------------------------------------------------------//
void OmsTwfReport2::RunReportInCore_MakeReqUID() {
   this->MakeReportReqUID(this->ExgTime_, this->BeforeQty_);
}
bool OmsTwfReport2::RunReportInCore_IsBfAfMatch(const OmsOrderRaw& ordu) {
   return static_cast<const OmsTwfOrderRaw1*>(&ordu)->BeforeQty_ == this->BeforeQty_
      && static_cast<const OmsTwfOrderRaw1*>(&ordu)->AfterQty_ == this->Qty_;
}
bool OmsTwfReport2::RunReportInCore_IsExgTimeMatch(const OmsOrderRaw& ordu) {
   return static_cast<const OmsTwfOrderRaw1*>(&ordu)->LastExgTime_ == this->ExgTime_;
}
//--------------------------------------------------------------------------//
void OmsTwfReport2::RunReportInCore_Order(OmsReportChecker&& checker, OmsOrder& order) {
   if (AdjustReportPrice(OmsGetReportPriMul(order, checker, *this), *this)) {
      this->PriStyle_ = OmsReportPriStyle::HasDecimal; // 避免重複計算小數位.
      base::RunReportInCore_Order(std::move(checker), order);
   }
}
void OmsTwfReport2::RunReportInCore_OrderNotFound(OmsReportChecker&& checker, OmsOrdNoMap& ordnoMap) {
   if (AdjustReportPrice(OmsGetReportPriMul(checker, *this), *this)) {
      this->PriStyle_ = OmsReportPriStyle::HasDecimal; // 避免重複計算小數位.
      base::RunReportInCore_OrderNotFound(std::move(checker), ordnoMap);
   }
}
void OmsTwfReport2::RunReportInCore_InitiatorNew(OmsReportRunnerInCore&& inCoreRunner) {
   assert(dynamic_cast<OmsTwfOrderRaw1*>(&inCoreRunner.OrderRaw_) != nullptr);
   OmsTwfOrderRaw1&  ordraw = *static_cast<OmsTwfOrderRaw1*>(&inCoreRunner.OrderRaw_);
   ordraw.AfterQty_ = ordraw.LeavesQty_ = this->Qty_;
   ordraw.PosEff_   = this->PosEff_;
   OmsRunReportInCore_InitiatorNew(std::move(inCoreRunner), ordraw, *this);
}
void OmsTwfReport2::RunReportInCore_DCQ(OmsReportRunnerInCore&& inCoreRunner) {
   assert(this->RxKind() != f9fmkt_RxKind_Unknown);
   assert(dynamic_cast<OmsTwfOrderRaw1*>(&inCoreRunner.OrderRaw_) != nullptr);
   OmsTwfOrderRaw1&  ordraw = *static_cast<OmsTwfOrderRaw1*>(&inCoreRunner.OrderRaw_);
   OmsRunReportInCore_DCQ(std::move(inCoreRunner), ordraw, *this);
}
void OmsTwfReport2::OnSynReport(const OmsRequestBase* ref, fon9::StrView message) {
   base::OnSynReport(ref, message);
   this->Message_.assign(message);
   this->PriStyle_ = OmsReportPriStyle::HasDecimal;
}
//--------------------------------------------------------------------------//
static void OmsTwf1_ProcessPendingReport(OmsResource& res, const OmsRequestBase& rpt, const OmsTwfReport2* chkFields) {
   OmsProcessPendingReport<OmsTwfOrderRaw1, OmsTwfRequestIni1>(res, rpt, chkFields);
}
void OmsTwfReport2::ProcessPendingReport(OmsResource& res) const {
   OmsTwf1_ProcessPendingReport(res, *this, this);
}
void OmsTwfRequestChg1::ProcessPendingReport(OmsResource& res) const {
   OmsTwf1_ProcessPendingReport(res, *this, nullptr);
}

} // namespaces
