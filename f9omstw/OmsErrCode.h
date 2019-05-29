// \file f9omstw/OmsErrCode.h
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsErrCode_h__
#define __f9omstw_OmsErrCode_h__
#include "fon9/sys/Config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// 下單失敗的原因: 錯誤代碼.
/// 0:沒有錯誤, 1..9999: OMS內部錯誤.
enum OmsErrCode : uint16_t {
   OmsErrCode_NoError = 0,

   /// 刪改查要求, 但找不到對應的委託書.
   /// 通常是提供的委託Key不正確.
   OmsErrCode_OrderNotFound = 1,

   /// OmsOrdNoMap::AllocOrdNo(); 下單的 runner.OrderRaw_.Order_->ScResource_.OrdTeamGroupId_ 有問題.
   OmsErrCode_OrdTeamGroupId = 10,
   /// OmsOrdNoMap::AllocOrdNo(); 自編委託書號, 但委託書已存在.
   OmsErrCode_OrderAlreadyExists = 11,
   /// OmsOrdNoMap::AllocOrdNo(); 自訂委託櫃號, 但委託書號已用完.
   OmsErrCode_OrdNoOverflow = 12,
   /// OmsOrdNoMap::AllocOrdNo(); 沒有自編委託櫃號(或委託書號)的權限. 新單要求的 OrdNo 必須為空白.
   OmsErrCode_OrdNoMustEmpty = 13,
   /// OmsOrdNoMap::AllocOrdNo(); 委託櫃號已用完(或沒設定).
   OmsErrCode_OrdTeamUsedUp = 14,
   /// OmsOrdNoMap 沒找到, 可能是 Market 或 SessionId 不正確, 或系統沒設定.
   OmsErrCode_OrdNoMapNotFound = 15,

   /// 無可用的下單連線.
   OmsErrCode_NoReadyLine = 100,

   /// 來自風控管制的錯誤碼.
   OmsErrCode_FromSc = 10000,
   /// 來自交易所的錯誤碼 = OrderErr::FromExg + 交易所錯誤代號.
   OmsErrCode_FromExg = 20000,
};

#ifdef __cplusplus
}
#endif
#endif//__f9omstw_OmsErrCode_h__
