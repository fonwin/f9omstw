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
static inline OmsTwsOType RptOType2OrdOType(const OmsTwsReport& rpt) {
   return rpt.OType_ == OmsTwsOType::DayTradeGn ? OmsTwsOType::Gn : rpt.OType_;
}
static inline void OmsAssignOrderFromReportNewOrig(OmsTwsOrderRaw& ordraw, OmsTwsReport& rpt) {
   if (ordraw.OType_ == OmsTwsOType{})
      ordraw.OType_ = RptOType2OrdOType(rpt);
   ordraw.AfterQty_ = ordraw.LeavesQty_ = rpt.Qty_;
   OmsTwsReport_Update(ordraw);
}
void OmsTwsReport::RunReportInCore_FromOrig(OmsReportChecker&& checker, const OmsRequestBase& origReq) {
   if (fon9_LIKELY(this->RunReportInCore_FromOrig_Precheck(checker, origReq))) {
      OmsOrder& order = origReq.LastUpdated()->Order();

      // 如果在 checker.SearchOrigRequestId() 有找到 request,
      // 則會直接來到此處, 不會經過 RunReportInCore_Order()/RunReportInCore_NewOrder();
      // 所以這裡要自行呼叫 AdjustReportQtys() 將數量調整成股數;
      // 因為 AdjustReportQtys() 成功時, 會設定 this->QtyStyle_ = OmsReportQtyStyle::OddLot;
      // 所以呼叫多次 AdjustReportQtys() 沒有問題;
      AdjustReportQtys(order, checker.Resource_, *this);

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
   AdjustReportQtys(runner.OrderRaw().Order(), runner.Resource_, *this);
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
   OmsTwsOrderRaw&  ordraw = inCoreRunner.OrderRawT<OmsTwsOrderRaw>();
   // AdjustReportQtys(ordraw.Order(), inCoreRunner.Resource_, *this); 已在 RunReportInCore_Order() 處理過了.
   ordraw.AfterQty_ = ordraw.LeavesQty_ = this->Qty_;
   ordraw.OType_ = RptOType2OrdOType(*this);
   OmsRunReportInCore_InitiatorNew(std::move(inCoreRunner), ordraw, *this);
   OmsTwsReport_Update(ordraw);
}

void OmsTwsReport::AssignFromIniBase(const OmsTwsRequestIni& ini) {
   this->Side_ = ini.Side_;
   this->Symbol_ = ini.Symbol_;
   this->IvacNo_ = ini.IvacNo_;
   this->SubacNo_ = ini.SubacNo_;
   this->IvacNoFlag_ = ini.IvacNoFlag_;
   if (this->OType_ == OmsTwsOType{})
      this->OType_ = ini.OType_;
}
void OmsTwsReport::RunReportInCore_DCQ(OmsReportRunnerInCore&& inCoreRunner) {
   assert(this->RxKind() != f9fmkt_RxKind_Unknown);
   OmsTwsOrderRaw&  ordraw = inCoreRunner.OrderRawT<OmsTwsOrderRaw>();
   // AdjustReportQtys(ordraw.Order(), inCoreRunner.Resource_, *this); 已在 RunReportInCore_Order() 處理過了.
   OmsRunReportInCore_DCQ(std::move(inCoreRunner), ordraw, *this);
   OmsTwsReport_Update(ordraw);
   if (auto* ini = static_cast<const OmsTwsRequestIni*>(ordraw.Order().Initiator())) {
      assert(dynamic_cast<const OmsTwsRequestIni*>(ordraw.Order().Initiator()) != nullptr);
      this->AssignFromIniBase(*ini);
   }
}
//--------------------------------------------------------------------------//
static void OrderRawFromPendingReport(const OmsRequestRunnerInCore& prevRunner, const OmsRequestBase& rpt, const OmsTwsReport* chkFields) {
   OmsProcessPendingReport<OmsTwsOrderRaw, OmsTwsRequestIni>(prevRunner, rpt, chkFields);
}
void OmsTwsReport::ProcessPendingReport(const OmsRequestRunnerInCore& prevRunner) const {
   OrderRawFromPendingReport(prevRunner, *this, this);
}
void OmsTwsRequestChg::ProcessPendingReport(const OmsRequestRunnerInCore& prevRunner) const {
   OrderRawFromPendingReport(prevRunner, *this, nullptr);
}
//--------------------------------------------------------------------------//
void OmsTwsReport::OnSynReport(const OmsRequestBase* ref, fon9::StrView message) {
   base::OnSynReport(ref, message);
   this->Message_.assign(message);
   this->QtyStyle_ = OmsReportQtyStyle::OddLot;
}
//--------------------------------------------------------------------------//
bool OmsTwsRequestChg::PutAskToRemoteMessage(OmsRequestRunnerInCore& runner, fon9::RevBuffer& rbuf) const {
   if (this->RxKind() == f9fmkt_RxKind_RequestChgQty) {
      // Qty = 改量: 送給證交所的數量(欲刪減量).
      if (auto* lastOrd = static_cast<const OmsTwsOrderRaw*>(runner.OrderRaw().Order().Tail())) {
         auto qty = runner.GetRequestChgQty(this->Qty_, true/*isWantToKill*/, lastOrd->LeavesQty_, lastOrd->CumQty_);
         if (qty >= 0) {
            fon9::RevPrint(rbuf, "|Qty=", fon9::unsigned_cast(qty));
            return true;
         }
      }
      return false;
   }
   if (this->RxKind() == f9fmkt_RxKind_RequestChgPri) {
      switch (this->PriType_) {
      case f9fmkt_PriType_Unknown:
      case f9fmkt_PriType_Limit:
         fon9::RevPrint(rbuf, "|Pri=", this->Pri_);
         break;
      case f9fmkt_PriType_Market:
      case f9fmkt_PriType_Mwp:
         break;
      }
      fon9::RevPrint(rbuf, "|PriType=", this->PriType_);
   }
   return true;
}
bool OmsTwsReport::ParseAskerMessage(fon9::StrView message) {
   // 解析後的欄位, 須配合 TwfTradingLineTmp::SendRequest(); 對 TwfRpt 欄位的理解.
   fon9::StrView tag, value;
   while (fon9::StrFetchTagValue(message, tag, value)) {
      if (tag == "Qty") {
         this->Qty_ = fon9::StrTo(value, this->Qty_);
      }
      else if (tag == "Pri") {
         this->Pri_ = fon9::StrTo(value, this->Pri_);
      }
      else if (tag == "PriType") {
         this->PriType_ = value.empty() ? f9fmkt_PriType_Unknown : static_cast<f9fmkt_PriType>(*value.begin());
      }
   }
   return true;
}

} // namespaces
