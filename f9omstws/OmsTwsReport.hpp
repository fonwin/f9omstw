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

   void RunReportFromOrig(OmsReportRunner&& runner, const OmsRequestBase& origReq);
   void RunReportNew(OmsRequestRunnerInCore&& inCoreRunner);
   void RunReportOrder(OmsRequestRunnerInCore&& inCoreRunner);
public:
   /// 如果有跨日交易(夜盤), 則跨日的回報 ExgTime_ 應加上 TimeInterval_Day(1);
   fon9::DayTime     ExgTime_{fon9::DayTime::Null()};
   fon9::CharVector  Message_;
   OmsTwsQty         BeforeQty_{};
   char              padding__[4];

   using base::base;

   void RunReportInCore(OmsReportRunner&& runner) override;
   void ProcessPendingReport(OmsResource& res) const override;

   static OmsRequestSP MakeReportIn(OmsRequestFactory& creator, f9fmkt_RxKind reqKind) {
      auto retval = new OmsTwsReport{creator, reqKind};
      retval->InitializeForReportIn();
      return retval;
   }

   static void MakeFields(fon9::seed::Fields& flds);
};

} // namespaces
#endif//__f9omstws_OmsTwsReport_hpp__
