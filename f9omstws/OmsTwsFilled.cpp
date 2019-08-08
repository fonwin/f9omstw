// \file f9omstws/OmsTwsFilled.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsFilled.hpp"
#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsCore.hpp"
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
bool OmsTwsFilled::CheckFields(const OmsTwsRequestIni& ini) const {
   return(this->IvacNo_ == ini.IvacNo_
          && this->Side_ == ini.Side_
          && this->Symbol_ == ini.Symbol_);
}
void OmsTwsFilled::RunReportInCore(OmsReportRunner&& runner) {
   OmsOrdNoMap* ordnoMap = runner.GetOrdNoMap();
   if (ordnoMap == nullptr)
      return;
   // ReqUID = Market + SessionId + BrkId[後2碼] + MatchKey.
   fon9::NumOutBuf nbuf;
   char*           pout = nbuf.end();
   memset(pout -= sizeof(this->ReqUID_), 0, sizeof(this->ReqUID_));
   pout = fon9::ToStrRev(pout, this->MatchKey_);
   if (this->Side_ != f9fmkt_Side{})
      *--pout = this->Side_;
   this->ReqUID_.Chars_[0] = this->Market();
   this->ReqUID_.Chars_[1] = this->SessionId();
   this->ReqUID_.Chars_[2] = this->BrkId_[sizeof(f9tws::BrkId) - 2];
   this->ReqUID_.Chars_[3] = this->BrkId_[sizeof(f9tws::BrkId) - 1];
   memcpy(this->ReqUID_.Chars_ + 4, pout, sizeof(this->ReqUID_) - 4);

   OmsOrder* order;
   if (fon9_UNLIKELY((order = ordnoMap->GetOrder(this->OrdNo_)) == nullptr)) {
      // 成交找不到 order: 等候新單回報.
      assert(this->Creator().OrderFactory_.get() != nullptr);
      OmsOrderFactory* ordfac = this->Creator().OrderFactory_.get();
      if (ordfac == nullptr) {
         runner.ReportAbandon("OmsTwsFilled: No OrderFactory.");
         return;
      }
      OmsRequestRunnerInCore inCoreRunner{std::move(runner), *ordfac->MakeOrder(*this, nullptr)};
      assert(dynamic_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_) != nullptr);
      runner.Resource_.Backend_.FetchSNO(*this);
      inCoreRunner.OrderRaw_.Order().InsertFilled(this);
      OmsTwsOrderRaw& ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
      ordraw.OrdNo_ = this->OrdNo_;
      ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_ReportPending;
      inCoreRunner.Update(f9fmkt_TradingRequestSt_Filled);
      ordnoMap->EmplaceOrder(ordraw);
      return;
   }
   const OmsTwsRequestIni* iniReq = static_cast<const OmsTwsRequestIni*>(order->Initiator());
   if (iniReq && !this->CheckFields(*iniReq)) {
      runner.ReportAbandon("TwsFilled field not match.");
      return;
   }
   // 加入 order 的成交串列.
   if (order->InsertFilled(this) != nullptr) {
      runner.ReportAbandon("Duplicate MatchKey in TwsFilled");
      return;
   }
   // 更新 order.
   OmsRequestRunnerInCore inCoreRunner{std::move(runner), *order->BeginUpdate(*this)};
   inCoreRunner.Resource_.Backend_.FetchSNO(*this);
   if (iniReq)
      this->IniSNO_ = iniReq->RxSNO();

   if (fon9_LIKELY(order->LastOrderSt() >= f9fmkt_OrderSt_NewDone))
      this->UpdateCum(std::move(inCoreRunner));
   else { // 新單尚未回報, 保留此回報, 等新單確認.
      inCoreRunner.OrderRaw_.UpdateOrderSt_ = f9fmkt_OrderSt_ReportPending;
      inCoreRunner.Update(f9fmkt_TradingRequestSt_Filled, nullptr);
   }
}
void OmsTwsFilled::UpdateCum(OmsRequestRunnerInCore&& inCoreRunner) const {
   // 成交異動, 更新 CumQty, CumAmt, LeavesQty.
   OmsTwsOrderRaw&   ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
   ordraw.LastFilledTime_ = this->Time_;
   ordraw.CumQty_ += this->Qty_;
   ordraw.CumAmt_ += OmsTwsAmt{this->Pri_} *this->Qty_;
   if (fon9::signed_cast(ordraw.AfterQty_ -= this->Qty_) < 0) {
      fon9::RevPrint(inCoreRunner.ExLogForUpd_, "Bad.After=", fon9::signed_cast(ordraw.AfterQty_),
                     fon9_kCSTR_ROWSPL fon9_kCSTR_CELLSPL);
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
   OmsRequestRunnerInCore  inCoreRunner{res, *order.BeginUpdate(*this)};
   if (ini && this->CheckFields(*ini))
      this->UpdateCum(std::move(inCoreRunner));
   else {
      inCoreRunner.OrderRaw_.ErrCode_ = OmsErrCode_FieldNotMatch;
   }
}

} // namespaces
