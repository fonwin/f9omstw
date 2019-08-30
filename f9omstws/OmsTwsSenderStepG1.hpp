// \file f9omstw/OmsTwsSenderStepG1.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsTwsSenderStepG1_hpp__
#define __f9omstw_OmsTwsSenderStepG1_hpp__
#include "f9omstw/OmsRequestRunner.hpp"
#include "f9omstws/OmsTwsTradingLineMgr.hpp"

namespace f9omstw {

/// Tws 單一線路群組, 整合: 一個「上市線路管理員」及一個「上櫃線路管理員」.
/// - 負責處理 TDay 改變事件的後續相關作業.
class TwsTradingLineMgrG1 {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineMgrG1);
   OmsCoreMgr&    CoreMgr_;
   fon9::SubConn  SubrTDayChanged_{};
   void OnTDayChanged(OmsCore& core);
public:
   TwsTradingLineMgrSP  TseTradingLineMgr_;
   TwsTradingLineMgrSP  OtcTradingLineMgr_;

   TwsTradingLineMgrG1(OmsCoreMgr& coreMgr);
   ~TwsTradingLineMgrG1();
};

class OmsTwsSenderStepG1 : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(OmsTwsSenderStepG1);
   using base = OmsRequestRunStep;
   using base::base;

public:
   TwsTradingLineMgrG1&  LineMgr_;
   OmsTwsSenderStepG1(TwsTradingLineMgrG1&);
   void RunRequest(OmsRequestRunnerInCore&& runner) override;
   void RerunRequest(OmsReportRunnerInCore&& runner) override;
};

} // namespaces
#endif//__f9omstw_OmsTwsSenderStepG1_hpp__
