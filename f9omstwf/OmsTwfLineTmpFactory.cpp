// \file f9omstwf/OmsTwfLineTmpFactory.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfLineTmpFactory.hpp"
#include "f9omstwf/OmsTwfTradingLineTmp.hpp"
#include "f9omstw/OmsCoreMgr.hpp"

namespace f9omstw {

fon9::TimeStamp TwfLineTmpFactory::GetTDay() {
   return this->CoreMgr_.CurrentCoreTDay();
}
fon9::io::SessionSP TwfLineTmpFactory::CreateLineTmp(f9twf::ExgTradingLineMgr&    lineMgr,
                                                     const f9twf::ExgLineTmpArgs& lineArgs,
                                                     f9twf::ExgLineTmpLog&&       log,
                                                     std::string&                 errReason) {
   switch (lineArgs.ApCode_) {
   case f9twf::TmpApCode::Trading:
   case f9twf::TmpApCode::TradingShortFormat:
      return new TwfTradingLineTmp(*this, lineMgr, lineArgs, std::move(log));
   case f9twf::TmpApCode::ReportCm:
   case f9twf::TmpApCode::TmpDc:
      return new TwfRptLineTmp(*this, lineMgr, lineArgs, std::move(log));
   default:
      errReason = "Unknown ApCode.";
      return nullptr;
   }
}

} // namespaces
