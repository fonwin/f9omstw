// \file f9omstws/OmsTwsTradingLineBase.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsTradingLineBase.hpp"
#include "f9omstw/OmsCoreMgr.hpp"

namespace f9omstw {

TwsTradingLineBase::TwsTradingLineBase(const f9tws::ExgLineArgs& lineargs)
   : StrSendingBy_{fon9::RevPrintTo<fon9::CharVector>(
                        "Sending.",
                        lineargs.Market_,
                        lineargs.BrkId_,
                        lineargs.SocketId_)} {
}
//--------------------------------------------------------------------------//
fon9::TimeStamp TwsTradingLineFactoryBase::GetCoreTDay() const {
   if (auto core = this->CoreMgr_.CurrentCore())
      return core->TDay();
   return fon9::TimeStamp{};
}

} // namespaces
