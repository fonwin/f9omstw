// \file f9omstws/OmsTwsRequestTools.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsRequestTools_hpp__
#define __f9omstws_OmsTwsRequestTools_hpp__
#include "f9omstws/OmsTwsRequest.hpp"
#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstw/OmsRequestRunner.hpp"

namespace f9omstw {

inline OmsTwsRequestChg::QtyType GetTwsRequestChgQty(OmsRequestRunnerInCore& runner,
                                                     const OmsRequestTrade*  curReq,
                                                     const OmsTwsOrderRaw*   lastOrd,
                                                     bool isWantToKill)
{
   if (auto* chgQtyReq = dynamic_cast<const OmsTwsRequestChg*>(curReq))
      return runner.GetRequestChgQty(chgQtyReq->Qty_, isWantToKill, lastOrd->LeavesQty_, lastOrd->CumQty_);
   // 使用 OmsTwsRequestIni 改量, 則直接把 chgQtyIniReq->Qty_ 送交易所.
   if (auto* chgQtyIniReq = dynamic_cast<const OmsTwsRequestIni*>(curReq)) {
      if (fon9::signed_cast(chgQtyIniReq->Qty_) >= 0)
         return fon9::signed_cast(chgQtyIniReq->Qty_);
      runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_ChgQty, nullptr);
      return -1;
   }
   runner.Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_UnknownRequestType, nullptr);
   return -1;
}

} // namespaces
#endif//__f9omstws_OmsTwsRequestTools_hpp__
