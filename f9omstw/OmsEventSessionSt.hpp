// \file f9omstw/OmsEventSessionSt.hpp
//
// OmsEventSessionSt 流程:
// - 即時 OmsCore.PublishSessionSt():
//   - OmsCore.EventToCore() => EventToCoreImpl()
//   - OmsCore.EventInCore();
//     - OmsCoreMgr.OnEventInCore();
//       - OmsCoreMgr.OnEventSessionSt()
//         - OmsCore.OnEventSessionSt(): 設定 OmsCore.MapSessionSt_;
//         - OmsCoreMgr.OmsSessionStEvent_.Publish();
//     - Backend.Append();
//     - OmsCoreMgr.OmsEvent_.Publish();
// - 重載:
//   - OmsBackend::OpenReload(): Reload from log;
//     - OmsCoreMgr.ReloadEvent();
//       - OmsCoreMgr.OnEventSessionSt();
//         - OmsCore.OnEventSessionSt(): 設定 OmsCore.MapSessionSt_;
//         - OmsCoreMgr.OmsSessionStEvent_.Publish();
//       - OmsCoreMgr.OmsEvent_.Publish();
//
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsEventSessionSt_hpp__
#define __f9omstw_OmsEventSessionSt_hpp__
#include "f9omstw/OmsEventFactory.hpp"

namespace f9omstw {

/// 提供 f9fmkt_TradingSessionSt 異動事件.
class OmsEventSessionSt : public OmsEvent {
   fon9_NON_COPY_NON_MOVE(OmsEventSessionSt);
   using base = OmsEvent;
   using FlowGroupT = uint8_t;
   const f9fmkt_TradingSessionSt SessionSt_;
   const FlowGroupT              FlowGroup_;
   char                          Padding___[6];
public:
   OmsEventSessionSt(OmsEventFactory&        creator,
                     fon9::TimeStamp         tm,
                     f9fmkt_TradingSessionSt sesSt,
                     f9fmkt_TradingMarket    mkt,
                     f9fmkt_TradingSessionId sesId,
                     FlowGroupT              flowGroup)
      : base{creator, tm, mkt, sesId}
      , SessionSt_{sesSt}
      , FlowGroup_{flowGroup} {
   }
   ~OmsEventSessionSt();

   f9fmkt_TradingSessionSt SessionSt() const {
      return this->SessionSt_;
   }
   FlowGroupT FlowGroup() const {
      return this->FlowGroup_;
   }

   /// 包含的欄位: Time, Market, SessionId, FlowGroup, SessionSt;
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
                        f9fmkt_TradingSessionSt sesSt,
                        f9fmkt_TradingMarket    mkt,
                        f9fmkt_TradingSessionId sesId,
                        uint8_t                 flowGroup) {
      return new OmsEventSessionSt{*this, now, sesSt, mkt, sesId, flowGroup};
   }
};

} // namespaces
#endif//__f9omstw_OmsEventSessionSt_hpp__
