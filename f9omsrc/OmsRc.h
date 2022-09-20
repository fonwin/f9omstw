// \file f9omsrc/OmsRc.h
// \author fonwinz@gmail.com
#ifndef __f9omsrc_OmsRc_h__
#define __f9omsrc_OmsRc_h__
#include "fon9/rc/RcClientApi.h"
#include "fon9/rc/RcSeedVisitor.h"
#include "f9omstw/OmsPoIvList.h"

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

#ifndef f9OmsRc_DECL
#  ifdef __GNUC__
#     define f9OmsRc_DECL
#  else
#     if defined(f9OmsRc_BUILD_DLL)
#        define f9OmsRc_DECL __declspec(dllexport)
#     elif defined(f9OmsRc_USE_DLL)
#        define f9OmsRc_DECL __declspec(dllimport)
#     else // defined(f9OmsRc_NO_DLL) 預設使用 static library.
#        define f9OmsRc_DECL
#     endif
#  endif
#endif

#ifndef   f9OmsRc_CALL
#  ifdef __GNUC__
#     define f9OmsRc_CALL
#  else
#     define f9OmsRc_CALL  __stdcall
#  endif
#endif

#define f9OmsRc_API_FN(ReturnType)  extern f9OmsRc_DECL ReturnType f9OmsRc_CALL

// ErrCode 訊息翻譯服務, 透過 f9omstw/OmsMakeErrMsg.h 提供的 functions 載入、釋放.
// typedef struct f9omstw_ErrCodeTx    f9omstw_ErrCodeTx;

//--------------------------------------------------------------------------//

fon9_ENUM(f9OmsRc_OpKind, uint8_t) {
   f9OmsRc_OpKind_Config,

   /// C <- S.
   /// 如果 server 正在建立新的 OmsCore, 則 Client 可能先收到 TDayChanged 然後才收到 Config.
   /// 此時 client 應先將 TDay 保留, 等收到 Config 時, 如果 TDay 不一致, 則送出 TDayConfirm;
   f9OmsRc_OpKind_TDayChanged = 1,
   /// C -> S.
   f9OmsRc_OpKind_TDayConfirm = 1,

   /// C -> S: 回補要求.
   /// C <- S: 回補結束通知.
   f9OmsRc_OpKind_ReportSubscribe,
};

fon9_ENUM(f9OmsRc_RptFilter, uint8_t) {
   f9OmsRc_RptFilter_RptAll = 0,
   /// 回補階段, 回補最後狀態, 例如:
   /// - 若某筆要求已收到交易所回報, 則中間過程的 Queuing, Sending 不回補.
   /// - 若某筆要求正在 Sending, 則中間過程的 Queuing 不回補.
   f9OmsRc_RptFilter_RecoverLastSt = 0x01,
   /// 僅回補 WorkingOrder, 即時回報不考慮此旗標.
   f9OmsRc_RptFilter_RecoverWorkingOrder = 0x02,

   /// 在 NoExternal 的情況下, 是否要回報[其他主機]的新單Request(例: TwsRpt、TwfRpt)?
   /// - OmsRcSyn 模組可設定此旗標.
   /// - 雙主機備援: 若某主機死亡後, 當日不啟用死亡主機, 則可不用設定此旗標.
   /// - 多主機(>2)備援: 備援主機會接手死亡主機的線路, 則需要啟用此旗標.
   ///   這樣才能在 [A主機] 死亡, [B主機] 接手 [A主機] 線路後, [B主機] 將成交回報正確地送給 [C主機];
   f9OmsRc_RptFilter_IncludeRcSynNew = 0x08,

   /// Queuing 不回報(也不回補).
   f9OmsRc_RptFilter_NoQueuing = 0x10,
   /// Sending 不回報(也不回補).
   f9OmsRc_RptFilter_NoSending = 0x20,
   /// 僅回補(回報)成交;
   f9OmsRc_RptFilter_MatchOnly = 0x40,
   /// 排除外部回報, 僅本地下單要求的回報.
   /// 成交回報: 僅回報新單為本地所下的成交.
   f9OmsRc_RptFilter_NoExternal = 0x80,
};

fon9_ENUM(f9OmsRc_LayoutKind, uint8_t) {
   f9OmsRc_LayoutKind_Request = 0,
   f9OmsRc_LayoutKind_ReportRequest = 1,
   f9OmsRc_LayoutKind_ReportRequestIni = 2,
   f9OmsRc_LayoutKind_ReportRequestAbandon = 3,
   f9OmsRc_LayoutKind_ReportOrder = 4,
   f9OmsRc_LayoutKind_ReportEvent = 5,
};

