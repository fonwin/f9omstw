﻿namespace f9oms
{
   /// 下單失敗的原因: 錯誤代碼.
   /// 0:沒有錯誤, 1..9999: OMS內部錯誤.
   public enum OmsErrCode : System.UInt16
   {
      NoError = 0,

      /// 刪改查要求, 但找不到對應的委託書.
      /// 通常是提供的 IniSNO 或 OrdKey(BrkId or Market or SessionId or OrdNo) 不正確.
      OrderNotFound = 1,
      /// 委託書號已存在, 但遺漏新單回報.
      OrderInitiatorNotFound = 2,
      /// 有問題的 Report(例如:TwsRpt): 可能是重複回報, 回報內容不正確...
      /// 此類回報如果 RxSNO==0, 則無法回補.
      Bad_Report = 3,

      /// 沒有商品資料,無法下單.
      SymbNotFound = 4,
      /// P08的價格小數位數有誤.
      SymbDecimalLocator = 5,

      /// OmsCore::RunInCore();
      /// Request_->Creator_->RunStep_ 沒定義.
      RequestStepNotFound = 10,
      /// 無法將 req 轉成適當的 OmsRequest(例如: OmsTwsRequestChg);
      UnknownRequestType = 11,

      /// OmsRequestIni::BeforeReq_CheckOrdKey();
      /// 下單要求的 BrkId 找不到對應的券商資料.
      Bad_BrkId = 100,
      /// 不是新單要求, 則必須提供 OrdNo.
      Bad_OrdNo = 101,

      Bad_SessionId = 110,
      /// 新單沒有填 SessionId, 且找不到商品資料, 無法自動填 SessionId.
      /// 台灣期交所: 用商品的FlowGroup, 來判斷現在是日盤還是夜盤.
      Bad_SessionId_SymbNotFound = 111,
      /// 新單沒有填 SessionId, 且商品資料的相關欄位不正確(例如 symb->FlowGroup_, symb->TradingSessionId_);
      Bad_SymbSessionId = 112,

      /// 不認識的 MarketId, 無法決定下單要求要送到何處.
      Bad_MarketId = 120,
      /// 新單沒有填 Market, 且找不到商品資料, 無法自動填 MarketId.
      Bad_MarketId_SymbNotFound = 121,
      /// 新單沒有填 Market, 且商品資料的 MarketId 為 f9fmkt_TradingMarket_Unknown.
      Bad_SymbMarketId = 122,

      /// 不正確的 LgOut 線路群組設定.
      Bad_LgOut = 130,

      /// RxKind 有誤.
      Bad_RxKind = 200,
      /// 買賣別問題.
      Bad_Side = 201,
      /// 改單期望的改後數量有誤: 不可增量,或期望的改後數量與現在(LeavesQty+CumQty)相同.
      Bad_ChgQty = 202,
      /// 下單價格問題.
      Bad_Pri = 203,
      Bad_PriType = 204,
      Bad_TimeInForce = 205,
      /// 商品不支援市價單.
      Symb_NoMarketPri = 206,
      /// 不支援改價. (定價交易無改價功能, 或系統不支援改價);
      Symb_NoChgPri = 207,

      /// OmsRequestIni::BeforeReq_CheckIvRight(); Ivr not found, or no permission.
      IvNoPermission = 300,
      /// OmsRequestIni::BeforeReq_CheckIvRight(); 必要欄位不正確(例如: IvacNo, Side, Symbol...)
      FieldNotMatch = 301,
      /// OmsRequestIni::BeforeReq_CheckIvRight(); Order not found, or RequestIni not allowed.
      /// 補單操作, 必須有 AllowAddReport 權限.
      DenyAddReport = 302,
      /// 不支援使用 OmsRequestIni 執行刪改查.
      /// RequestIni 必須是 f9fmkt_RxKind_RequestNew;
      /// 此錯誤碼不一定會發生, 由實際下單步驟決定是否支援.
      IniMustRequestNew = 303,

      /// 客戶端(例:RcClient) 超過下單流量管制: OmsPoUserRightsAgent 裡面的設定.
      OverFlowControl = 400,

      /// OmsOrdNoMap::AllocOrdNo(); 下單的 OrdTeamGroupId 有問題.
      OrdTeamGroupId = 500,
      /// OmsOrdNoMap::AllocOrdNo(); 自編委託書號, 但委託書已存在.
      /// OmsRequestIni::BeforeReq_CheckOrdKey() 新單自編委託書, 但委託書已存在.
      OrderAlreadyExists = 501,
      /// OmsOrdNoMap::AllocOrdNo(); 自訂委託櫃號, 但委託書號已用完.
      OrdNoOverflow = 502,
      /// OmsOrdNoMap::AllocOrdNo(); OmsRequestIni::ValidateInUser();
      /// 沒有自編委託櫃號(或委託書號)的權限. 新單要求的 OrdNo 必須為空白.
      OrdNoMustEmpty = 503,
      /// OmsOrdNoMap::AllocOrdNo(); 委託櫃號已用完(或沒設定).
      OrdTeamUsedUp = 504,
      /// OmsOrdNoMap 沒找到, 可能是 Market 或 SessionId 不正確, 或系統沒設定.
      OrdNoMapNotFound = 505,

      // -----------------------------------------------------------------------
      /// 無可用的下單連線.
      NoReadyLine = 900,

      // -----------------------------------------------------------------------
      /// 09xxx = FIX SessionReject.
      FromExgSessionReject = 9000,

      // -----------------------------------------------------------------------
      /// 來自風控管制的錯誤碼.
      FromRisk = 10000,

      // -----------------------------------------------------------------------
      /// 來自證交所的錯誤碼 = OrderErr::FromTwSEC + 交易所錯誤代號.
      FromTwSEC = 20000,
      FromTwOTC = 30000,
      FromTwFEX = 40000,
   }
}

