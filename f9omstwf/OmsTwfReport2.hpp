// \file f9omstwf/OmsTwfReport2.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfReport2_hpp__
#define __f9omstwf_OmsTwfReport2_hpp__
#include "f9omstw/OmsReportFactory.hpp"
#include "f9omstw/OmsReportTools.hpp"
#include "f9omstwf/OmsTwfRequest1.hpp"

namespace f9omstw {

/// Twf 的「新刪改查」回報(R02/R32/R22).
/// this->Qty_ = R2.LeavesQty_; AfterQty;
/// this->Price_.GetOrigValue() = R2.Price_; 尚未處理小數部分.
class OmsTwfReport2 : public OmsTwfRequestIni1 {
   fon9_NON_COPY_NON_MOVE(OmsTwfReport2);
   using base = OmsTwfRequestIni1;
   void RunReportInCore_FromOrig(OmsReportChecker&& checker, const OmsRequestBase& origReq) override;
   void RunReportInCore_MakeReqUID() override;
   void RunReportInCore_Order(OmsReportChecker&& checker, OmsOrder& order) override;
   void RunReportInCore_OrderNotFound(OmsReportChecker&& checker, OmsOrdNoMap& ordnoMap) override;
   void RunReportInCore_InitiatorNew(OmsReportRunnerInCore&& inCoreRunner) override;
   void RunReportInCore_DCQ(OmsReportRunnerInCore&& inCoreRunner) override;
   bool RunReportInCore_IsBfAfMatch(const OmsOrderRaw& ordu) override;
   bool RunReportInCore_IsExgTimeMatch(const OmsOrderRaw& ordu) override;

public:
   fon9::TimeStamp   ExgTime_{fon9::TimeStamp::Null()};
   /// this->Qty_ 為 AfterQty;
   OmsTwfQty         BeforeQty_{};
   OmsReportPriStyle PriStyle_{OmsReportPriStyle::NoDecimal};
   char              padding__[5];

   using base::base;

   void ProcessPendingReport(OmsResource& res) const override;

   static OmsRequestSP MakeReportIn(OmsRequestFactory& creator, f9fmkt_RxKind reqKind) {
      auto retval = new OmsTwfReport2{creator, reqKind};
      retval->InitializeForReportIn();
      return retval;
   }

   static void MakeFields(fon9::seed::Fields& flds);
};

using OmsTwfReport2Factory = OmsReportFactoryT<OmsTwfReport2>;
using OmsTwfReport2FactorySP = fon9::intrusive_ptr<OmsTwfReport2Factory>;

} // namespaces
#endif//__f9omstwf_OmsTwfReport2_hpp__
