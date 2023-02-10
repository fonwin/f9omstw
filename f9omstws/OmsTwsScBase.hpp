// \file f9omstws/OmsTwsScBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsScBase_hpp__
#define __f9omstws_OmsTwsScBase_hpp__
#include "f9omstws/OmsTwsRequest.hpp"
#include "f9omstw/OmsScBase.hpp" // OmsErrCode_Sc_BadOType

namespace f9omstw {

struct OmsTwsScBase {
   const OmsTwsRequestIni* IniReq_;
   OmsBSIdx                BSIdx_;
   OmsOTypeIdx             OTypeIdx_;
   OmsTwsTradingSessionIdx TradingSessionIdx_;
   char                    Padding____[5];

   /// \retval OmsErrCode_Null
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
      switch (this->IniReq_->OType_) {
      case f9tws::TwsOType::Gn:
      case f9tws::TwsOType::DayTradeGn:
         this->OTypeIdx_ = OmsOTypeIdx_Gn;
         break;
      case f9tws::TwsOType::DayTradeCD:
         this->OTypeIdx_ = (this->BSIdx_ == OmsBSIdx_Buy
                            ? OmsOTypeIdx_Cr
                            : OmsOTypeIdx_Db);
         break;
      case f9tws::TwsOType::CrAgent:
      case f9tws::TwsOType::CrSelf:
         this->OTypeIdx_ = OmsOTypeIdx_Cr;
         break;
      case f9tws::TwsOType::DbAgent:
      case f9tws::TwsOType::DbSelf:
         this->OTypeIdx_ = OmsOTypeIdx_Db;
         break;
      default:
      case f9tws::TwsOType::SBL5:
      case f9tws::TwsOType::SBL6:
         return OmsErrCode_Sc_BadOType;
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
      return OmsErrCode_Null;
   }
};

} // namespaces
#endif//__f9omstws_OmsTwsScBase_hpp__
