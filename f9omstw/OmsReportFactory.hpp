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

   OmsRequestSP MakeReportImpl(f9fmkt_RxKind reqKind) override {
      return ReportBaseT::MakeReport(*this, reqKind);
   }

public:
   using base::base;

   OmsReportFactoryT(std::string name)
      : base(nullptr, fon9::Named(std::move(name)),
             f9omstw::MakeFieldsT<ReportBaseT>()) {
   }
};

} // namespaces
#endif//__f9omstw_OmsReportFactory_hpp__
