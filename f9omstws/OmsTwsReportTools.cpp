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
   if (ec == OmsErrCode_NoError)
      return;
   if (0);// 針對 OmsErrCode 設定如何處理? NeedsResend? LeavesQty=0?
   if (ec == 50)
      rpt.SetReportSt(f9fmkt_TradingRequestSt_ExchangeNoLeavesQty);

   ec = static_cast<OmsErrCode>(ec + (f9fmkt_TradingMarket_TwOTC == rpt.Market()
                                      ? OmsErrCode_FromTwOTC : OmsErrCode_FromTwSEC));
   rpt.SetErrCode(ec);
#if 0
   switch (ec) {
   case 4: // 0004:WAIT FOR MATCH: 需要重送(刪改查)?
      isNeedsResend = true;
      break;
   case 5: // 0005:ORDER NOT FOUND
           // 是否需要再送一次刪單? 因為可能新單還在路上(尚未送達交易所), 就已送出刪單要求,
           // 若刪單要求比新單要求早送達交易所, 就會有 "0005:ORDER NOT FOUND" 的錯誤訊息.
           // 或是重送刪單時「指定線路(送出新單的線路)」
      switch (req->OrderSt_) {
      case TOrderSt_NewSending:
      case TOrderSt_OtherNewSending:
         // 超過 n 次就不要再送了.
         isNeedsResend = (++req->ResendCount_ < 5);
         break;
      }
      break;
   }
#endif
}

} // namespaces