typedef f9sv_Field   f9OmsRc_LayoutField;
typedef uint16_t     f9OmsRc_TableIndex;
typedef uint16_t     f9OmsRc_FieldIndexU;
typedef int16_t      f9OmsRc_FieldIndexS;

/// 下單格式、回報格式.
typedef struct {
   fon9_CStrView              LayoutName_;
   /// 如果是委託回報格式, 則會額外提供是哪種回報造成的委託異動.
   /// 例如: "abandon", "event", "TwsNew", "TwsChg", "TwsFil";
   fon9_CStrView              ExParam_;

   const f9OmsRc_LayoutField* FieldArray_;
   f9OmsRc_FieldIndexU        FieldCount_;

   /// 從 1 開始計算, ReqTableId 或 RptTableId.
   f9OmsRc_TableIndex         LayoutId_;

   /// 如果是 "REQ" 的下單設定, 則此處為 f9OmsRc_LayoutKind_Request;
   /// 如果是 "RPT" 的回報設定, 則根據設定內容.
   f9OmsRc_LayoutKind         LayoutKind_;
   char                       Reserved3___[1];

   /// FieldArray_[IdxKind_] = 下單要求的種類: f9fmkt_RxKind 新刪改查、委託、系統事件...;
   f9OmsRc_FieldIndexS  IdxKind_;
   /// FieldArray_[IdxMarket_] = 下單要求的市場別: f9fmkt_TradingMarket;
   f9OmsRc_FieldIndexS  IdxMarket_;
   /// FieldArray_[IdxSessionId_] = 交易時段: f9fmkt_TradingSessionId;
   f9OmsRc_FieldIndexS  IdxSessionId_;
   /// FieldArray_[IdxBrkId_] = 券商代號;
   f9OmsRc_FieldIndexS  IdxBrkId_;
   /// FieldArray_[IdxOrdNo_] = 委託書號;
   /// 對於一筆委託書: 市場唯一編號 = OrdKey = Market + SessionId + BrkId + OrdNo;
   f9OmsRc_FieldIndexS  IdxOrdNo_;

   f9OmsRc_FieldIndexS  IdxIvacNo_;
   f9OmsRc_FieldIndexS  IdxSubacNo_;
   f9OmsRc_FieldIndexS  IdxIvacNoFlag_;
   f9OmsRc_FieldIndexS  IdxUsrDef_;
   f9OmsRc_FieldIndexS  IdxClOrdId_;
   f9OmsRc_FieldIndexS  IdxSide_;
   f9OmsRc_FieldIndexS  IdxOType_;
   f9OmsRc_FieldIndexS  IdxSymbol_;
   f9OmsRc_FieldIndexS  IdxPriType_;
   f9OmsRc_FieldIndexS  IdxPri_;
   f9OmsRc_FieldIndexS  IdxQty_;
   f9OmsRc_FieldIndexS  IdxPosEff_;
   f9OmsRc_FieldIndexS  IdxTIF_;
   f9OmsRc_FieldIndexS  IdxPri2_;

   // 底下的欄位為回報專用.
   f9OmsRc_FieldIndexS  IdxReqUID_;
   f9OmsRc_FieldIndexS  IdxCrTime_;
   f9OmsRc_FieldIndexS  IdxUpdateTime_;
   f9OmsRc_FieldIndexS  IdxOrdSt_;
   f9OmsRc_FieldIndexS  IdxReqSt_;
   f9OmsRc_FieldIndexS  IdxUserId_;
   f9OmsRc_FieldIndexS  IdxFromIp_;
   f9OmsRc_FieldIndexS  IdxSrc_;
   f9OmsRc_FieldIndexS  IdxSesName_;
   f9OmsRc_FieldIndexS  IdxIniQty_;
   f9OmsRc_FieldIndexS  IdxIniOType_;
   f9OmsRc_FieldIndexS  IdxIniTIF_;

   f9OmsRc_FieldIndexS  IdxChgQty_;
   f9OmsRc_FieldIndexS  IdxErrCode_;
   f9OmsRc_FieldIndexS  IdxMessage_;
   f9OmsRc_FieldIndexS  IdxBeforeQty_;
   f9OmsRc_FieldIndexS  IdxAfterQty_;
   f9OmsRc_FieldIndexS  IdxLeavesQty_;
   f9OmsRc_FieldIndexS  IdxLastExgTime_;

   f9OmsRc_FieldIndexS  IdxLastFilledTime_;
   f9OmsRc_FieldIndexS  IdxCumQty_;
   f9OmsRc_FieldIndexS  IdxCumAmt_;
   f9OmsRc_FieldIndexS  IdxMatchQty_;
   f9OmsRc_FieldIndexS  IdxMatchPri_;
   f9OmsRc_FieldIndexS  IdxMatchKey_;
   f9OmsRc_FieldIndexS  IdxMatchTime_;

   // 第1腳成交.
   f9OmsRc_FieldIndexS  IdxCumQty1_;
   f9OmsRc_FieldIndexS  IdxCumAmt1_;
   f9OmsRc_FieldIndexS  IdxMatchQty1_;
   f9OmsRc_FieldIndexS  IdxMatchPri1_;
   // 第2腳成交.
   f9OmsRc_FieldIndexS  IdxCumQty2_;
   f9OmsRc_FieldIndexS  IdxCumAmt2_;
   f9OmsRc_FieldIndexS  IdxMatchQty2_;
   f9OmsRc_FieldIndexS  IdxMatchPri2_;
   /// 子單下單要求, 會有對應的母單序號.
   f9OmsRc_FieldIndexS  IdxParentRequestSNO_;
   f9OmsRc_FieldIndexS  IdxIniSNO_;

   /// 券商提供的特殊欄位.
   /// 可在收到 FnOnConfig_ 事件時透過:
   /// `f9OmsRc_GetRequestLayout();`
   /// `f9OmsRc_GetReportLayout();`
   /// 取得 layout 之後設定這裡的值.
   f9OmsRc_FieldIndexS  IdxUserFields_[14];
} f9OmsRc_Layout;

