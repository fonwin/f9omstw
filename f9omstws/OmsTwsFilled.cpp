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

} // namespaces
