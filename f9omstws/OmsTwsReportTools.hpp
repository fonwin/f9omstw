// \file f9omstws/OmsTwsReportTools.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsReportTools_hpp__
#define __f9omstws_OmsTwsReportTools_hpp__
#include "f9omstws/OmsTwsReport.hpp"
#include "f9omstws/OmsTwsFilled.hpp"
#include "fon9/fix/FixParser.hpp"
#include "fon9/fix/FixConfig.hpp"
#include "fon9/fix/FixApDef.hpp"

namespace f9omstw {

void AssignRptBase(OmsRequestBase& rpt, const fon9::fix::FixParser& fixmsg);
void AssignRptBase(OmsRequestBase& rpt, const fon9::fix::FixRecvEvArgs& rxargs);

/// CancelReject/ExecutionReport 共用.
void AssignTwsReportBase(OmsTwsReport& rpt, const fon9::fix::FixRecvEvArgs& rxargs, fon9::TimeStamp tday);
/// 處理 Text 訊息內容.
void AssignTwsReportMessage(OmsTwsReport& rpt, const fon9::fix::FixParser& fixmsg);

inline OmsTwsQty GetFixFieldQty(const fon9::fix::FixParser& fixmsg, fon9::fix::FixTag tag) {
   if (const fon9::fix::FixParser::FixField* fixfld = fixmsg.GetField(tag))
      return fon9::StrTo(fixfld->Value_, OmsTwsQty{});
   return OmsTwsQty{};
}
inline bool SpaceToNul(char* ch) {
   if (*ch != ' ')
      return false;
   *ch = '\0';
   return true;
}
inline fon9::DayTime GetFixTransactTime(const fon9::fix::FixParser& fixmsg, fon9::TimeStamp tday) {
   if (auto fixfld = fixmsg.GetField(f9fix_kTAG_TransactTime))
      return fon9::StrTo(fixfld->Value_, tday) + fon9::GetLocalTimeZoneOffset() - tday;
   return fon9::DayTime::Null();
}
inline f9fmkt_Side GetFixSide(const fon9::fix::FixParser& fixmsg) {
   if (auto fixfld = fixmsg.GetField(f9fix_kTAG_Side)) {
      switch (fixfld->Value_.Get1st()) {
      case *f9fix_kVAL_Side_Buy:    return f9fmkt_Side_Buy;
      case *f9fix_kVAL_Side_Sell:   return f9fmkt_Side_Sell;
      }
   }
   return f9fmkt_Side_Unknown;
}
inline f9fmkt_TradingSessionId GetFixSessionId(const fon9::fix::FixParser::FixField* fixfld) {
   if (fixfld) {
      switch (static_cast<f9tws::TwsApCode>(fixfld->Value_.Get1st())) {
      default:
      case f9tws::TwsApCode::Regular:
         break;
      case f9tws::TwsApCode::OddLot:
         return f9fmkt_TradingSessionId_OddLot;
      case f9tws::TwsApCode::FixedPrice:
         return f9fmkt_TradingSessionId_FixedPrice;
      }
   }
   return f9fmkt_TradingSessionId_Normal;
}
inline void AssignReportSymbol(OmsTwsSymbol& symb, const fon9::fix::FixParser& fixmsg) {
   if (auto fixfld = fixmsg.GetField(f9fix_kTAG_Symbol)) {
      symb.AssignFrom(fixfld->Value_);
      if (SpaceToNul(symb.end() - 1))
         SpaceToNul(symb.end() - 2);
   }
}
inline void AssignReportReqUID(OmsRequestId& rid, const fon9::fix::FixParser::FixField& fixfld) {
   const char* pbeg = fixfld.Value_.begin();
   if (const char* p = fon9::StrRFindIf(pbeg, fixfld.Value_.end(),
                                          [](unsigned char ch) { return ch != ' '; }))
      rid.ReqUID_.CopyFrom(pbeg, static_cast<size_t>(p - pbeg + 1));
}
inline void AssignReportReqUID(OmsRequestId& rid, const fon9::fix::FixParser& fixmsg, fon9::fix::FixTag tag) {
   if (auto fixfld = fixmsg.GetField(tag))
      AssignReportReqUID(rid, *fixfld);
}

} // namespaces
#endif//__f9omstws_OmsTwsReportTools_hpp__
