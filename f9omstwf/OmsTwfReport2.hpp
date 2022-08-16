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
   fon9::TimeStamp         ExgTime_{fon9::TimeStamp::Null()};
   fon9::CharVector        Message_;
   f9twf::TmpSessionId_t   OutPvcId_{};
   /// this->Qty_ 為 AfterQty;
   OmsTwfQty               BeforeQty_{};
   OmsReportPriStyle       PriStyle_{OmsReportPriStyle::NoDecimal};
   char                    padding__[3];

   using base::base;

   void ProcessPendingReport(const OmsRequestRunnerInCore& prevRunner) const override;
   void OnSynReport(const OmsRequestBase* ref, fon9::StrView message) override;

   static OmsRequestSP MakeReportIn(OmsRequestFactory& creator, f9fmkt_RxKind reqKind) {
      auto retval = new OmsTwfReport2{creator, reqKind};
      retval->InitializeForReportIn();
      return retval;
   }

   static void MakeFields(fon9::seed::Fields& flds);
};

using OmsTwfReport2Factory = OmsReportFactoryT<OmsTwfReport2>;
using OmsTwfReport2FactorySP = fon9::intrusive_ptr<OmsTwfReport2Factory>;
//--------------------------------------------------------------------------//
template <class OmsTwfReport2Derived>
struct OmsTwfReport2FactoryT : public OmsTwfReport2Factory {
   fon9_NON_COPY_NON_MOVE(OmsTwfReport2FactoryT);
   using base = OmsTwfReport2Factory;
   OmsRequestSP MakeReportInImpl(f9fmkt_RxKind reqKind) override {
      OmsTwfReport2Derived* retval = new OmsTwfReport2Derived(*this, reqKind);
      retval->InitializeForReportIn();
      return retval;
   }
public:
   OmsTwfReport2FactoryT(std::string name, OmsOrderFactorySP ordFactory)
      : base(ordFactory, nullptr, fon9::Named(std::move(name)),
             MakeFieldsT<OmsTwfReport2Derived>()) {
   }
   static_assert(std::is_base_of<OmsTwfReport2, OmsTwfReport2Derived>::value, "");
};

} // namespaces
#endif//__f9omstwf_OmsTwfReport2_hpp__
