// \file f9omstwf/OmsTwfTradingLineTmp.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfTradingLineTmp_hpp__
#define __f9omstwf_OmsTwfTradingLineTmp_hpp__
#include "f9omstwf/OmsTwfRptLineTmp.hpp"
#include "f9omstw/OmsBase.hpp"
#include "f9twf/ExgLineTmpFactory.hpp"
#include "fon9/FlowCounter.hpp"

namespace f9omstw {
namespace f9fmkt = fon9::fmkt;

class TwfTradingLineTmp : public TwfRptLineTmp, public f9fmkt::TradingLine {
   fon9_NON_COPY_NON_MOVE(TwfTradingLineTmp);
   using base = TwfRptLineTmp;
   using baseTradingLine = f9fmkt::TradingLine;

   fon9::FlowCounter Fc_;

protected:
   void GetApReadyInfo(fon9::RevBufferList& rbuf) override;
   void OnExgTmp_ApReady() override;
   void OnExgTmp_ApBroken() override;

public:
   const fon9::CharVector  StrSendingBy_;

   TwfTradingLineTmp(TwfLineTmpWorker&            worker,
                     f9twf::ExgTradingLineMgr&    lineMgr,
                     const f9twf::ExgLineTmpArgs& lineArgs,
                     f9twf::ExgLineTmpLog&&       log);

   SendResult SendRequest(f9fmkt::TradingRequest& req) override;
   bool IsOrigSender(const f9fmkt::TradingRequest& req) const override;
};

} // namespaces
#endif//__f9omstwf_OmsTwfTradingLineTmp_hpp__
