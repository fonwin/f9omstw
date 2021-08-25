// \file f9omstwf/OmsTwfSenderStepG1.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfSenderStepG1_hpp__
#define __f9omstwf_OmsTwfSenderStepG1_hpp__
#include "f9omstw/OmsRequestRunner.hpp"
#include "f9omstwf/OmsTwfTradingLineMgr.hpp"

namespace f9omstw {

/// Twf 單一線路群組, 整合:
/// - 期貨日盤線路管理員、選擇權日盤線路管理員、期貨夜盤線路管理員、選擇權夜盤線路管理員.
/// - 負責處理 TDay 改變事件的後續相關作業.
class TwfTradingLineMgrG1 {
   fon9_NON_COPY_NON_MOVE(TwfTradingLineMgrG1);
   OmsCoreMgr&    CoreMgr_;
   fon9::SubConn  SubrTDayChanged_{};
   void OnTDayChanged(OmsCore& core);
public:
   TwfTradingLineMgrSP  TradingLineMgr_[f9twf::ExgSystemTypeCount()];

   TwfTradingLineMgrG1(OmsCoreMgr& coreMgr);
   ~TwfTradingLineMgrG1();

   /// 若返回 nullptr, 則會先執行 runner.Reject();
   TwfTradingLineMgr* GetLineMgr(OmsRequestRunnerInCore& runner) const;
};

class OmsTwfSenderStepG1 : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(OmsTwfSenderStepG1);
   using base = OmsRequestRunStep;
   using base::base;

public:
   TwfTradingLineMgrG1&  LineMgr_;
   OmsTwfSenderStepG1(TwfTradingLineMgrG1&);
   void RunRequest(OmsRequestRunnerInCore&& runner) override;
   void RerunRequest(OmsReportRunnerInCore&& runner) override;
};

} // namespaces
#endif//__f9omstwf_OmsTwfSenderStepG1_hpp__
