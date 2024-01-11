﻿// \file f9omstw/OmsErrCode.h
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsErrCode_h__
#define __f9omstw_OmsErrCode_h__
#include "fon9/sys/Config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// 嚴格來說, 此為 [訊息代碼], 應該採用 OmsMsgCode 或 OmsStCode, 但為了相容性, 只好將錯就錯了!
/// - 0:沒有額外須提供的訊息, 1..9999: OMS內部訊息.
/// - 非 0 不一定代表錯誤或失敗(例: 期交所 40248:Session已達設定之流量值80%);
///   委託錯誤或失敗, 應先從 OrdSt 或 ReqSt 來判斷, 然後再透過 OmsErrCode 來判斷原因.
/// - OmsErrCode_*OK 不一定會提供, 由回報處理程序決定.
fon9_ENUM(OmsErrCode, uint16_t) {
   OmsErrCode_MaxV = 0xffff,

   /// 因為 OmsErrCode 不一定代表失敗, 為了避免誤用, 所以取消 OmsErrCode_NoError, 改成 OmsErrCode_Null;
   OmsErrCode_Null = 0,

   OmsErrCode_NewOrderOK = 6,
   OmsErrCode_DelOrderOK = 7,
   OmsErrCode_ChgQtyOK   = 8,
   OmsErrCode_ChgPriOK   = 9,
   /// 詢價OK.
   OmsErrCode_QuoteROK = 13,
   /// 報價OK.
   OmsErrCode_QuoteOK = 14,
   OmsErrCode_IocDelOK = 15,
   OmsErrCode_FokDelOK = 16,
   /// 成交回報, 如果尚未收到新單結果, 強制執行 RunReportInCore_FilledBeforeNewDone();
   /// 台灣期交所: 不理會此設定, 因為期交所不會改變[新單要求量],
   ///            所以一定會處理 RunReportInCore_FilledBeforeNewDone(),
   ///            且異動時的 ErrCode 依然填入 OmsErrCode_NewOrderOK or OmsErrCode_QuoteOK;
   /// 台灣證交所: 預設成交回報不會使用此代碼(若先收到成交,則先保留,等到收到新單回報,再處理保留的成交),
   ///            - 因為證交所: 實際[新單下單成功量]可能會小於[新單要求量];
   ///            - 只有確定券商回報當有[成交回報]時, 不會送出[新單回報], 才會在 OmsTwsFilled 填入此代碼.
   ///              此時異動時的 ErrCode 填入 OmsErrCode_FilledBeforeNewDone;
   OmsErrCode_FilledBeforeNewDone = 96,
   /// ec <= OmsErrCode_LastInternalOK 表示OK的訊息代碼.
   /// 但 ec > OmsErrCode_LastInternalOK 不一定代表失敗, 例: 40249:Session已達設定之流量值90%;
   OmsErrCode_LastInternalOK = 99,

   /// OmsRequestIni::BeforeReq_CheckOrdKey();
   /// 下單要求的 BrkId 找不到對應的券商資料.
   OmsErrCode_Bad_BrkId = 100,
   /// 不是新單要求, 則必須提供 OrdNo.
   OmsErrCode_Bad_OrdNo = 101,

   OmsErrCode_Bad_SessionId = 110,
   /// 新單沒有填 SessionId, 且找不到商品資料, 無法自動填 SessionId.
   /// 台灣期交所: 用商品的FlowGroup, 來判斷現在是日盤還是夜盤.
   OmsErrCode_Bad_SessionId_SymbNotFound = 111,
   /// 新單沒有填 SessionId, 且商品資料的相關欄位不正確(例如 symb->FlowGroup_, symb->TradingSessionId_);
   OmsErrCode_Bad_SymbSessionId = 112,
   /// 已收盤, 由風控步驟自行判斷 Symb->TradingSessionSt_ 處理.
   OmsErrCode_SessionClosedRejected = 113,
   /// 以收盤, 由交易所(或券商系統)取消剩餘量.
   OmsErrCode_SessionClosedCanceled = 114,

   /// 不認識的 MarketId, 無法決定下單要求要送到何處.
   OmsErrCode_Bad_MarketId = 120,
   /// 新單沒有填 Market, 且找不到商品資料, 無法自動填 MarketId.
   OmsErrCode_Bad_MarketId_SymbNotFound = 121,
   /// 新單沒有填 Market, 且商品資料的 MarketId 為 f9fmkt_TradingMarket_Unknown.
   OmsErrCode_Bad_SymbMarketId = 122,

   /// 不正確的 LgOut 線路群組設定.
   OmsErrCode_Bad_LgOut = 130,

   /// RxKind 有誤.
   OmsErrCode_Bad_RxKind = 200,
   /// 買賣別問題.
   OmsErrCode_Bad_Side = 201,
   /// 改單期望的改後數量有誤: 不可增量,或期望的改後數量與現在(LeavesQty+CumQty)相同.
   OmsErrCode_Bad_ChgQty = 202,
   /// 下單價格問題.
   OmsErrCode_Bad_Pri = 203,
   OmsErrCode_Bad_PriType = 204,
   OmsErrCode_Bad_TimeInForce = 205,
   /// 商品不支援市價單.
   OmsErrCode_Symb_NoMarketPri = 206,
   /// 不支援改價. (定價交易無改價功能, 或系統不支援改價);
   OmsErrCode_Symb_NoChgPri = 207,
   /// 已觸發, 無法更改條件內容;
   OmsErrCode_Triggered_Cannot_ChgCond = 208,

   /// OmsRequestIni::BeforeReq_CheckIvRight(); Ivr not found, or no permission.
   OmsErrCode_IvNoPermission = 300,
   /// OmsRequestIni::BeforeReq_CheckIvRight(); 必要欄位不正確(例如: IvacNo, Side, Symbol...)
   OmsErrCode_FieldNotMatch = 301,
   /// OmsRequestIni::BeforeReq_CheckIvRight(); Order not found, or RequestIni not allowed.
   /// 補單操作, 必須有 AllowAddReport 權限.
   OmsErrCode_DenyAddReport = 302,
   /// 不支援使用 OmsRequestIni 執行刪改查.
   /// RequestIni 必須是 f9fmkt_RxKind_RequestNew;
   /// 此錯誤碼不一定會發生, 由實際下單步驟決定是否支援.
   OmsErrCode_IniMustRequestNew = 303,

   /// 客戶端(例:RcClient) 超過下單流量管制: OmsPoUserRightsAgent 裡面的設定.
   OmsErrCode_OverFlowControl = 400,

   /// OmsOrdNoMap::AllocOrdNo(); 下單的 OrdTeamGroupId 有問題.
   OmsErrCode_OrdTeamGroupId = 500,
   /// OmsOrdNoMap::AllocOrdNo(); 自編委託書號, 但委託書已存在.
   /// OmsRequestIni::BeforeReq_CheckOrdKey() 新單自編委託書, 但委託書已存在.
   OmsErrCode_OrderAlreadyExists = 501,
   /// OmsOrdNoMap::AllocOrdNo(); 自訂委託櫃號, 但委託書號已用完.
   OmsErrCode_OrdNoOverflow = 502,
   /// OmsOrdNoMap::AllocOrdNo(); OmsRequestIni::ValidateInUser();
   /// 沒有自編委託櫃號(或委託書號)的權限. 新單要求的 OrdNo 必須為空白.
   OmsErrCode_OrdNoMustEmpty = 503,
   /// OmsOrdNoMap::AllocOrdNo(); 委託櫃號已用完(或沒設定).
   OmsErrCode_OrdTeamUsedUp = 504,
   /// OmsOrdNoMap 沒找到, 可能是 Market 或 SessionId 不正確, 或系統沒設定.
   OmsErrCode_OrdNoMapNotFound = 505,
   /// OmsOrdNoMap::AllocOrdNo(); 自編委託書號(或自選櫃號), 但使用者無此櫃號權限.
   OmsErrCode_OrdTeamDeny = 506,

   // -----------------------------------------------------------------------
   /// 透過遠端主機協助繞送.
   /// 此時的 ReqSt 必定為 f9fmkt_TradingRequestSt_AskToRemote;
   /// 訊息內容為: "AskToRemote=RemoteHostId";
   /// RemoteHostId 為遠端主機代號.
   OmsErrCode_AskToRemote = 600,
   /// 繞送接收端, 回報回補時才發現繞進請求.
   /// 則此筆下單要求視為失敗.
   /// 回報回補: 主機間斷線後重新連線(或系統重啟).
   /// 由繞送的接收端負責處理此錯誤.
   OmsErrCode_FailAskToRemote = 601,
   /// 繞送請求端, 回報回補時才發現繞送退單(OmsErrCode_RemoteBack_NoReadyLine or Busy).
   /// 此筆下單要求視為失敗.
   /// 回報回補: 主機間斷線後重新連線(或系統重啟).
   /// 由繞送的請求端負責處理此錯誤.
   OmsErrCode_FailOnRecoverAskResult = 602,
   /// 繞送接收端退回繞進請求: [繞送接收端] 無可用線路.
   OmsErrCode_RemoteBack_NoReadyLine = 610,
   /// 繞送接收端退回繞進請求: [繞送接收端] 忙碌中.
   OmsErrCode_RemoteBack_Busy = 611,
      
   // -----------------------------------------------------------------------
   /// 刪改查要求, 但找不到對應的委託書.
   /// 通常是提供的 IniSNO 或 OrdKey(BrkId or Market or SessionId or OrdNo) 不正確.
   OmsErrCode_OrderNotFound = 701,
   /// 委託書號已存在, 但遺漏新單回報.
   OmsErrCode_OrderInitiatorNotFound = 702,
   /// 有問題的 Report(例如:TwsRpt): 可能是重複回報, 回報內容不正確...
   /// 此類回報如果 RxSNO==0, 則無法回補.
   OmsErrCode_Bad_Report = 703,

   /// 沒有商品資料,無法下單.
   OmsErrCode_SymbNotFound = 704,
   /// P08的價格小數位數有誤.
   OmsErrCode_SymbDecimalLocator = 705,

   /// OmsCore::RunInCore();
   /// Request_->Creator_->RunStep_ 沒定義.
   OmsErrCode_RequestStepNotFound = 710,
   /// 無法將 req 轉成適當的 OmsRequest(例如: OmsTwsRequestChg);
   OmsErrCode_UnknownRequestType = 711,
   /// 改單要求不支援此種委託.
   OmsErrCode_RequestNotSupportThisOrder = 712,

   // -----------------------------------------------------------------------
   /// 無可用的下單連線.
   OmsErrCode_NoReadyLine = 900,
   /// 新單超過有效時間, 從新單建立開始, 到送出時, 若超過設定的時間, 則視為失敗.
   OmsErrCode_OverVaTimeMS = 901,
   /// 斷線後回覆, 交易所沒有回覆該筆回報.
   OmsErrCode_FailSending = 902,
   /// 條件單, 尚未觸發前, 發現行情斷線.
   OmsErrCode_MdNoData = 903,
   /// 下單要求在尚未執行(RunInCore)前, 被取消.
   OmsErrCode_AbandonBeforeRun = 904,

   // -----------------------------------------------------------------------
   /// 09xxx = FIX SessionReject.
   OmsErrCode_FromFixSessionReject = 9000,

   // -----------------------------------------------------------------------
   /// 來自風控管制的錯誤碼.
   OmsErrCode_FromRisk = 10000,

   // -----------------------------------------------------------------------
   /// 來自證交所的錯誤碼 = OrderErr::FromTwSEC + 交易所錯誤代號.
   OmsErrCode_FromTwSEC = 20000,
   OmsErrCode_FromTwOTC = 30000,
   OmsErrCode_FromTwFEX = 40000,
   /// 40047: 期交所因價穩措施刪單.
   OmsErrCode_Twf_DynPriBandRej = OmsErrCode_FromTwFEX + 47,
   /// 40048: 期交所價穩措施刪單後, 告知實際的限制價格.
   OmsErrCode_Twf_DynPriBandRpt = OmsErrCode_FromTwFEX + 48,

   // -----------------------------------------------------------------------
   /// 透過券商系統下單, 券商回覆的錯誤訊息.
   OmsErrCode_FromBroker = 50000,
};

#ifdef __cplusplus
}
#endif
#endif//__f9omstw_OmsErrCode_h__
