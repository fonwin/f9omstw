// \file f9omstws/OmsTwsScBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsScBase_hpp__
#define __f9omstws_OmsTwsScBase_hpp__
#include "f9omstws/OmsTwsRequest.hpp"

namespace f9omstw {

struct OmsTwsScBase {
   const OmsTwsRequestIni* IniReq_;
   OmsBSIdx                BSIdx_;
   OmsTwsTradingSessionIdx TradingSessionIdx_;

   /// \retval OmsErrCode_NoError
   /// \retval OmsErrCode_OrderInitiatorNotFound
   /// \retval OmsErrCode_Bad_Side
   /// \retval OmsErrCode_Bad_SessionId
   OmsErrCode SetScInitiator(const OmsTwsRequestIni* inireq) {
      if ((this->IniReq_ = inireq) == nullptr)
         return OmsErrCode_OrderInitiatorNotFound;
      fon9_WARN_DISABLE_SWITCH;
      switch (this->IniReq_->Side_) {
      case f9fmkt_Side_Buy:   this->BSIdx_ = OmsBSIdx_Buy;  break;
      case f9fmkt_Side_Sell:  this->BSIdx_ = OmsBSIdx_Sell; break;
      case f9fmkt_Side_Unknown:
      default:
         return OmsErrCode_Bad_Side;
      }
      switch (this->IniReq_->SessionId()) {
      case f9fmkt_TradingSessionId_Normal:
         this->TradingSessionIdx_ = OmsTwsTradingSessionIdx_Normal;
         break;
      case f9fmkt_TradingSessionId_OddLot:
         this->TradingSessionIdx_ = OmsTwsTradingSessionIdx_Odd;
         break;
      case f9fmkt_TradingSessionId_FixedPrice:
         this->TradingSessionIdx_ = OmsTwsTradingSessionIdx_Fixed;
         break;
      default:
         return OmsErrCode_Bad_SessionId;
      }
      fon9_WARN_POP;
      return OmsErrCode_NoError;
   }
};

} // namespaces
#endif//__f9omstws_OmsTwsScBase_hpp__
