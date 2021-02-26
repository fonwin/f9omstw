// \file f9omstw/OmsEventFactory.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsEventFactory.hpp"
#include "f9omstw/OmsTools.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

fon9::seed::Fields OmsEvent::MakeBaseEventFields() {
   fon9::seed::Fields flds;
   flds.Add(fon9_MakeField2(OmsEventInfo, Time));
   flds.Add(fon9_MakeField2(OmsEventInfo, Market));
   flds.Add(fon9_MakeField2(OmsEventInfo, SessionId));
   return flds;
}
void OmsEvent::RevPrint(fon9::RevBuffer& rbuf) const {
   RevPrintFields(rbuf, *this->Creator_, fon9::seed::SimpleRawRd{*this});
}
const fon9::fmkt::TradingRxItem* OmsEvent::CastToEvent() const {
   return this;
}
//--------------------------------------------------------------------------//
OmsEventFactory::~OmsEventFactory() {
}
//--------------------------------------------------------------------------//
OmsEventInfoFactory::~OmsEventInfoFactory() {
}
OmsEventSP OmsEventInfoFactory::MakeReloadEvent() {
   return new OmsEventInfo{*this, fon9::TimeStamp{}, nullptr};
}
//--------------------------------------------------------------------------//
fon9::seed::Fields OmsEventInfo::MakeFields() {
   fon9::seed::Fields flds = MakeBaseEventFields();
   flds.Add(fon9_MakeField2(OmsEventInfo, Info));
   return flds;
}

} // namespaces
