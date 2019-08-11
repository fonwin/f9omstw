// \file f9omstws/OmsTwsFilled.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsFilled_hpp__
#define __f9omstws_OmsTwsFilled_hpp__
#include "f9omstw/OmsReportFilled.hpp"
#include "f9omstw/OmsReportFactory.hpp"
#include "f9omstws/OmsTwsRequest.hpp"

namespace f9omstw {

class OmsTwsFilled : public OmsReportFilled {
   fon9_NON_COPY_NON_MOVE(OmsTwsFilled);
   using base = OmsReportFilled;

   // 檢查基本欄位是否一致.
   bool CheckFields(const OmsTwsRequestIni& ini) const;
   void UpdateCum(OmsRequestRunnerInCore&& inCoreRunner) const;
   void RunOrderFilled(OmsReportRunner&& runner, OmsOrder& order);

public:
   fon9::DayTime  Time_;
   OmsTwsPri      Pri_;
   OmsTwsQty      Qty_;
   
   IvacNo         IvacNo_;
   OmsTwsSymbol   Symbol_;
   f9fmkt_Side    Side_;
   char           padding__[5];

   using base::base;

   void RunReportInCore(OmsReportRunner&& runner) override;
   void ProcessPendingReport(OmsResource& res) const override;

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