//--------------------------------------------------------------------------//

#ifdef __cplusplus
/// 記錄下單訊息 & Config.
#define f9oms_ClientLogFlag_Request  static_cast<f9rc_ClientLogFlag>(0x10000)
/// 記錄回報訊息 & Config.
#define f9oms_ClientLogFlag_Report   static_cast<f9rc_ClientLogFlag>(0x20000)
/// 登入成功後, TDayChanged, Config 相關事件.
#define f9oms_ClientLogFlag_Config   static_cast<f9rc_ClientLogFlag>(0x40000)
#else
#define f9oms_ClientLogFlag_Request  ((f9rc_ClientLogFlag)0x10000)
#define f9oms_ClientLogFlag_Report   ((f9rc_ClientLogFlag)0x20000)
#define f9oms_ClientLogFlag_Config   ((f9rc_ClientLogFlag)0x40000)
#endif

typedef struct {
   uint32_t HostId_;
   uint32_t YYYYMMDD_;
   /// 相同 TDay 的啟動次數.
   uint32_t UpdatedCount_;
} f9OmsRc_CoreTDay;
static inline int f9OmsRc_IsCoreTDayChanged(const f9OmsRc_CoreTDay* a, const f9OmsRc_CoreTDay* b) {
   return a->HostId_ != b->HostId_
      || a->YYYYMMDD_ != b->YYYYMMDD_
      || a->UpdatedCount_ != b->UpdatedCount_;
}

typedef struct {
   /// 帳號字串: "*" or ""BrkId" or "BrkId-IvacNo" or "BrkId-IvacNo-SubacNo"
   fon9_CStrView  IvKey_;
   f9oms_IvRight  IvRights_;
   char           Reserved7__[7];
} f9OmsRc_IvRight;

typedef struct {
   f9OmsRc_CoreTDay              CoreTDay_;

   /// 下單要求的表單數量.
   f9OmsRc_TableIndex            RequestLayoutCount_;
   char                          Padding2__[2];
   /// 下單要求的表單指標陣列.
   const f9OmsRc_Layout* const*  RequestLayoutArray_;
   /// OmsApi 登入成功後, 收到的相關權限資料.
   f9sv_GvTables                 RightsTables_;

   /// 下單流量管制.
   uint16_t                      FcReqCount_;
   uint16_t                      FcReqMS_;
   char                          Padding4__[4]; 
   /// 可用櫃號列表.
   fon9_CStrView                 OrdTeams_;
} f9OmsRc_ClientConfig;

typedef uint64_t  f9OmsRc_SNO;

