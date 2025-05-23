﻿// \file f9omstws/OmsTwsReport.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsReport_hpp__
#define __f9omstws_OmsTwsReport_hpp__
#include "f9omstw/OmsReportFactory.hpp"
#include "f9omstws/OmsTwsRequest.hpp"
#include "f9omstw/OmsReportTools.hpp"

namespace f9omstw {

/// Tws 的「新刪改查」回報, 透過此處處理.
class OmsTwsReport : public OmsTwsRequestIni {
   fon9_NON_COPY_NON_MOVE(OmsTwsReport);
   using base = OmsTwsRequestIni;

protected:
   void RunReportInCore_Order(OmsReportChecker&& checker, OmsOrder& order) override;
   void RunReportInCore_NewOrder(OmsReportRunnerInCore&& runner) override;
   void RunReportInCore_FromOrig(OmsReportChecker&& checker, const OmsRequestBase& origReq) override;
   void RunReportInCore_MakeReqUID() override;
   void RunReportInCore_InitiatorNew(OmsReportRunnerInCore&& inCoreRunner) override;
   void RunReportInCore_DCQ(OmsReportRunnerInCore&& inCoreRunner) override;
   bool RunReportInCore_IsBfAfMatch(const OmsOrderRaw& ordu) const override;
   bool RunReportInCore_IsExgTimeMatch(const OmsOrderRaw& ordu) const override;
   /// 從 ini 複製基本欄位: Side_, Symbol_, IvacNo_, SubacNo_, IvacNoFlag_, OType_(如果this->OType_==OmsTwsOType{})
   void AssignFromIniBase(const OmsTwsRequestIni& ini);

public:
   /// 如果有跨日交易(夜盤), 則跨日的回報 ExgTime_ 應加上 TimeInterval_Day(1);
   fon9::DayTime     ExgTime_{fon9::DayTime::Null()};
   fon9::CharVector  Message_;
   OmsTwsQty         BeforeQty_{};
   /// 若為 this->LeavesQty_ == 0; 則使用 this->Qty_; (AfterQty);
   OmsTwsQty         LeavesQty_{};
   /// 通常用在建立回報時設定,
   /// 在更新 ordraw 時: 會先將 this->Qty_, this->BeforeQty_ 調整成股數,
   /// 並將此欄改為 OmsReportQtyStyle::OddLot;
   /// 讓後續的處理可以不用考慮此欄.
   OmsReportQtyStyle QtyStyle_{OmsReportQtyStyle::BySessionId};
   fon9::CharAryF<2> OutPvcId_{nullptr};
   char              padding__[5];

   using base::base;
   OmsTwsReport() = default;

   void ProcessPendingReport(const OmsRequestRunnerInCore& prevRunner) const override;
   void OnSynReport(const OmsRequestBase* ref, fon9::StrView message) override;
   bool ParseAskerMessage(fon9::StrView message) override;

   static OmsRequestSP MakeReportIn(OmsRequestFactory& creator, f9fmkt_RxKind reqKind) {
      auto retval = new OmsTwsReport{creator, reqKind};
      retval->InitializeForReportIn();
      return retval;
   }

   static void MakeFields(fon9::seed::Fields& flds);
};

using OmsTwsReportFactory = OmsReportFactoryT<OmsTwsReport>;
using OmsTwsReportFactorySP = fon9::intrusive_ptr<OmsTwsReportFactory>;
//--------------------------------------------------------------------------//
template <class OmsTwsReportDerived>
struct OmsTwsReportFactoryT : public OmsTwsReportFactory {
   fon9_NON_COPY_NON_MOVE(OmsTwsReportFactoryT);
   using base = OmsTwsReportFactory;
   OmsRequestSP MakeReportInImpl(f9fmkt_RxKind reqKind) override {
      OmsTwsReportDerived* retval = new OmsTwsReportDerived(*this, reqKind);
      retval->InitializeForReportIn();
      return retval;
   }
public:
   OmsTwsReportFactoryT(std::string name, OmsOrderFactorySP ordFactory)
      : base(ordFactory, nullptr, fon9::Named(std::move(name)),
             MakeFieldsT<OmsTwsReportDerived>()) {
   }
   static_assert(std::is_base_of<OmsTwsReport, OmsTwsReportDerived>::value, "");
};

} // namespaces
#endif//__f9omstws_OmsTwsReport_hpp__
