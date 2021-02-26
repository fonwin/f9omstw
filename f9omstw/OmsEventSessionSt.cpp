// \file f9omstw/OmsEventSessionSt.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsEventSessionSt.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

OmsEventSessionStFactory::~OmsEventSessionStFactory() {
}
OmsEventSP OmsEventSessionStFactory::MakeReloadEvent() {
   return new OmsEventSessionSt{*this,
      fon9::TimeStamp{},
      f9fmkt_TradingMarket_Unknown,
      f9fmkt_TradingSessionId_Unknown,
      f9fmkt_TradingSessionSt_Unknown};
}
//--------------------------------------------------------------------------//
fon9::seed::Fields OmsEventSessionSt::MakeFields() {
   fon9::seed::Fields flds = MakeBaseEventFields();
   flds.Add(fon9::seed::FieldSP{new fon9::seed::FieldIntHx<fon9::underlying_type_t<f9fmkt_TradingSessionSt>>(
      fon9::Named("SessionSt"), fon9_OffsetOfRawPointer(OmsEventSessionSt, SessionSt_))});
   return flds;
}

} // namespaces
