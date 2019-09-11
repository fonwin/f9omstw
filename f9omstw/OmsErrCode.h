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
fon9_ENUM(OmsErrCode, uint16_t) {
   OmsErrCode_NoError = 0,

   /// 刪改查要求, 但找不到對應的委託書.
   /// 通常是提供的 IniSNO 或 OrdKey(BrkId or Market or SessionId or OrdNo) 不正確.
   OmsErrCode_OrderNotFound = 1,
   /// 委託書號已存在, 但遺漏新單回報.
   OmsErrCode_OrderInitiatorNotFound = 2,
   /// 有問題的 Report(例如:TwsRpt): 可能是重複回報, 回報內容不正確...
   /// 此類回報如果 RxSNO==0, 則無法回補.
   OmsErrCode_Bad_Report = 3,

   /// OmsCore::RunInCore();
   /// Request_->Creator_->RunStep_ 沒定義.
   OmsErrCode_RequestStepNotFound = 10,
   /// 無法將 req 轉成適當的 OmsRequest(例如: OmsTwsRequestChg);
   OmsErrCode_UnknownRequestType = 11,

   /// OmsRequestIni::BeforeReq_CheckOrdKey();
   /// 下單要求的 BrkId 找不到對應的券商資料.
   OmsErrCode_Bad_BrkId = 100,
   /// 不是新單要求, 則必須提供 OrdNo.
   OmsErrCode_Bad_OrdNo = 101,
   OmsErrCode_Bad_SessionId = 102,
   /// 新單沒有填 Market, 且找不到商品資料, 無法自動填 MarketId.
   OmsErrCode_Bad_MarketId_SymbNotFound = 103,
   /// 新單沒有填 Market, 且商品資料的 MarketId 為 f9fmkt_TradingMarket_Unknown.
   OmsErrCode_Bad_SymbMarketId = 104,
   /// 不認識的 MarketId, 無法決定下單要求要送到何處.
   OmsErrCode_Bad_MarketId = 105,
   /// 不正確的 LgOut 線路群組設定.
   OmsErrCode_Bad_LgOut = 106,

   /// RxKind 有誤.
   OmsErrCode_Bad_RxKind = 110,
   /// 買賣別問題.
   OmsErrCode_Bad_Side = 111,
   /// 新單: 新單數量問題, 超過上限? 非整股數量?
   /// 改單: 期望的改後數量有誤: 不可增量, 或期望的改後數量與現在(LeavesQty+CumQty)相同.
   OmsErrCode_Bad_Qty = 112,
   /// 下單價格問題.
   OmsErrCode_Bad_Pri = 113,
   OmsErrCode_Bad_PriType = 114,
   OmsErrCode_Bad_TimeInForce = 115,

   /// OmsRequestIni::BeforeReq_CheckIvRight(); Ivr not found, or no permission.
   OmsErrCode_IvNoPermission = 120,
   /// OmsRequestIni::BeforeReq_CheckIvRight(); 必要欄位不正確(例如: IvacNo, Side, Symbol...)
   OmsErrCode_FieldNotMatch = 121,
   /// OmsRequestIni::BeforeReq_CheckIvRight(); Order not found, or RequestIni not allowed.
   /// 補單操作, 必須有 AllowAddReport 權限.
   OmsErrCode_DenyAddReport = 122,

   /// 客戶端(例:RcClient) 超過下單流量管制: OmsPoUserRightsAgent 裡面的設定.
   OmsErrCode_OverFlowControl = 130,

   /// OmsOrdNoMap::AllocOrdNo(); 下單的 OrdTeamGroupId 有問題.
   OmsErrCode_OrdTeamGroupId = 200,
   /// OmsOrdNoMap::AllocOrdNo(); 自編委託書號, 但委託書已存在.
   /// OmsRequestIni::BeforeReq_CheckOrdKey() 新單自編委託書, 但委託書已存在.
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

   // -----------------------------------------------------------------------
   /// 無可用的下單連線.
   OmsErrCode_NoReadyLine = 900,

   // -----------------------------------------------------------------------
   /// 09xxx = FIX SessionReject.
   OmsErrCode_FromExgSessionReject = 9000,

   // -----------------------------------------------------------------------
   /// 來自風控管制的錯誤碼.
   OmsErrCode_FromRisk = 10000,

   // -----------------------------------------------------------------------
   /// 來自證交所的錯誤碼 = OrderErr::FromTwSEC + 交易所錯誤代號.
   OmsErrCode_FromTwSEC = 20000,
   OmsErrCode_FromTwOTC = 30000,
   OmsErrCode_FromTwFEX = 40000,
};

#ifdef __cplusplus
}
#endif
#endif//__f9omstw_OmsErrCode_h__
