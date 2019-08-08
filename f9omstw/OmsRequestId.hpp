// \file f9omstw/OmsRequestId.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestId_hpp__
#define __f9omstw_OmsRequestId_hpp__
#include "f9omstw/OmsBase.hpp"

namespace f9omstw {

/// Request 在多主機環境下的唯一編號.
/// - f9omstw 的下單要求: ReqUID 透過 OmsReqUID_Builder 填入(與 LocalHostId, RxSNO 相關).
/// - 外部單: 由外部單(回報)提供者, 提供編碼規則, 例如:
///   - Market + SessionId + BrkId[2尾碼] + OrdNo + 外部單唯一序號(或BeforeQty).
///     - 不適合用 AfterQty: 因為刪減成功 AfterQty 可能為 0, 刪減失敗(NoLeavesQty) AfterQty 也是 0.
///   - Market + SessionId + BrkId[2尾碼] + 成交市場序號.
///   - Market + SessionId + BrkId[2尾碼] + 線路Id + 回報序號. (期交所成交)
struct OmsRequestId {
   fon9::CharAry<24> ReqUID_;

   OmsRequestId() {
      memset(this, 0, sizeof(*this));
   }
};

class OmsReqUID_Builder {
   // [buffer.LocalHostId--------]  '-' = '\0'
   //        \_ sizeof(ReqUID) _/ 
   char  Buffer_[64 + sizeof(OmsRequestId)];

public:
   /// 必須在 LocalHostId 準備好之後建構.
   OmsReqUID_Builder();

   char* RevStart() {
      return this->Buffer_ + sizeof(this->Buffer_) - sizeof(OmsRequestId);
   }

   /// 此時若 req.ReqUID_.empty1st(); 則會編製 req.ReqUID_;
   void MakeReqUID(OmsRequestId& req, OmsRxSNO sno);

   /// 從 reqId 解析出 OmsRxSNO;
   static OmsRxSNO ParseRequestId(const OmsRequestId& reqId);
};

} // namespaces
#endif//__f9omstw_OmsRequestId_hpp__