typedef struct {
   f9OmsRc_SNO             ReportSNO_;
   f9OmsRc_SNO             ReferenceSNO_;

   /// 回報事件若此值為 NULL, 則表示回補完畢通知, 之後開始接收即時回報.
   const f9OmsRc_Layout*   Layout_;

   /// 不能保留這裡的指標, 回報通知完畢後, 字串會被銷毀.
   /// 欄位數量可從 Layout_->FieldCount_ 得到.
   const fon9_CStrView*    FieldArray_;
} f9OmsRc_ClientReport;

typedef void (f9OmsRc_CALL *f9OmsRc_FnOnConfig)(f9rc_ClientSession*, const f9OmsRc_ClientConfig* cfg);
typedef void (f9OmsRc_CALL *f9OmsRc_FnOnReport)(f9rc_ClientSession*, const f9OmsRc_ClientReport* rpt);
typedef void (f9OmsRc_CALL *f9OmsRc_FnOnFlowControl)(f9rc_ClientSession*, unsigned usWait);

/// OmsRc API 客戶端用戶的事件處理程序及必要參數.
typedef struct {
   f9rc_FunctionNoteParams          BaseParams_;
   const struct f9omstw_ErrCodeTx*  ErrCodeTx_;

   /// 通知時機:
   /// - 當登入成功後, Server 回覆現在交易日, 使用者設定.
   /// - 當 Server 切換到新的交易日.
   /// - 您應該在收到此事件時「回補 & 訂閱」回報.
   f9OmsRc_FnOnConfig   FnOnConfig_;

   /// 當收到回報時通知.
   ///
   /// \code
   /// // f9OmsRc_ClientReport* rpt;
   /// const f9OmsRc_Layout* layout = rpt->Layout_;
   /// \endcode
   ///
   /// 取得特定欄位的回報名稱及內容:
   /// \code
   /// if (layout->IdxKind_ >= 0) {
   ///   layout->FieldArray_[layout->IdxKind_]; // 欄位名稱及描述.
   ///   rpt->FieldArray_[layout->IdxKind_];    // 欄位回報內容.
   /// }
   /// \endcode
   ///
   /// 依序取得欄位的回報名稱及內容:
   /// \code
   /// for (unsigned L = 0; L < layout->FieldCount_; ++L) {
   ///   layout->FieldArray_[L]; // 欄位名稱及描述.
   ///   rpt->FieldArray_[L];    // 欄位回報內容.
   /// }
   /// \endcode
   f9OmsRc_FnOnReport   FnOnReport_;

   /// 當下單遇到流量管制時通知.
   /// 如果這裡為 NULL, 則由 API 「等候流量解除」然後「送單」, 之後才返回.
   /// 您也可以在送單前自主呼叫 f9OmsRc_CheckFcRequest() 取得流量管制需要等候的時間.
   f9OmsRc_FnOnFlowControl FnOnFlowControl_;
} f9OmsRc_ClientSessionParams;

static inline void f9OmsRc_InitClientSessionParams(f9rc_ClientSessionParams* f9rcCliParams, f9OmsRc_ClientSessionParams* omsRcParams) {
   f9rc_InitClientSessionParams(f9rcCliParams, omsRcParams, f9rc_FunctionCode_OmsApi);
}

//--------------------------------------------------------------------------//

/// 取得 f9OmsRc API 版本字串.
f9OmsRc_API_FN(const char*) f9OmsRc_ApiVersionInfo(void);

/// 啟動 f9OmsRc(fon9 OMS Rc client) 函式庫.
/// - 請使用時注意: 禁止 multi thread 同時呼叫 f9OmsRc_Initialize()/fon9_Finalize();
/// - 可重覆呼叫 f9OmsRc_Initialize(), 但須有對應的 fon9_Finalize();
///   最後一個 fon9_Finalize() 被呼叫時, 結束函式庫.
/// - 其餘請參考 fon9_Initialize();
/// - 然後透過 f9rc_CreateClientSession() 建立 Session;
///   - 建立 Session 時, 必須提供 f9OmsRc_ClientSessionParams; 參數.
///   - 且需要經過 f9OmsRc_InitClientSessionParams() 初始化.
f9OmsRc_API_FN(int) f9OmsRc_Initialize(const char* logFileFmt);

/// 用指定名稱取得「下單表單格式」.
/// - cfg 必須是在 FnOnConfig_ 事件裡面提供的那個;
/// - 這裡取得的 layout 將在下次 FnOnConfig_ 事件之後銷毀.
/// - 建議您在處理 FnOnConfig_ 事件時, 取得需要的 request layout.
/// - 可自行設定 retval->IdxUserFields_[], 不可更改 retval 的其他 members;
/// - 您也可以透過 cfg->RequestLayoutArray_[] 循序搜尋 layout.
f9OmsRc_API_FN(f9OmsRc_Layout*)
f9OmsRc_GetRequestLayout(f9rc_ClientSession* ses,
                         const f9OmsRc_ClientConfig* cfg,
                         const char* reqName);

