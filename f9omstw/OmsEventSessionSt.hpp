// \file f9omstw/OmsEventSessionSt.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsEventSessionSt_hpp__
#define __f9omstw_OmsEventSessionSt_hpp__
#include "f9omstw/OmsEventFactory.hpp"

namespace f9omstw {

/// 提供 f9fmkt_TradingSessionSt 異動事件.
/// 包含的欄位: Time + Market + SessionId + SessionSt;
class OmsEventSessionSt : public OmsEvent {
   fon9_NON_COPY_NON_MOVE(OmsEventSessionSt);
   using base = OmsEvent;
   f9fmkt_TradingSessionSt SessionSt_;
   char                    Padding___[7];
public:
   OmsEventSessionSt(OmsEventFactory&        creator,
                     fon9::TimeStamp         tm,
                     f9fmkt_TradingMarket    mkt,
                     f9fmkt_TradingSessionId sesId,
                     f9fmkt_TradingSessionSt sesSt)
      : base{creator, tm, mkt, sesId}
      , SessionSt_{sesSt} {
   }

   f9fmkt_TradingSessionSt SessionSt() const {
      return this->SessionSt_;
   }

   static fon9::seed::Fields MakeFields();
};

class OmsEventSessionStFactory : public OmsEventFactory {
   fon9_NON_COPY_NON_MOVE(OmsEventSessionStFactory);
   using base = OmsEventFactory;

public:
   #define f9omstw_kCSTR_OmsEventSessionStFactory_Name   "EvSesSt"
   OmsEventSessionStFactory(std::string name = f9omstw_kCSTR_OmsEventSessionStFactory_Name)
      : base(fon9::Named{std::move(name)}, OmsEventSessionSt::MakeFields()) {
   }
   ~OmsEventSessionStFactory();

   OmsEventSP MakeReloadEvent() override;

   OmsEventSP MakeEvent(fon9::TimeStamp         now,
                        f9fmkt_TradingMarket    mkt,
                        f9fmkt_TradingSessionId sesId,
                        f9fmkt_TradingSessionSt sesSt) {
      return new OmsEventSessionSt{*this, now, mkt, sesId, sesSt};
   }
};

} // namespaces
#endif//__f9omstw_OmsEventSessionSt_hpp__
