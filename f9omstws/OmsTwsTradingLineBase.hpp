// \file f9omstws/OmsTwsTradingLineBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsTradingLineBase_hpp__
#define __f9omstws_OmsTwsTradingLineBase_hpp__
#include "f9omstws/OmsTwsFilled.hpp"
#include "f9omstws/OmsTwsReport.hpp"
#include "f9tws/ExgLineArgs.hpp"

namespace f9omstw {

class TwsTradingLineBase {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineBase);
public:
   const fon9::CharVector  StrSendingBy_;

   TwsTradingLineBase(const f9tws::ExgLineArgs& lineargs);
};

//--------------------------------------------------------------------------//

class TwsTradingLineFactoryBase {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineFactoryBase);
public:
   OmsCoreMgr&          CoreMgr_;
   OmsTwsReportFactory& RptFactory_;
   OmsTwsFilledFactory& FilFactory_;

   TwsTradingLineFactoryBase(OmsCoreMgr&          coreMgr,
                             OmsTwsReportFactory& rptFactory,
                             OmsTwsFilledFactory& filFactory)
      : CoreMgr_(coreMgr)
      , RptFactory_(rptFactory)
      , FilFactory_(filFactory) {
   }
};

} // namespaces
#endif//__f9omstws_OmsTwsTradingLineBase_hpp__