/// 用 rptTableId(從1開始) 取得「回報表單格式」, 回報表單可能會有相同名稱.
/// - cfg 必須是在 FnOnConfig_ 事件裡面提供的那個;
/// - 這裡取得的 layout 將在下次 FnOnConfig_ 事件之後銷毀.
/// - 可自行設定 retval->IdxUserFields_[], 不可更改 retval 的其他 members;
f9OmsRc_API_FN(f9OmsRc_Layout*)
f9OmsRc_GetReportLayout(f9rc_ClientSession* ses,
                        const f9OmsRc_ClientConfig* cfg,
                        unsigned rptTableId);

/// 訂閱回報, ses 與 cfg 必須是 FnOnConfig_ 事件通知提供的.
f9OmsRc_API_FN(void)
f9OmsRc_SubscribeReport(f9rc_ClientSession* ses,
                        const f9OmsRc_ClientConfig* cfg,
                        f9OmsRc_SNO from,
                        f9OmsRc_RptFilter filter);

/// 傳送下單要求: 已組好的字串形式.
/// \retval 1=true  下單要求已送出.
/// \retval 0=false 無法下單: 沒有呼叫過 f9OmsRc_Initialize();
///                 或建立 ses 時, 沒有提供 f9OmsRc_ClientSessionParams 參數.
f9OmsRc_API_FN(int)
f9OmsRc_SendRequestString(f9rc_ClientSession* ses,
                          const f9OmsRc_Layout* reqLayout,
                          const fon9_CStrView reqStr);

/// 傳送下單要求: 已填妥下單欄位字串陣列: reqFieldArray[0 .. reqLayout->FieldCount_);
/// \retval 1=true  下單要求已送出.
/// \retval 0=false 無法下單: 沒有呼叫過 f9OmsRc_Initialize();
///                 或建立 ses 時, 沒有提供 f9OmsRc_ClientSessionParams 參數.
f9OmsRc_API_FN(int)
f9OmsRc_SendRequestFields(f9rc_ClientSession* ses,
                          const f9OmsRc_Layout* reqLayout,
                          const fon9_CStrView* reqFieldArray);

/// 傳送下單要求之前, 可以自行檢查流量.
/// 傳回需要等候的 microseconds. 傳回 0 表示不需管制.
f9OmsRc_API_FN(unsigned)
f9OmsRc_CheckFcRequest(f9rc_ClientSession* ses);

typedef struct {
   const f9OmsRc_Layout*   Layout_;
   fon9_CStrView           ReqStr_;
} f9OmsRc_RequestBatch;
/// 傳送一批下單要求.
/// - 若遇到流量管制, 則已打包的會先送出, 然後用 f9OmsRc_ClientSessionParams.FnOnFlowControl_ 機制處理。
/// - 若打包資料量超過 n bytes(例: 1K bytes), 則會先送出, 然後繼續打包之後的下單要求。
/// \retval 1=true  下單要求已送出.
/// \retval 0=false 無法下單: 沒有呼叫過 f9OmsRc_Initialize();
///                 或建立 ses 時, 沒有提供 f9OmsRc_ClientSessionParams 參數.
f9OmsRc_API_FN(int)
f9OmsRc_SendRequestBatch(f9rc_ClientSession*         ses,
                         const f9OmsRc_RequestBatch* reqBatch,
                         unsigned                    reqCount);
/// 取得 f9OmsRc_SendRequestBatch() 最大打包資料量.
/// 因 TCP/Ethernet 封包的特性, 若資料量超過 MSS, 則在網路線上傳送時, 需要拆包送出,
/// 接收端需要等候併包後才能處理, 反而造成更大的延遲, 所以系統供一個最大打包資料量.
/// 一旦批次下單打包資料量超過此值, 就先送出, 然後繼續打包後續的下單要求.
f9OmsRc_API_FN(unsigned) f9OmsRc_GetRequestBatchMSS(void);
/// 設定新的 f9OmsRc_SendRequestBatch() 最大打包資料量.
/// 返回設定前的值.
f9OmsRc_API_FN(unsigned) f9OmsRc_SetRequestBatchMSS(unsigned value);

#ifdef __cplusplus
}//extern "C"
#endif//__cplusplus
#endif//__f9omsrc_OmsRc_h__
