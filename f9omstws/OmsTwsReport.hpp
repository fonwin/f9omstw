// \file f9omstws/OmsTwsReport.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsReport_hpp__
#define __f9omstws_OmsTwsReport_hpp__
#include "f9omstw/OmsReportFilled.hpp"
#include "f9omstws/OmsTwsRequest.hpp"

namespace f9omstw {

/// Tws 的「新刪改查」回報, 透過此處處理.
class OmsTwsReport : public OmsTwsRequestIni {
   fon9_NON_COPY_NON_MOVE(OmsTwsReport);
   using base = OmsTwsRequestIni;

public:
   fon9::DayTime     ExgTime_{fon9::DayTime::Null()};
   fon9::CharVector  Message_;
   OmsTwsQty         BeforeQty_{};
   char              padding__[4];

   using base::base;

   void RunReportInCore(OmsReportRunner&& runner) override;
   void ProcessPendingReport(OmsResource& res) const override;

   static OmsRequestSP MakeReport(OmsRequestFactory& creator, f9fmkt_RxKind reqKind) {
      auto retval = new OmsTwsReport{creator, reqKind};
      retval->InitializeForReport();
      return retval;
   }

   static void MakeFields(fon9::seed::Fields& flds);
};

class OmsTwsFilled : public OmsReportFilled {
   fon9_NON_COPY_NON_MOVE(OmsTwsFilled);
   using base = OmsReportFilled;

public:
   fon9::DayTime  Time_;
   OmsTwsPri      Pri_;
   OmsTwsQty      Qty_;

   using base::base;

   void RunReportInCore(OmsReportRunner&& runner) override;
   void ProcessPendingReport(OmsResource& res) const override;

   static OmsRequestSP MakeReport(OmsRequestFactory& creator, f9fmkt_RxKind reqKind) {
      (void)reqKind; assert(reqKind == f9fmkt_RxKind_Filled || reqKind == f9fmkt_RxKind_Unknown);
      return new OmsTwsFilled{creator};
   }

   static void MakeFields(fon9::seed::Fields& flds);
};

} // namespaces
#endif//__f9omstws_OmsTwsReport_hpp__
