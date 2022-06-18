// \file f9omstwf/OmsTwfSenderStepG1.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfSenderStepG1_hpp__
#define __f9omstwf_OmsTwfSenderStepG1_hpp__
#include "f9omstwf/OmsTwfTradingLineMgr.hpp"
#include "f9omstw/OmsTradingLineReqRunStep.hpp"

namespace f9omstw {

/// Twf 單一線路群組, 整合:
/// - 期貨日盤線路管理員、選擇權日盤線路管理員、期貨夜盤線路管理員、選擇權夜盤線路管理員.
/// - 負責處理 TDay 改變事件的後續相關作業.
using TwfTradingLineG1 = OmsTradingLineG1T<TwfTradingLineGroup>;
using TwfTradingLineG1SP = std::unique_ptr<TwfTradingLineG1>;

struct OmsTwfSenderStepG1 : public OmsTradingLineReqRunStepT<TwfTradingLineG1> {
   fon9_NON_COPY_NON_MOVE(OmsTwfSenderStepG1);
   using base = OmsTradingLineReqRunStepT<TwfTradingLineG1>;
   using base::base;
   ~OmsTwfSenderStepG1();
};

} // namespaces
#endif//__f9omstwf_OmsTwfSenderStepG1_hpp__
