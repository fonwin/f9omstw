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

   /// OmsRequestBase::PreCheck_GetRequestInitiator();
   /// 刪改查要求, 但找不到對應的委託書.
   /// 通常是提供的 IniSNO 或 OrdKey(BrkId or Market or SessionId or OrdNo) 不正確.
   OmsErrCode_OrderNotFound = 1,
   /// OmsCore::RunInCore();
   /// Request_->Creator_->RunStep_ 沒定義.
   OmsErrCode_RequestStepNotFound = 2,

   /// OmsRequestIni::PreCheck_OrdKey();
   /// 下單要求的 BrkId 找不到對應的券商資料.
   OmsErrCode_Bad_BrkId = 100,
   /// 不是新單要求, 則必須提供 OrdNo.
   OmsErrCode_Bad_OrdNo = 101,
   OmsErrCode_Bad_SessionId = 102,
   /// 新單沒有填 Market, 且找不到商品資料, 無法自動填 MarketId.
   OmsErrCode_Bad_MarketId_SymbNotFound = 103,
   /// 新單沒有填 Market, 且商品資料的 MarketId 為 f9fmkt_TradingMarket_Unknown.
   OmsErrCode_Bad_SymbMarketId = 104,

   /// OmsRequestIni::PreCheck_IvRight(); Ivr not found, or no permission.
   OmsErrCode_IvNoPermission = 120,
   /// OmsRequestIni::PreCheck_IvRight(); 必要欄位不正確(例如: IvacNo, Side, Symbol...)
   OmsErrCode_FieldNotMatch = 121,
   /// OmsRequestIni::PreCheck_IvRight(); Order not found, or RequestIni not allowed.
   // 若委託不存在(用 OrdKey 找不到委託) => 「委託遺失」的補單操作, 必須有 AllowRequestIni 權限.
   OmsErrCode_DenyRequestIni = 122,

   /// OmsOrdNoMap::AllocOrdNo(); 下單的 OrdTeamGroupId 有問題.
   OmsErrCode_OrdTeamGroupId = 200,
   /// OmsOrdNoMap::AllocOrdNo(); 自編委託書號, 但委託書已存在.
   /// OmsRequestIni::PreCheck_OrdKey() 新單自編委託書, 但委託書已存在.
   OmsErrCode_OrderAlreadyExists = 201,
   /// OmsOrdNoMap::AllocOrdNo(); 自訂委託櫃號, 但委託書號已用完.
   OmsErrCode_OrdNoOverflow = 202,
   /// OmsOrdNoMap::AllocOrdNo(); OmsRequestIni::ValidateInUser();
   /// 沒有自編委託櫃號(或委託書號)的權限. 新單要求的 OrdNo 必須為空白.
   OmsErrCode_OrdNoMustEmpty = 203,
   /// OmsOrdNoMap::AllocOrdNo(); 委託櫃號已用完(或沒設定).
   OmsErrCode_OrdTeamUsedUp = 204,
   /// OmsOrdNoMap 沒找到, 可能是 Market 或 SessionId 不正確, 或系統沒設定.
   OmsErrCode_OrdNoMapNotFound = 205,

   /// 無可用的下單連線.
   OmsErrCode_NoReadyLine = 1000,

   /// 來自風控管制的錯誤碼.
   OmsErrCode_FromSc = 10000,
   /// 來自交易所的錯誤碼 = OrderErr::FromExg + 交易所錯誤代號.
   OmsErrCode_FromExg = 20000,
};

#ifdef __cplusplus
}
#endif
#endif//__f9omstw_OmsErrCode_h__
