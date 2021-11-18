// \file f9omstwf/OmsTwfReport9.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfReport9_hpp__
#define __f9omstwf_OmsTwfReport9_hpp__
#include "f9omstw/OmsReportFactory.hpp"
#include "f9omstw/OmsReportTools.hpp"
#include "f9omstwf/OmsTwfReport2.hpp"
#include "f9omstwf/OmsTwfRequest9.hpp"

namespace f9omstw {

/// 報價單回報.
/// - 新單: 若是補單, 必須同時提供 Bid & Offer;
/// - 新單: 若僅提供一邊, 且無原始報價要求, 則拋棄該次回報.
/// - 刪改: 可能單邊成功(R02/R32/R22), 另一邊失敗(R03);
class OmsTwfReport9 : public OmsTwfRequestIni9 {
   fon9_NON_COPY_NON_MOVE(OmsTwfReport9);
   using base = OmsTwfRequestIni9;
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
   fon9::CharVector  Message_;

   /// this->BidQty_; this->OfferQty_; = AfterQty;
   OmsTwfQty         BidBeforeQty_{};
   OmsTwfQty         OfferBeforeQty_{};

   /// 通常僅用在新單補單.
   /// 報價單沒有改價功能.
   OmsReportPriStyle PriStyle_{OmsReportPriStyle::NoDecimal};

   /// - f9fmkt_Side_Unknown: 同時提供 Bid & Offer;
   /// - f9fmkt_Side_Buy:     僅提供 Bid;
   /// - f9fmkt_Side_Sell:    僅提供 Offer;
   f9fmkt_Side       Side_;

   f9twf::TmpSessionId_t   OutPvcId_{};

   using base::base;

   void ProcessPendingReport(OmsResource& res) const override;
   void OnSynReport(const OmsRequestBase* ref, fon9::StrView message) override;

   static OmsRequestSP MakeReportIn(OmsRequestFactory& creator, f9fmkt_RxKind reqKind) {
      auto retval = new OmsTwfReport9{creator, reqKind};
      retval->InitializeForReportIn();
      return retval;
   }

   static void MakeFields(fon9::seed::Fields& flds);
};

using OmsTwfReport9Factory = OmsReportFactoryT<OmsTwfReport9>;
using OmsTwfReport9FactorySP = fon9::intrusive_ptr<OmsTwfReport9Factory>;

} // namespaces
#endif//__f9omstwf_OmsTwfReport9_hpp__
