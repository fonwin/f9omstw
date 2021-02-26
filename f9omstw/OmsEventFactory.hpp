// \file f9omstw/OmsEventFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsEventFactory_hpp__
#define __f9omstw_OmsEventFactory_hpp__
#include "f9omstw/OmsBase.hpp"
#include "f9omstw/OmsFactoryPark.hpp"
#include "fon9/TimeStamp.hpp"

namespace f9omstw {

class OmsEvent : public fon9::fmkt::TradingRxItem {
   fon9_NON_COPY_NON_MOVE(OmsEvent);
   using base = fon9::fmkt::TradingRxItem;
public:
   OmsEventFactory* const  Creator_;
   fon9::TimeStamp         Time_;

   OmsEvent(OmsEventFactory&        creator,
            fon9::TimeStamp         tm,
            f9fmkt_TradingMarket    mkt = f9fmkt_TradingMarket_Unknown,
            f9fmkt_TradingSessionId sesId = f9fmkt_TradingSessionId_Unknown)
      : base(f9fmkt_RxKind_Event)
      , Creator_{&creator}
      , Time_{tm} {
      this->SetMarket(mkt);
      this->SetSessionId(sesId);
   }

   /// 使用 RevPrintFields() 輸出.
   void RevPrint(fon9::RevBuffer& rbuf) const override;

   const fon9::fmkt::TradingRxItem* CastToEvent() const override;

   /// 提供的欄位: Time, Market, SessionId;
   static fon9::seed::Fields MakeBaseEventFields();
};
using OmsEventSP = fon9::intrusive_ptr<OmsEvent>;

class OmsEventFactory : public fon9::seed::Tab {
   fon9_NON_COPY_NON_MOVE(OmsEventFactory);
   using base = fon9::seed::Tab;

public:
   using base::base;
   ~OmsEventFactory();

   virtual OmsEventSP MakeReloadEvent() = 0;
};

/// 提供一個事件範例: 單純提供「Time + Market + SessionId + 系統訊息」的事件.
class OmsEventInfo : public OmsEvent {
   fon9_NON_COPY_NON_MOVE(OmsEventInfo);
   using base = OmsEvent;
   fon9::CharVector  Info_;
public:
   OmsEventInfo(OmsEventFactory&        creator,
                fon9::TimeStamp         tm,
                const fon9::StrView&    info,
                f9fmkt_TradingMarket    mkt   = f9fmkt_TradingMarket_Unknown,
                f9fmkt_TradingSessionId sesId = f9fmkt_TradingSessionId_Unknown)
      : base{creator, tm, mkt, sesId}
      , Info_{info} {
   }

   /// 提供的欄位: Time, Market, SessionId, Info字串.
   static fon9::seed::Fields MakeFields();
};

class OmsEventInfoFactory : public OmsEventFactory {
   fon9_NON_COPY_NON_MOVE(OmsEventInfoFactory);
   using base = OmsEventFactory;

public:
   #define f9omstw_kCSTR_OmsEventInfoFactory_Name  "EvInfo"
   OmsEventInfoFactory(std::string name = f9omstw_kCSTR_OmsEventInfoFactory_Name)
      : base(fon9::Named{std::move(name)}, OmsEventInfo::MakeFields()) {
   }
   ~OmsEventInfoFactory();

   OmsEventSP MakeReloadEvent() override;

   OmsEventSP MakeEvent(fon9::TimeStamp         now,
                        const fon9::StrView&    info,
                        f9fmkt_TradingMarket    mkt = f9fmkt_TradingMarket_Unknown,
                        f9fmkt_TradingSessionId sesId = f9fmkt_TradingSessionId_Unknown) {
      return new OmsEventInfo{*this, now, info, mkt, sesId};
   }
};

} // namespaces
#endif//__f9omstw_OmsEventFactory_hpp__
