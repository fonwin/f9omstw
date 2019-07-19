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
   OmsEvent(OmsEventFactory& creator)
      : base(f9fmkt_RxKind_Event)
      , Creator_{&creator} {
   }
};
using OmsEventSP = fon9::intrusive_ptr<OmsEvent>;

class OmsEventFactory : public fon9::seed::Tab {
   fon9_NON_COPY_NON_MOVE(OmsEventFactory);
   using base = fon9::seed::Tab;

public:
   using base::base;
   ~OmsEventFactory();

   virtual OmsEventSP MakeEvent(fon9::TimeStamp now = fon9::UtcNow());
};

} // namespaces
#endif//__f9omstw_OmsEventFactory_hpp__
