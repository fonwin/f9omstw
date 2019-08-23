// \file f9omstws/OmsTwsReportTools.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsReportTools.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9tws/ExgTypes.hpp"
#include "f9tws/ExgTradingLineFix.hpp"

namespace f9omstw {

void AssignRptBase(OmsRequestBase& rpt, const fon9::fix::FixParser& fixmsg) {
   const fon9::fix::FixParser::FixField* fixfld;
   AssignReportReqUID(rpt, fixmsg, f9fix_kTAG_ClOrdID);
   if ((fixfld = fixmsg.GetField(f9fix_kTAG_OrderID)) != nullptr)
      rpt.OrdNo_.AssignFrom(fixfld->Value_);
   if ((fixfld = fixmsg.GetField(f9fix_kTAG_TargetSubID)) != nullptr)
      rpt.BrkId_.CopyFrom(fixfld->Value_);
   rpt.SetSessionId(GetFixSessionId(fixmsg.GetField(f9fix_kTAG_SenderSubID)));
}
void AssignRptBase(OmsRequestBase& rpt, const fon9::fix::FixRecvEvArgs& rxargs) {
   AssignRptBase(rpt, rxargs.Msg_);
   const fon9::fix::FixParser::FixField* fixfld; (void)fixfld;
   rpt.SetMarket(static_cast<f9tws::ExgTradingLineFix*>(rxargs.FixSession_)->LineArgs_.Market_);
   assert((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_TargetCompID)) != nullptr
          && fixfld->Value_.Get1st() == rpt.Market());
}
void AssignTwsReportBase(OmsTwsReport& rpt, const fon9::fix::FixRecvEvArgs& rxargs, fon9::TimeStamp tday) {
   AssignRptBase(rpt, rxargs);
   rpt.ExgTime_ = GetFixTransactTime(rxargs.Msg_, tday);
   const fon9::fix::FixParser::FixField* fixfld;
   if ((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_Account)) != nullptr)
      rpt.IvacNo_ = fon9::StrTo(fixfld->Value_, 0u);
   AssignTwsReportMessage(rpt, rxargs.Msg_);
}
void AssignTwsReportMessage(OmsTwsReport& rpt, const fon9::fix::FixParser& fixmsg) {
   auto fixfld = fixmsg.GetField(f9fix_kTAG_Text);
   if (fixfld == nullptr || fixfld->Value_.empty())
      return;
   rpt.Message_.assign(fixfld->Value_);
   OmsErrCode ec = static_cast<OmsErrCode>(fon9::StrTo(fixfld->Value_, 0u));
   if (ec != OmsErrCode_NoError)
      ec = static_cast<OmsErrCode>(ec + (f9fmkt_TradingMarket_TwOTC == rpt.Market()
                                         ? OmsErrCode_FromTwOTC : OmsErrCode_FromTwSEC));
   rpt.SetErrCode(ec);
}

} // namespaces
