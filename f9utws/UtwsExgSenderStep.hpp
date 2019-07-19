// \file f9utws/UtwsExgSenderStep.hpp
// \author fonwinz@gmail.com
#ifndef __f9utws_UtwsExgSenderStep_hpp__
#define __f9utws_UtwsExgSenderStep_hpp__
#include "f9omstw/OmsRequestRunner.hpp"
#include "f9omstws/OmsTwsTradingLineMgr.hpp"

namespace f9omstw {

class UtwsExgTradingLineMgr {
   fon9_NON_COPY_NON_MOVE(UtwsExgTradingLineMgr);
   OmsCoreMgr&    CoreMgr_;
   fon9::SubConn  SubrTDayChanged_{};
   void OnTDayChanged(OmsCore& core);
public:
   using TradingLineMgrSP = fon9::intrusive_ptr<TwsTradingLineMgr>;
   TradingLineMgrSP  TseTradingLineMgr_;
   TradingLineMgrSP  OtcTradingLineMgr_;

   UtwsExgTradingLineMgr(OmsCoreMgr& coreMgr);
   ~UtwsExgTradingLineMgr();
};

class UtwsExgSenderStep : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(UtwsExgSenderStep);
   using base = OmsRequestRunStep;
   using base::base;

public:
   UtwsExgTradingLineMgr&  LineMgr_;
   UtwsExgSenderStep(UtwsExgTradingLineMgr&);
   void RunRequest(OmsRequestRunnerInCore&& runner) override;
};

} // namespaces
#endif//__f9utws_UtwsExgSenderStep_hpp__
