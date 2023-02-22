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
   auto        fixfld = fixmsg.GetField(f9fix_kTAG_Text);
   OmsErrCode  ec;
   if (fixfld == nullptr || fixfld->Value_.empty())
      goto __ASSIGN_EC_OK;
   else {
      rpt.Message_.assign(fixfld->Value_);
      const unsigned char  ch0 = static_cast<unsigned char>(fixfld->Value_.Get1st());
      if (fon9_LIKELY(fon9::isdigit(ch0)))
         ec = static_cast<OmsErrCode>(fon9::StrTo(fixfld->Value_, 0u));
      else {
         switch (ch0) {
         case 'C': // 盤中零股錯誤碼=Cxxx: ec = xxx + 500
            ec = static_cast<OmsErrCode>(500 + fon9::StrTo(fon9::StrView{fixfld->Value_.begin() + 1, fixfld->Value_.end()}, 0u));
            break;
         default:
            ec = OmsErrCode_Null;
            break;
         }
      }
   }
   if (ec == OmsErrCode_Null) {
   __ASSIGN_EC_OK:
      if (rpt.ReportSt() == f9fmkt_TradingRequestSt_ExchangeCanceling && rpt.RxKind() == f9fmkt_RxKind_RequestNew)
         return;
      ec = rpt.GetOkErrCode();
   }
   else {
      ec = static_cast<OmsErrCode>(ec + (f9fmkt_TradingMarket_TwOTC == rpt.Market()
                                         ? OmsErrCode_FromTwOTC : OmsErrCode_FromTwSEC));
   }
   rpt.SetErrCode(ec);
}

} // namespaces
