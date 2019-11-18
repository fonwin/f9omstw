// \file f9omstws/OmsTwsFilled.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsFilled_hpp__
#define __f9omstws_OmsTwsFilled_hpp__
#include "f9omstw/OmsReportFilled.hpp"
#include "f9omstw/OmsReportFactory.hpp"
#include "f9omstw/OmsReportTools.hpp"
#include "f9omstws/OmsTwsTypes.hpp"

namespace f9omstw {

class OmsTwsFilled : public OmsReportFilled {
   fon9_NON_COPY_NON_MOVE(OmsTwsFilled);
   using base = OmsReportFilled;

   char* RevFilledReqUID(char* pout) override;
   void RunReportInCore_FilledOrder(OmsReportChecker&& checker, OmsOrder& order) override;
   OmsOrderRaw* RunReportInCore_FilledMakeOrder(OmsReportChecker& checker) override;
   bool RunReportInCore_FilledIsFieldsMatch(const OmsRequestIni& ini) const override;
   void RunReportInCore_FilledUpdateCum(OmsReportRunnerInCore&& inCoreRunner) const override;

public:
   fon9::DayTime     Time_;
   OmsTwsPri         Pri_;
   OmsTwsQty         Qty_;
   
   IvacNo            IvacNo_;
   OmsTwsSymbol      Symbol_;
   f9fmkt_Side       Side_;
   OmsReportQtyStyle QtyStyle_{OmsReportQtyStyle::BySessionId};
   char              padding__[4];

   using base::base;

   static OmsRequestSP MakeReportIn(OmsRequestFactory& creator, f9fmkt_RxKind reqKind) {
      (void)reqKind; assert(reqKind == f9fmkt_RxKind_Filled || reqKind == f9fmkt_RxKind_Unknown);
      return new OmsTwsFilled{creator};
   }

   static void MakeFields(fon9::seed::Fields& flds);
};

using OmsTwsFilledFactory = OmsReportFactoryT<OmsTwsFilled>;
using OmsTwsFilledFactorySP = fon9::intrusive_ptr<OmsTwsFilledFactory>;

} // namespaces
#endif//__f9omstws_OmsTwsFilled_hpp__
