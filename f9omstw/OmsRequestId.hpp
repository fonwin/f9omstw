// \file f9omstw/OmsRequestId.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestId_hpp__
#define __f9omstw_OmsRequestId_hpp__
#include "f9omstw/OmsRxItem.hpp"
#include "fon9/CharAry.hpp"

namespace f9omstw {

/// Request 在多主機環境下的唯一編號.
/// - 外部單: 由外部單(回報)提供者提供編碼規則, 例如:
///   - Market + Session + BrkId[2尾碼] + OrdNo + 外部單Seq(或AfQty).
///   - Market + Session + BrkId[2尾碼] + OrdNo + 成交序號.
/// - 否則: ReqUID 透過 OmsReqUID_Builder 填入(與 LocalHostId, RxSNO 相關).
struct OmsRequestId {
   fon9::CharAry<16> ReqUID_;
   OmsRequestId() {
      *ReqUID_.begin() = 0;
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

   /// 此時若 *req.ReqUID_.begin() == '\0'; 則會編製 req.ReqUID_;
   void MakeReqUID(OmsRequestId& req, OmsRxSNO sno);
};

} // namespaces
#endif//__f9omstw_OmsRequestId_hpp__
