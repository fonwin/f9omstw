// \file f9omstwf/OmsTwfReport3.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfReport3_hpp__
#define __f9omstwf_OmsTwfReport3_hpp__
#include "f9omstw/OmsReportFactory.hpp"
#include "f9omstwf/OmsTwfRequest0.hpp"

namespace f9omstw {

/// 處理 Twf 的「R03失敗」回報.
class OmsTwfReport3 : public OmsRequestBase {
   fon9_NON_COPY_NON_MOVE(OmsTwfReport3);
   using base = OmsRequestBase;

protected:
   void RunReportInCore_FromOrig(OmsReportChecker&& checker, const OmsRequestBase& origReq) override;
   void RunReportInCore_MakeReqUID() override;
   void RunReportInCore_Order(OmsReportChecker&& checker, OmsOrder& order) override;
   void RunReportInCore_OrderNotFound(OmsReportChecker&& checker, OmsOrdNoMap& ordnoMap) override;

public:
   fon9::TimeStamp   ExgTime_{fon9::TimeStamp::Null()};
   f9fmkt_Side       Side_{};
   char              Padding___[7];

   using base::base;

   void ProcessPendingReport(const OmsRequestRunnerInCore& prevRunner) const override;

   static OmsRequestSP MakeReportIn(OmsRequestFactory& creator, f9fmkt_RxKind reqKind) {
      auto retval = new OmsTwfReport3{creator, reqKind};
      retval->InitializeForReportIn();
      retval->SetForceInternal();
      return retval;
   }

   static void MakeFields(fon9::seed::Fields& flds);
};

using OmsTwfReport3Factory = OmsReportFactoryT<OmsTwfReport3>;
using OmsTwfReport3FactorySP = fon9::intrusive_ptr<OmsTwfReport3Factory>;

} // namespaces
#endif//__f9omstwf_OmsTwfReport3_hpp__
