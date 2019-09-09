// \file f9omstws/OmsTwsTradingLineTmp2019.hpp
//
// 20200323: 因應逐筆撮合有新格式.
// 此處提供的是逐筆撮合之前的格式.
//
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsTradingLineTmp2019_hpp__
#define __f9omstws_OmsTwsTradingLineTmp2019_hpp__
#include "f9tws/ExgLineTmpFactory.hpp"
#include "f9tws/ExgLineTmp.hpp"
#include "f9omstws/OmsTwsTradingLineBase.hpp"

namespace f9omstw {
namespace f9fmkt = fon9::fmkt;

class TwsTradingLineTmp2019 : public f9tws::ExgTmpIoSession
                            , public f9fmkt::TradingLine
                            , public TwsTradingLineBase {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineTmp2019);
   using base = f9tws::ExgTmpIoSession;
   OmsRxSNO    SendingSNO_{};
   OmsCoreSP   OmsCore_;
   void ClearSending();

protected:
   void OnExgTmp_ApReady() override;
   void OnExgTmp_ApBroken() override;
   void OnExgTmp_ApPacket(const f9tws::ExgTmpHead& pktmp, unsigned pksz) override;

public:
   OmsTwsReportFactory& RptFactory_;

   TwsTradingLineTmp2019(OmsTwsReportFactory&         rptFactory,
                         f9tws::ExgTradingLineMgr&    lineMgr,
                         const f9tws::ExgLineTmpArgs& lineargs,
                         std::string                  logFileName);

   SendResult SendRequest(f9fmkt::TradingRequest& req) override;
};

//--------------------------------------------------------------------------//

class TwsTradingLineTmpFactory2019 : public f9tws::ExgLineTmpFactory
                                   , public TwsTradingLineFactoryBase {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineTmpFactory2019);
   using base = f9tws::ExgLineTmpFactory;

protected:
   fon9::TimeStamp GetTDay() override;
   fon9::io::SessionSP CreateLineTmp(f9tws::ExgTradingLineMgr& lineMgr,
                                     const f9tws::ExgLineTmpArgs& args,
                                     std::string& errReason,
                                     std::string logFileName) override;
public:
   TwsTradingLineTmpFactory2019(OmsCoreMgr&          coreMgr,
                                OmsTwsReportFactory& rptFactory,
                                OmsTwsFilledFactory& filFactory,
                                std::string          logPathFmt,
                                Named&&              name);
};

} // namespaces
#endif//__f9omstws_OmsTwsTradingLineTmp2019_hpp__
