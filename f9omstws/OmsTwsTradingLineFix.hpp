// \file f9omstws/OmsTwsTradingLineFix.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsTradingLineFix_hpp__
#define __f9omstws_OmsTwsTradingLineFix_hpp__
#include "f9tws/ExgTradingLineFixFactory.hpp"
#include "f9omstws/OmsTwsFilled.hpp"
#include "f9omstws/OmsTwsReport.hpp"

namespace f9omstw {
namespace f9fix = fon9::fix;
namespace f9fmkt = fon9::fmkt;

class TwsTradingLineFix : public f9tws::ExgTradingLineFix {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineFix);
   using base = f9tws::ExgTradingLineFix;

public:
   const fon9::CharVector  StrSendingBy_;

   TwsTradingLineFix(f9fix::IoFixManager&                mgr,
                     const f9fix::FixConfig&             fixcfg,
                     const f9tws::ExgTradingLineFixArgs& lineargs,
                     f9fix::IoFixSenderSP&&              fixSender);

   SendResult SendRequest(f9fmkt::TradingRequest& req) override;
};

//--------------------------------------------------------------------------//

class TwsTradingLineFixFactory : public f9tws::ExgTradingLineFixFactory {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineFixFactory);
   using base = f9tws::ExgTradingLineFixFactory;

   void OnFixReject(const f9fix::FixRecvEvArgs& rxargs, const f9fix::FixOrigArgs& orig);
   void OnFixExecFilled(const f9fix::FixRecvEvArgs& rxargs);
   void OnFixExecReport(const f9fix::FixRecvEvArgs& rxargs);
   void OnFixCancelReject(const f9fix::FixRecvEvArgs& rxargs);

protected:
   OmsCoreMgr&          CoreMgr_;
   OmsTwsReportFactory& RptFac_;
   OmsTwsFilledFactory& FilFac_;

   fon9::TimeStamp GetTDay() override;
   fon9::io::SessionSP CreateTradingLine(f9tws::ExgTradingLineMgr&           lineMgr,
                                         const f9tws::ExgTradingLineFixArgs& args,
                                         f9fix::IoFixSenderSP                fixSender) override;
public:
   TwsTradingLineFixFactory(OmsCoreMgr&          coreMgr,
                            OmsTwsReportFactory& rptFactory,
                            OmsTwsFilledFactory& filFactory,
                            std::string          fixLogPathFmt,
                            Named&& name);
};

} // namespaces
#endif//__f9omstws_OmsTwsTradingLineFix_hpp__
