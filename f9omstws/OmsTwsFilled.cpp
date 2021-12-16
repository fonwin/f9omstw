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
//--------------------------------------------------------------------------//
static inline void AdjustReportQtys(OmsOrder& order, OmsResource& res, OmsTwsFilled& rpt) {
   if (auto shUnit = OmsGetReportQtyUnit(order, res, rpt)) {
      rpt.QtyStyle_ = OmsReportQtyStyle::OddLot;
      rpt.Qty_ *= shUnit;
   }
}
char* OmsTwsFilled::RevFilledReqUID(char* pout) {
   if (this->Side_ != f9fmkt_Side{})
      *--pout = this->Side_;
   memcpy(pout -= 2, this->BrkId_.begin() + sizeof(f9tws::BrkId) - 2, 2);
   return pout;
}
void OmsTwsFilled::RunReportInCore(OmsReportChecker&& checker) {
   if (fon9_UNLIKELY(this->Market() == f9fmkt_TradingMarket_Unknown)) {
      // 在某些特殊環境,成交回報可能沒提供市場別,
      // 此時只好用 [商品的市場別].
      if (auto symb = checker.Resource_.Symbs_->GetOmsSymb(ToStrView(this->Symbol_)))
         this->SetMarket(symb->TradingMarket_);
   }
   // -----
   base::RunReportInCore(std::move(checker));
}
OmsOrderRaw* OmsTwsFilled::RunReportInCore_FilledMakeOrder(OmsReportChecker& checker) {
   OmsOrderRaw* ordraw = base::RunReportInCore_FilledMakeOrder(checker);
   if (ordraw)
      AdjustReportQtys(ordraw->Order(), checker.Resource_, *this);
   return ordraw;
}
bool OmsTwsFilled::RunReportInCore_FilledIsFieldsMatch(const OmsRequestIni& ini) const {
   assert(dynamic_cast<const OmsTwsRequestIni*>(&ini) != nullptr);
   return(this->IvacNo_ == ini.IvacNo_
          && this->Side_ == static_cast<const OmsTwsRequestIni*>(&ini)->Side_
          && this->Symbol_ == static_cast<const OmsTwsRequestIni*>(&ini)->Symbol_);
}
void OmsTwsFilled::RunReportInCore_FilledOrder(OmsReportChecker&& checker, OmsOrder& order) {
   AdjustReportQtys(order, checker.Resource_, *this);
   base::RunReportInCore_FilledOrder(std::move(checker), order);
}
void OmsTwsFilled::RunReportInCore_FilledUpdateCum(OmsReportRunnerInCore&& inCoreRunner) const {
   OmsTwsOrderRaw&   ordraw = *static_cast<OmsTwsOrderRaw*>(&inCoreRunner.OrderRaw_);
   ordraw.LastFilledTime_ = this->Time_;
   OmsRunReportInCore_FilledUpdateCum(std::move(inCoreRunner), ordraw, this->Pri_, this->Qty_, *this);
}
//--------------------------------------------------------------------------//
void OmsTwsFilled::OnSynReport(const OmsRequestBase* ref, fon9::StrView message) {
   base::OnSynReport(ref, message);
   this->QtyStyle_ = OmsReportQtyStyle::OddLot;
   if (ref) {
      auto ini = dynamic_cast<const OmsTwsRequestIni*>(ref);
      if (fon9_LIKELY(ini != nullptr)) {
      ___COPY_FROM_INI:;
         this->Symbol_ = ini->Symbol_;
         this->IvacNo_ = ini->IvacNo_;
         this->Side_ = ini->Side_;
         return;
      }
      if (auto* ordraw = ref->LastUpdated()) {
         if ((ini = dynamic_cast<const OmsTwsRequestIni*>(ordraw->Order().Initiator())) != nullptr)
            goto ___COPY_FROM_INI;
      }
      this->Symbol_.Clear();
      this->IvacNo_ = IvacNo{};
      this->Side_ = f9fmkt_Side{};
   }
}

} // namespaces
