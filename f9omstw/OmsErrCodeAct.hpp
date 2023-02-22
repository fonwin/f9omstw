// \file f9omstw/OmsErrCodeAct.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsErrCodeAct_hpp__
#define __f9omstw_OmsErrCodeAct_hpp__
#include "f9omstw/OmsBase.hpp"
#include "f9omstw/OmsErrCode.h"
#include "fon9/TimeStamp.hpp"
#include "fon9/LevelArray.hpp"

namespace f9omstw {

struct OmsErrCodeAct : public fon9::intrusive_ref_counter<OmsErrCodeAct> {
   fon9_NON_COPY_NON_MOVE(OmsErrCodeAct);
   OmsErrCodeAct(OmsErrCode ec) : ErrCode_{ec} {
   }

   const OmsErrCode        ErrCode_;
   uint16_t                RerunTimes_{0};
   OmsErrCode              ReErrCode_{OmsErrCode_MaxV};
   bool                    IsResetOkErrCode_{false};
   f9fmkt_TradingRequestSt ReqSt_{};
   bool                    IsUseNewLine_{false};
   bool                    IsAtNewDone_{false};
   char                    padding___[2];

   fon9::CharVector        OTypes_;
   fon9::CharVector        Srcs_;
   fon9::CharVector        NewSrcs_;
   fon9::TimeInterval      NewSending_;

   /// - BeginTime_ < EndTime_:  結束日=開始日:   (BeginTime_ <= now && now <= EndTime_)
   /// - BeginTime_ >= EndTime_: 結束日=開始日+1: (BeginTime_ <= now || now <= EndTime_)
   fon9::DayTime     BeginTime_{fon9::DayTime::Null()};
   fon9::DayTime     EndTime_{fon9::DayTime::Null()};
   bool CheckTime(fon9::DayTime now) const;

   /// 同一個 OmsErrCode 可支援多組設定.
   OmsErrCodeActSP   Next_;

   std::string ConfigStr_;
   std::string ConfigAt_;
   std::string Memo_;

   friend void RevPrint(fon9::RevBuffer& rbuf, const OmsErrCodeAct&);
   /// \retval nullptr  表示成功.
   /// \retval !nullptr 異常的位置, 此時若 msg != nullptr, 則填入錯誤原因.
   const char* ParseFrom(fon9::StrView cfg, std::string* msg);
};
using OmsErrCodeActAry = fon9::LevelArray<OmsErrCode, OmsErrCodeActSP>;

} // namespaces
#endif//__f9omstw_OmsErrCodeAct_hpp__
