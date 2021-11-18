// \file f9omstwf/OmsTwfReport8.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfReport8_hpp__
#define __f9omstwf_OmsTwfReport8_hpp__
#include "f9omstw/OmsReportFactory.hpp"
#include "f9omstwf/OmsTwfRequest7.hpp"

namespace f9omstw {

/// Twf 的「詢價」回報(R08/R38), 透過此處處理.
class OmsTwfReport8 : public OmsTwfRequestIni7 {
   fon9_NON_COPY_NON_MOVE(OmsTwfReport8);
   using base = OmsTwfRequestIni7;
   void RunReportInCore_FromOrig(OmsReportChecker&& checker, const OmsRequestBase& origReq) override;
   void RunReportInCore_MakeReqUID() override;
   void RunReportInCore_Order(OmsReportChecker&& checker, OmsOrder& order) override;
   void RunReportInCore_NewOrder(OmsReportRunnerInCore&& runner) override;

public:
   fon9::TimeStamp         ExgTime_{fon9::TimeStamp::Null()};
   f9twf::TmpSessionId_t   OutPvcId_{};
   char                    Padding____[6];

   using base::base;

   // 詢價只有成功或失敗, 不可能有 Pending.
   // void ProcessPendingReport(OmsResource& res) override;

   static OmsRequestSP MakeReportIn(OmsRequestFactory& creator, f9fmkt_RxKind reqKind) {
      auto retval = new OmsTwfReport8{creator, reqKind};
      retval->InitializeForReportIn();
      return retval;
   }

   static void MakeFields(fon9::seed::Fields& flds);
};

using OmsTwfReport8Factory = OmsReportFactoryT<OmsTwfReport8>;
using OmsTwfReport8FactorySP = fon9::intrusive_ptr<OmsTwfReport8Factory>;

} // namespaces
#endif//__f9omstwf_OmsTwfReport8_hpp__
