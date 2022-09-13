// \file f9omstw/OmsReportFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsReportFactory_hpp__
#define __f9omstw_OmsReportFactory_hpp__
#include "f9omstw/OmsRequestFactory.hpp"

namespace f9omstw {

template <class ReportBaseT, class OmsFactoryBase = OmsRequestFactory>
class OmsReportFactoryT : public OmsFactoryBase {
   fon9_NON_COPY_NON_MOVE(OmsReportFactoryT);
   using base = OmsFactoryBase;

   OmsRequestSP MakeReportInImpl(f9fmkt_RxKind reqKind) override {
      return ReportBaseT::MakeReportIn(*this, reqKind);
   }

public:
   using base::base;

   OmsReportFactoryT(std::string name, OmsOrderFactorySP ordFactory, OmsRequestRunStepSP&& rptRerunStepList = nullptr)
      : base(ordFactory, nullptr, fon9::Named(std::move(name)), f9omstw::MakeFieldsT<ReportBaseT>()) {
      this->SetRptRerunStep(std::move(rptRerunStepList));
   }
};

} // namespaces
#endif//__f9omstw_OmsReportFactory_hpp__
