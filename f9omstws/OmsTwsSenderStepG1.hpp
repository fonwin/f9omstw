// \file f9omstws/OmsTwsSenderStepG1.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsSenderStepG1_hpp__
#define __f9omstws_OmsTwsSenderStepG1_hpp__
#include "f9omstws/OmsTwsTradingLineMgr.hpp"
#include "f9omstw/OmsTradingLineReqRunStep.hpp"

namespace f9omstw {

/// Tws 單一線路群組, 整合: 一個「上市線路管理員」及一個「上櫃線路管理員」.
/// - 負責處理 TDay 改變事件的後續相關作業.
class TwsTradingLineG1 : public OmsTradingLineG1T<TwsTradingLineGroup> {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineG1);
   using base = OmsTradingLineG1T<TwsTradingLineGroup>;
   void OnOmsSessionStEvent(OmsResource& res, const OmsEventSessionSt&);
public:
   TwsTradingLineG1(OmsCoreMgr& coreMgr);
   ~TwsTradingLineG1();
};
using TwsTradingLineG1SP = std::unique_ptr<TwsTradingLineG1>;

using OmsTwsSenderStepG1 = OmsTradingLineReqRunStepT<TwsTradingLineG1>;

} // namespaces
#endif//__f9omstws_OmsTwsSenderStepG1_hpp__
