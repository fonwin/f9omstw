// \file f9omstws/OmsTwsFilled.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsFilled.hpp"
#include "f9omstws/OmsTwsRequest.hpp"
#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwsFilled::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwsFilled>(flds);
   flds.Add(fon9_MakeField2(OmsTwsFilled, Time));
   flds.Add(fon9_MakeField2(OmsTwsFilled, Pri));
   flds.Add(fon9_MakeField2(OmsTwsFilled, Qty));
   flds.Add(fon9_MakeField2(OmsTwsFilled, IvacNo));
   flds.Add(fon9_MakeField2(OmsTwsFilled, Symbol));
   flds.Add(fon9_MakeField2(OmsTwsFilled, Side));
}

static void AdjustQtyUnit(OmsOrder& order, OmsResource& res, OmsTwsFilled& rpt) {
   if (rpt.SessionId() == f9fmkt_TradingSessionId_OddLot)
      return;
   auto shUnit = fon9::fmkt::GetTwsSymbShUnit(order.GetSymb(res, rpt.Symbol_));
   if ((rpt.Qty_ % shUnit) != 0)
      rpt.Qty_ *= shUnit;
}
static void AssignFilledReqUID(OmsTwsFilled& rpt) {
   // ReqUID = Market + SessionId + BrkId[後2碼] + MatchKey.
   fon9::NumOutBuf nbuf;
   char*           pout = nbuf.end();
   memset(pout -= sizeof(rpt.ReqUID_), 0, sizeof(rpt.ReqUID_));
   pout = fon9::ToStrRev(pout, rpt.MatchKey_);
   if (rpt.Side_ != f9fmkt_Side{})
      *--pout = rpt.Side_;
   rpt.ReqUID_.Chars_[0] = rpt.Market();
   rpt.ReqUID_.Chars_[1] = rpt.SessionId();
   rpt.ReqUID_.Chars_[2] = rpt.BrkId_[sizeof(f9tws::BrkId) - 2];
   rpt.ReqUID_.Chars_[3] = rpt.BrkId_[sizeof(f9tws::BrkId) - 1];
   memcpy(rpt.ReqUID_.Chars_ + 4, pout, sizeof(rpt.ReqUID_) - 4);
}
bool OmsTwsFilled::CheckFields(const OmsTwsRequestIni& ini) const {
   return(this->IvacNo_ == ini.IvacNo_
          && this->Side_ == ini.Side_
          && this->Symbol_ == ini.Symbol_);
}
void OmsTwsFilled::RunOrderFilled(OmsReportChecker&& checker, OmsOrder& order) {
   const OmsTwsRequestIni* iniReq = static_cast<const OmsTwsRequestIni*>(order.Initiator());
   if (iniReq && !this->CheckFields(*iniReq)) {
      checker.ReportAbandon("TwsFilled field not match.");
      return;
   }
   // 加入 order 的成交串列.
   if (order.InsertFilled(this) != nullptr) {
      checker.ReportAbandon("Duplicate MatchKey in TwsFilled");
      return;
   }
   // 更新 order.
   OmsReportRunnerInCore inCoreRunner{std::move(checker), *order.BeginUpdate(*this)};
   AdjustQtyUnit(order, inCoreRunner.Resource_, *this);
   inCoreRunner.Resource_.Backend_.FetchSNO(*this);
   if (iniReq)
      this->IniSNO_ = iniReq->RxSNO();

   if (fon9_LIKELY(order.LastOrderSt() >= f9fmkt_OrderSt_NewDone))
      this->UpdateCum(std::move(inCoreRunner));
   else { // 新單尚未回報, 保留此回報, 等新單確認.
      inCoreRunner.OrderRaw_.UpdateOrderSt_ = f9fmkt_OrderSt_ReportPending;
      inCoreRunner.Update(f9fmkt_TradingRequestSt_Filled, nullptr);
   }
}
void OmsTwsFilled::RunReportInCore(OmsReportChecker&& checker) {
   OmsOrder* order;
   if (!this->ReqUID_.empty1st()) {
      // 嘗試使用 ReqUID 取得 origReq.
      if (const OmsRequestBase* origReq = checker.SearchOrigRequestId())
         if (auto ordraw = origReq->LastUpdated()) {
            AssignFilledReqUID(*this);
            RunOrderFilled(std::move(checker), ordraw->Order());
            return;
         }
   }
   OmsOrdNoMap* ordnoMap = checker.GetOrdNoMap();
   if (ordnoMap == nullptr)
      return;
   AssignFilledReqUID(*this);
   if (fon9_UNLIKELY((order = ordnoMap->GetOrder(this->OrdNo_)) != nullptr)) {
      RunOrderFilled(std::move(checker), *order);
      return;
   }
   // 成交找不到 order: 等候新單回報.
   assert(this->Creator().OrderFactory_.get() != nullptr);
   OmsOrderFactory* ordfac = this->Creator().OrderFactory_.get();
   if (ordfac == nullptr) {
      checker.ReportAbandon("OmsTwsFilled: No OrderFactory.");
      return;
   }
   OmsReportRunnerInCore inCoreRunner{std::move(checker), *ordfac->MakeOrder(*this, nullptr)};
   assert(dynamic_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_) != nullptr);
   inCoreRunner.Resource_.Backend_.FetchSNO(*this);
   OmsTwsOrderRaw& ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
   ordraw.Order().InsertFilled(this);
   AdjustQtyUnit(ordraw.Order(), inCoreRunner.Resource_, *this);
   ordraw.OrdNo_ = this->OrdNo_;
   ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_ReportPending;
   inCoreRunner.Update(f9fmkt_TradingRequestSt_Filled);
   ordnoMap->EmplaceOrder(ordraw);
}
void OmsTwsFilled::UpdateCum(OmsReportRunnerInCore&& inCoreRunner) const {
   // 成交異動, 更新 CumQty, CumAmt, LeavesQty.
   OmsTwsOrderRaw&   ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
   ordraw.LastFilledTime_ = this->Time_;
   ordraw.CumQty_ += this->Qty_;
   ordraw.CumAmt_ += OmsTwsAmt{this->Pri_} *this->Qty_;
   if (fon9::signed_cast(ordraw.AfterQty_ -= this->Qty_) < 0) {
      fon9::RevPrint(inCoreRunner.ExLogForUpd_, "Bad.After=", fon9::signed_cast(ordraw.AfterQty_),
                     fon9_kCSTR_ROWSPL ">" fon9_kCSTR_CELLSPL);
      ordraw.AfterQty_ = 0;
   }
   ordraw.UpdateOrderSt_ = ((ordraw.LeavesQty_ = ordraw.AfterQty_) <= 0
                            ? f9fmkt_OrderSt_FullFilled : f9fmkt_OrderSt_PartFilled);
   inCoreRunner.Update(f9fmkt_TradingRequestSt_Filled, nullptr);
}
void OmsTwsFilled::ProcessPendingReport(OmsResource& res) const {
   assert(this->LastUpdated()->UpdateOrderSt_ == f9fmkt_OrderSt_ReportPending);
   OmsOrder& order = this->LastUpdated()->Order();
   if (order.LastOrderSt() < f9fmkt_OrderSt_NewDone)
      return;
   const OmsTwsRequestIni* ini = dynamic_cast<const OmsTwsRequestIni*>(order.Initiator());
   OmsReportRunnerInCore   inCoreRunner{res, *order.BeginUpdate(*this)};
   if (ini && this->CheckFields(*ini))
      this->UpdateCum(std::move(inCoreRunner));
   else {
      inCoreRunner.OrderRaw_.ErrCode_ = OmsErrCode_FieldNotMatch;
   }
}

} // namespaces
