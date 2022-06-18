// \file f9omstws/OmsTwsSenderStepG1.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsSenderStepG1.hpp"
#include "f9omstw/OmsEventSessionSt.hpp"

namespace f9omstw {

TwsTradingLineG1::TwsTradingLineG1(OmsCoreMgr& coreMgr) : base{coreMgr} {
   coreMgr.OmsSessionStEvent_.Subscribe(// &this->SubrOmsEvent_,
           std::bind(&TwsTradingLineG1::OnOmsSessionStEvent, this, std::placeholders::_1, std::placeholders::_2));
}
TwsTradingLineG1::~TwsTradingLineG1() {
}
void TwsTradingLineG1::OnOmsSessionStEvent(OmsResource& res, const OmsEventSessionSt& evSesSt) {
   this->SetTradingSessionSt(res.TDay(), evSesSt.Market(), evSesSt.SessionId(), evSesSt.SessionSt());
}

} // namespaces
