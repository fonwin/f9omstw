// \file f9omstwf/OmsTwfFilled.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfFilled.hpp"
#include "f9omstwf/OmsTwfRequest1.hpp"
#include "f9omstwf/OmsTwfOrder1.hpp"
#include "f9omstwf/OmsTwfOrder9.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

#define kERCODE_Canceling    40047

void OmsTwfFilled::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwfFilled>(flds);
   flds.Add(fon9_MakeField2(OmsTwfFilled, Time));
   flds.Add(fon9_MakeField2(OmsTwfFilled, Pri));
   flds.Add(fon9_MakeField2(OmsTwfFilled, Qty));
   flds.Add(fon9_MakeField2(OmsTwfFilled, IvacNo));
   flds.Add(fon9_MakeField2(OmsTwfFilled, Symbol));
   flds.Add(fon9_MakeField2(OmsTwfFilled, Side));
   flds.Add(fon9_MakeField2(OmsTwfFilled, PosEff));
   flds.Add(fon9_MakeField2(OmsTwfFilled, QtyCanceled));
   flds.Add(fon9_MakeField2(OmsTwfFilled, PriOrd));
   base::AddFieldsErrCode(flds);
}
void OmsTwfFilled2::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields(flds);
   flds.Add(fon9_MakeField2(OmsTwfFilled2, PriLeg2));
}
//-///////////////////////////////////////////////////////////////////////////
static inline bool AdjustReportPrice(int32_t priMul, OmsTwfFilled1& rpt) {
   switch (priMul) {
   case 0:  return true;
   case -1: return false;
   default:
      assert(priMul > 0);
      rpt.Pri_ *= priMul;
      rpt.PriOrd_ *= priMul;
      return true;
   }
}
static inline bool AdjustReportPrice(int32_t priMul, OmsTwfFilled2& rpt) {
   switch (priMul) {
   case 0:  return true;
   case -1: return false;
   default:
      rpt.Pri_     *= priMul;
      rpt.PriOrd_  *= priMul;
      rpt.PriLeg2_ *= priMul;
      return true;
   }
}
//-///////////////////////////////////////////////////////////////////////////
char* OmsTwfFilled::RevFilledReqUID(char* pout) {
   if (this->Side_ != f9fmkt_Side{})
      *--pout = this->Side_;
   memcpy(pout -= 3, this->BrkId_.end() - 3, 3);
   return pout;
}
bool OmsTwfFilled::RunReportInCore_FilledIsFieldsMatch(const OmsRequestIni& ini) const {
   assert(dynamic_cast<const OmsTwfRequestIni0*>(&ini) != nullptr);
   if ((this->IvacNo_ != 0 && this->IvacNo_ != ini.IvacNo_)
       || (!this->Symbol_.empty() && this->Symbol_ != static_cast<const OmsTwfRequestIni0*>(&ini)->Symbol_))
      return false;
   if (fon9_LIKELY(this->PosEff_ != OmsTwfPosEff::Quote)) {
      assert(dynamic_cast<const OmsTwfRequestIni1*>(&ini) != nullptr);
      if (this->Side_ != f9fmkt_Side_Unknown)
         return(this->Side_ == static_cast<const OmsTwfRequestIni1*>(&ini)->Side_);
   }
   return true;
}
void OmsTwfFilled::RunReportInCore_FilledBeforeNewDone(OmsResource& resource, OmsOrder& order) {
   assert(order.LastOrderSt() < f9fmkt_OrderSt_NewDone && order.Initiator() != nullptr);
   if (fon9_UNLIKELY(this->PosEff_ == OmsTwfPosEff::Quote && this->Side_ == f9fmkt_Side_Buy))
      return;
   OmsReportRunnerInCore inCoreRunner{resource, *order.BeginUpdate(*order.Initiator())};
   if (fon9_LIKELY(this->PosEff_ != OmsTwfPosEff::Quote)) {
      assert(dynamic_cast<OmsTwfOrderRaw1*>(&inCoreRunner.OrderRaw_) != nullptr);
      // 新單補單後的處理,不會走到這兒,會在 RunReportInCore_FilledUpdateCum() 更新 LastPri_;
      if (!this->PriOrd_.IsNullOrZero())
         static_cast<OmsTwfOrderRaw1*>(&inCoreRunner.OrderRaw_)->LastPri_ = this->PriOrd_;
      if (static_cast<OmsTwfOrderRaw1*>(&inCoreRunner.OrderRaw_)->LastPriTime_.IsNull())
         static_cast<OmsTwfOrderRaw1*>(&inCoreRunner.OrderRaw_)->LastPriTime_ = this->Time_;
   }
   else { // 報價.
      assert(dynamic_cast<OmsTwfOrderRaw9*>(&inCoreRunner.OrderRaw_) != nullptr);
   }
   static_cast<OmsTwfOrderRaw0*>(&inCoreRunner.OrderRaw_)->LastExgTime_ = this->Time_;
   inCoreRunner.UpdateSt(f9fmkt_OrderSt_ExchangeAccepted, f9fmkt_TradingRequestSt_ExchangeAccepted);
}
bool OmsTwfFilled::RunReportInCore_FilledIsNeedsReportPending(const OmsOrderRaw& lastOrdUpd) const {
   if (this->Qty_ > 0 && this->QtyCanceled_ <= 0) // 沒有取消量, 不需要 ReportPending;
      return false;
   assert((this->PosEff_ == OmsTwfPosEff::Quote && dynamic_cast<const OmsTwfOrderRaw9*>(&lastOrdUpd) != nullptr)
       || (this->PosEff_ != OmsTwfPosEff::Quote && dynamic_cast<const OmsTwfOrderRaw1*>(&lastOrdUpd) != nullptr));
   OmsTwfQty leaves = (this->PosEff_ != OmsTwfPosEff::Quote
                       ? static_cast<const OmsTwfOrderRaw1*>(&lastOrdUpd)->LeavesQty_
                       : (this->Side_ == f9fmkt_Side_Buy
                          ? static_cast<const OmsTwfOrderRaw9*>(&lastOrdUpd)->Bid_.LeavesQty_
                          : static_cast<const OmsTwfOrderRaw9*>(&lastOrdUpd)->Offer_.LeavesQty_));
   return leaves > (this->QtyCanceled_ + this->Qty_);
}
OmsOrderRaw* OmsTwfFilled::RunReportInCore_FilledMakeOrder(OmsReportChecker& checker) {
   if (this->IsAbandonOrderNotFound_) {
      checker.ReportAbandon("OmsTwfFilled: Order not found.");
      return nullptr;
   }
   return base::RunReportInCore_FilledMakeOrder(checker);
}
//--------------------------------------------------------------------------//
void OmsTwfFilled::RunReportInCore(OmsReportChecker&& checker) {
   if (fon9_UNLIKELY(this->Qty_ == 0)) {
      this->SetReportSt(this->ErrCode() == kERCODE_Canceling
                        ? f9fmkt_TradingRequestSt_ExchangeCanceling
                        : f9fmkt_TradingRequestSt_ExchangeCanceled);
   }
   base::RunReportInCore(std::move(checker));
   this->CheckPartFilledQtyCanceled(checker.Resource_);
}
void OmsTwfFilled::ProcessPendingReport(OmsResource& res) const {
   base::ProcessPendingReport(res);
   this->CheckPartFilledQtyCanceled(res);
}
void OmsTwfFilled::CheckPartFilledQtyCanceled(OmsResource& res) const {
   if (this->QtyCanceled_ <= 0 || this->Qty_ <= 0)
      return;
   const OmsOrderRaw* lastupd = this->LastUpdated();
   if (lastupd && lastupd->UpdateOrderSt_ != f9fmkt_OrderSt_ReportPending) {
      OmsOrder& order = lastupd->Order();
      // 這裡要用 order.BeginUpdate(*order.Initiator()) 或 order.BeginUpdate(*this)?
      // 對於委託最終狀態沒有影響, 使用 order.BeginUpdate(*this) 較為合理, 且可傳遞 ErrCode;
      //this->ProcessQtyCanceled(OmsReportRunnerInCore{res, *order.BeginUpdate(*order.Initiator())});
      this->ProcessQtyCanceled(OmsReportRunnerInCore{res, *order.BeginUpdate(*this)});
   }
}
void OmsTwfFilled::ProcessQtyCanceled(OmsReportRunnerInCore&& inCoreRunner) const {
   // assert(this->QtyCanceled_ > 0); 有可能是 40048 的 Canceled: Qty_ == QtyCanceled_ == 0;
   if (fon9_LIKELY(this->PosEff_ != OmsTwfPosEff::Quote)) {
      assert(dynamic_cast<OmsTwfOrderRaw1*>(&inCoreRunner.OrderRaw_) != nullptr);
      OmsTwfOrderRaw1&  ordraw = *static_cast<OmsTwfOrderRaw1*>(&inCoreRunner.OrderRaw_);
      assert(this->QtyCanceled_ == ordraw.LeavesQty_);
      ordraw.LastExgTime_ = this->Time_;
      ordraw.LeavesQty_ = ordraw.AfterQty_ = 0;
      if(this->ErrCode() == kERCODE_Canceling)
         inCoreRunner.UpdateSt(f9fmkt_OrderSt_Canceling, f9fmkt_TradingRequestSt_ExchangeCanceling);
      else
         inCoreRunner.UpdateSt(f9fmkt_OrderSt_ExchangeCanceled, f9fmkt_TradingRequestSt_ExchangeCanceled);
   }
   else { // 期交所自動取消報價.
      assert(dynamic_cast<OmsTwfOrderRaw9*>(&inCoreRunner.OrderRaw_) != nullptr);
      OmsTwfOrderRaw9&  ordraw = *static_cast<OmsTwfOrderRaw9*>(&inCoreRunner.OrderRaw_);
      ordraw.QuoteReportSide_ = this->Side_;
      ordraw.LastExgTime_ = this->Time_;
      switch (this->Side_) {
      case f9fmkt_Side_Buy:
         assert(this->QtyCanceled_ == ordraw.Bid_.LeavesQty_);
         ordraw.Bid_.AfterQty_ = ordraw.Bid_.LeavesQty_ = 0;
         break;
      case f9fmkt_Side_Sell:
         assert(this->QtyCanceled_ == ordraw.Offer_.LeavesQty_);
         ordraw.Offer_.AfterQty_ = ordraw.Offer_.LeavesQty_ = 0;
         break;
      case f9fmkt_Side_Unknown:
      default:
         inCoreRunner.OrderRaw_.Message_.assign("TwfFilled(Canceled): Unknown QuodeSide.");
         return;
      }
      inCoreRunner.UpdateSt((ordraw.Bid_.LeavesQty_ <= 0 && ordraw.Offer_.LeavesQty_ <= 0
                             ? f9fmkt_OrderSt_ExchangeCanceled : ordraw.Order().LastOrderSt()),
                            f9fmkt_TradingRequestSt_ExchangeCanceled);
   }
}
//--------------------------------------------------------------------------//
void OmsTwfFilled::OnSynReport(const OmsRequestBase* ref, fon9::StrView message) {
   base::OnSynReport(ref, message);
   this->PriStyle_ = OmsReportPriStyle::HasDecimal;
   if (ref) {
      auto ini = dynamic_cast<const OmsTwfRequestIni0*>(ref);
      if (fon9_LIKELY(ini != nullptr)) {
      ___COPY_FROM_INI:;
         this->Symbol_ = ini->Symbol_;
         this->IvacNo_ = ini->IvacNo_;
         // this->Side_;
         // this->PosEff_;
         // this->QtyCanceled_;
         return;
      }
      if (auto* ordraw = ref->LastUpdated()) {
         if ((ini = dynamic_cast<const OmsTwfRequestIni0*>(ordraw->Order().Initiator())) != nullptr)
            goto ___COPY_FROM_INI;
      }
      this->Symbol_.clear();
      this->IvacNo_ = IvacNo{};
   }
}
//-///////////////////////////////////////////////////////////////////////////
OmsOrderRaw* OmsTwfFilled1::RunReportInCore_FilledMakeOrder(OmsReportChecker& checker) {
   if (!AdjustReportPrice(OmsGetReportPriMul(checker, *this), *this))
      return nullptr;
   OmsOrderRaw* ordraw = nullptr;
   if (fon9_LIKELY(this->PosEff_ != OmsTwfPosEff::Quote))
      ordraw = base::RunReportInCore_FilledMakeOrder(checker);
   else {
      assert(dynamic_cast<OmsTwfFilled1Factory*>(&this->Creator()) != nullptr);
      assert(static_cast<OmsTwfFilled1Factory*>(&this->Creator())->QuoteOrderFactory_.get() != nullptr);
      if (OmsOrderFactory* ordfac = static_cast<OmsTwfFilled1Factory*>(&this->Creator())->QuoteOrderFactory_.get())
         ordraw = ordfac->MakeOrder(*this, nullptr);
      else
         return nullptr;
   }
   return ordraw;
}
void OmsTwfFilled1::RunReportInCore_FilledOrder(OmsReportChecker&& checker, OmsOrder& order) {
   if (AdjustReportPrice(OmsGetReportPriMul(order, checker, *this), *this))
      base::RunReportInCore_FilledOrder(std::move(checker), order);
}
void OmsTwfFilled1::RunReportInCore_FilledUpdateCum(OmsReportRunnerInCore&& inCoreRunner) const {
   if (this->Qty_ == 0) {
      // - 期交所 IOC、FOK 刪單. 且此次回報沒有成交.
      // - 期交所自動取消報價.
      this->ProcessQtyCanceled(std::move(inCoreRunner));
      return;
   }
   if (fon9_LIKELY(this->PosEff_ != OmsTwfPosEff::Quote)) {
      // 一般單式商品.
      assert(dynamic_cast<OmsTwfOrderRaw1*>(&inCoreRunner.OrderRaw_) != nullptr);
      OmsTwfOrderRaw1&  ordraw = *static_cast<OmsTwfOrderRaw1*>(&inCoreRunner.OrderRaw_);
      ordraw.LastFilledTime_ = this->Time_;
      OmsRunReportInCore_FilledUpdateCum(std::move(inCoreRunner), ordraw, this->Pri_, this->Qty_, *this);
   }
   else { // 報價成交.
      assert(dynamic_cast<OmsTwfOrderRaw9*>(&inCoreRunner.OrderRaw_) != nullptr);
      OmsTwfOrderRaw9&  ordraw = *static_cast<OmsTwfOrderRaw9*>(&inCoreRunner.OrderRaw_);
      ordraw.QuoteReportSide_ = this->Side_;
      ordraw.LastFilledTime_ = this->Time_;
      switch (this->Side_) {
      case f9fmkt_Side_Buy:
         OmsFilledUpdateCumFromReport(inCoreRunner, ordraw.Bid_, this->Pri_, this->Qty_);
         break;
      case f9fmkt_Side_Sell:
         OmsFilledUpdateCumFromReport(inCoreRunner, ordraw.Offer_, this->Pri_, this->Qty_);
         break;
      case f9fmkt_Side_Unknown:
      default:
         inCoreRunner.OrderRaw_.Message_.assign("TwfFilled1: Unknown QuodeSide.");
         inCoreRunner.UpdateFilled(f9fmkt_OrderSt_ExchangeAccepted, *this);
         return;
      }
      inCoreRunner.UpdateFilled(ordraw.Bid_.LeavesQty_ <= 0 && ordraw.Offer_.LeavesQty_ <= 0
                                ? f9fmkt_OrderSt_FullFilled : f9fmkt_OrderSt_PartFilled,
                                *this);
   }
}
//-///////////////////////////////////////////////////////////////////////////
OmsOrderRaw* OmsTwfFilled2::RunReportInCore_FilledMakeOrder(OmsReportChecker& checker) {
   if (!AdjustReportPrice(OmsGetReportPriMul(checker, *this), *this))
      return nullptr;
   assert(this->PosEff_ != OmsTwfPosEff::Quote);
   return base::RunReportInCore_FilledMakeOrder(checker);
}
void OmsTwfFilled2::RunReportInCore_FilledOrder(OmsReportChecker&& checker, OmsOrder& order) {
   if (AdjustReportPrice(OmsGetReportPriMul(order, checker, *this), *this))
      base::RunReportInCore_FilledOrder(std::move(checker), order);
}
void OmsTwfFilled2::RunReportInCore_FilledUpdateCum(OmsReportRunnerInCore&& inCoreRunner) const {
   if (this->Qty_ == 0) {
      this->ProcessQtyCanceled(std::move(inCoreRunner));
      return;
   }
   assert(this->PosEff_ != OmsTwfPosEff::Quote);
   assert(dynamic_cast<OmsTwfOrderRaw1*>(&inCoreRunner.OrderRaw_) != nullptr);
   assert(dynamic_cast<const OmsTwfRequestIni0*>(inCoreRunner.OrderRaw_.Order().Initiator()) != nullptr);
   // 一般複式商品.
   OmsTwfOrderRaw1&         ordraw = *static_cast<OmsTwfOrderRaw1*>(&inCoreRunner.OrderRaw_);
   const OmsTwfRequestIni0& reqini = *static_cast<const OmsTwfRequestIni0*>(ordraw.Order().Initiator());
   ordraw.LastFilledTime_ = this->Time_;

   fon9_GCC_WARN_DISABLE("-Wconversion");
   ordraw.Leg1CumAmt_ += OmsTwfAmt(this->Pri_) * this->Qty_;
   ordraw.Leg1CumQty_ += this->Qty_;
   ordraw.Leg2CumAmt_ += OmsTwfAmt(this->PriLeg2_) * this->Qty_;
   ordraw.Leg2CumQty_ += this->Qty_;
   fon9_GCC_WARN_POP;

   OmsRunReportInCore_FilledUpdateCum(std::move(inCoreRunner), ordraw,
                                      this->GetFilledPri(reqini.CombSide(inCoreRunner.Resource_)),
                                      this->Qty_,
                                      *this);
}

} // namespaces
