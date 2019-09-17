// \file f9omsrc/OmsRc.h
// \author fonwinz@gmail.com
#ifndef __f9omsrc_OmsRc_h__
#define __f9omsrc_OmsRc_h__
#include "fon9/io/IoState.h"
#include "fon9/rc/Rc.h"
#include "f9omstw/OmsPoIvList.h"
#include "f9omsrc/OmsGvTable.h"
#include <stdint.h>

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

/// ErrCode 訊息翻譯服務, 透過 OmsMakeErrMsg.h 提供的 functions 載入、釋放.
typedef struct f9omstw_ErrCodeTx    f9omstw_ErrCodeTx;

//--------------------------------------------------------------------------//

/// 在 server 回覆 Config 時, 提供 table 資料時,
/// 行的第1個字元, 如果是 *fon9_kCSTR_LEAD_TABLE, 則表示一個 table 的開始.
/// ```
/// fon9_kCSTR_LEAD_TABLE + TableNamed\n
/// FieldNames\n
/// FieldValues\n    可能有 0..n 行
/// ```
#define fon9_kCSTR_LEAD_TABLE    "\x02"

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
   f9OmsRc_RptFilter_AllPass = 0,
   /// 回補階段, 回補最後狀態, 例如:
   /// - 若某筆要求已收到交易所回報, 則中間過程的 Queuing, Sending 不回補.
   /// - 若某筆要求正在 Sending, 則中間過程的 Queuing 不回補.
   f9OmsRc_RptFilter_RecoverLastSt = 0x01,
   /// Queuing 不回報(也不回補).
   f9OmsRc_RptFilter_NoQueuing = 0x10,
   /// Sending 不回報(也不回補).
   f9OmsRc_RptFilter_NoSending = 0x20,
};

fon9_ENUM(f9OmsRc_LayoutKind, uint8_t) {
   f9OmsRc_LayoutKind_Request,
   f9OmsRc_LayoutKind_ReportRequest,
   f9OmsRc_LayoutKind_ReportRequestIni,
   f9OmsRc_LayoutKind_ReportRequestAbandon,
   f9OmsRc_LayoutKind_ReportOrder,
   f9OmsRc_LayoutKind_ReportEvent,
};

typedef struct {
   fon9_CStrView  Name_;
   fon9_CStrView  Title_;
   fon9_CStrView  Description_;
   /// 欄位型別.
   /// - Cn = char[n], 若 n==0 則為 Blob.Chars 欄位.
   /// - Bn = byte[n], 若 n==0 則為 Blob.Bytes 欄位.
   /// - Un[.s] = Unsigned, n=bytes(1,2,4,8), s=scale(小數位數)可為0
   /// - Sn[.s] = Signed, n=bytes(1,2,4,8), s=scale(小數位數)可為0
   /// - Unx = Unsigned, n=bytes(1,2,4,8), CellRevPrint()使用Hex輸出
   /// - Snx = Signed, n=bytes(1,2,4,8), CellRevPrint()使用Hex輸出
   /// - Ti = TimeInterval
   /// - Ts = TimeStamp
   char           TypeId_[16];
} f9OmsRc_LayoutField;

typedef uint16_t  f9OmsRc_TableIndex;
typedef uint16_t  f9OmsRc_FieldIndexU;
typedef int16_t   f9OmsRc_FieldIndexS;

/// 下單格式、回報格式.
typedef struct {
   fon9_CStrView              LayoutName_;
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
   /// FieldArray_[IdxBrkId_] = 勸商代號;
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

   /// 券商提供的特殊欄位.
   /// 可在收到 FnOnConfig_ 事件時透過:
   /// `f9OmsRc_GetRequestLayout();`
   /// `f9OmsRc_GetReportLayout();`
   /// 取得 layout 之後設定這裡的值.
   f9OmsRc_FieldIndexS  IdxUserFields_[16];
} f9OmsRc_Layout;

//--------------------------------------------------------------------------//

fon9_ENUM(f9OmsRc_ClientLogFlag, uint8_t) {
   f9OmsRc_ClientLogFlag_None = 0,
   f9OmsRc_ClientLogFlag_All = 0xff,
   /// 記錄下單訊息 & Config.
   f9OmsRc_ClientLogFlag_Request = 0x01,
   /// 記錄回報訊息 & Config.
   f9OmsRc_ClientLogFlag_Report = 0x02,
   /// 登入成功後, TDayChanged, Config 相關事件.
   f9OmsRc_ClientLogFlag_Config = 0x04,
   /// 連線(斷線)相關事件.
   f9OmsRc_ClientLogFlag_Link = 0x08,
};

typedef struct {
   void*                   UserData_;
   f9OmsRc_ClientLogFlag   LogFlags_;
   char                    Reserved7___[7];
} f9OmsRc_ClientSession;

typedef struct {
   uint32_t HostId_;
   uint32_t YYYYMMDD_;
   /// 相同 TDay 的啟動次數.
   uint32_t UpdatedCount_;
} f9OmsRc_CoreTDay;
inline int f9OmsRc_IsCoreTDayChanged(const f9OmsRc_CoreTDay* a, const f9OmsRc_CoreTDay* b) {
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
   char                          Reserved2___[2];
   /// 下單要求的表單指標陣列.
   const f9OmsRc_Layout* const*  RequestLayoutArray_;

   /// 收到的表格資料, 以字串形式表示.
   /// 每個表格用底下形式提供:
   /// \code
   /// fon9_kCSTR_LEAD_TABLE("\x02") + TableNamed\n
   /// FieldNames\n     使用 fon9_kCSTR_CELLSPL("\x01") 分隔
   /// FieldValues\n    可能有 0..n 行
   /// \endcode
   fon9_CStrView                 TablesStrView_;
   const f9oms_GvTable* const*   TableList_;
   f9OmsRc_TableIndex            TableCount_;
   char                          Reserved2a___[2];

   /// 下單流量管制.
   uint16_t                      FcReqCount_;
   uint16_t                      FcReqMS_;
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

typedef void (f9OmsRc_CALL *f9OmsRc_FnOnLinkEv)(f9OmsRc_ClientSession*, f9io_State st, fon9_CStrView info);
typedef void (f9OmsRc_CALL *f9OmsRc_FnOnConfig)(f9OmsRc_ClientSession*, const f9OmsRc_ClientConfig* cfg);
typedef void (f9OmsRc_CALL *f9OmsRc_FnOnReport)(f9OmsRc_ClientSession*, const f9OmsRc_ClientReport* rpt);
typedef void (f9OmsRc_CALL *f9OmsRc_FnOnFlowControl)(f9OmsRc_ClientSession*, unsigned usWait);

/// OmsRc API 客戶端用戶的事件處理程序.
typedef struct {
   /// 連線有關的事件: 連線失敗, 連線後斷線.
   f9OmsRc_FnOnLinkEv   FnOnLinkEv_;

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
   /// 您也可以在送單前自主呼叫 f9OmsRc_FcReqCheck() 取得流量管制需要等候的時間.
   f9OmsRc_FnOnFlowControl FnOnFlowControl_;
} f9OmsRc_ClientHandler;

/// OmsRc API 建立與主機連線時所需要的參數.
typedef struct {
   const f9OmsRc_ClientHandler*  Handler_;
   const char*                   UserId_;
   const char*                   Password_;
   const char*                   DevName_;
   const char*                   DevParams_;
   void*                         UserData_;
   f9rc_RcFlag                   RcFlags_;
   f9OmsRc_ClientLogFlag         LogFlags_;
   char                          Reserved7___[5];
   const f9omstw_ErrCodeTx*      ErrCodeTx_;
} f9OmsRc_ClientSessionParams;

//--------------------------------------------------------------------------//

/// 啟動 f9OmsRc 函式庫.
/// - 請使用時注意: 不考慮 multi thread 同時呼叫 f9OmsRc_Initialize()/f9OmsRc_Finalize();
/// - 可重覆呼叫 f9OmsRc_Initialize(), 但須有對應的 f9OmsRc_Finalize();
///   最後一個 f9OmsRc_Finalize() 被呼叫時, 結束函式庫.
/// - 啟動 Device factory.
/// - 啟動 LogFile: 
///   - if (logFileFmt == NULL) 表示不改變 log 的輸出位置, 預設 log 輸出在 console.
///   - if (*logFileFmt == '\0') 則 logFileFmt = "./logs/{0:f+'L'}/f9OmsRc.log";
///     {0:f+'L'} 表示使用 local time 的 YYYYMMDD.
/// - 若返回非 0:
///   - ENOSYS: 已經呼叫 f9OmsRc_Finalize() 系統結束.
///   - 期他為 log 檔開啟失敗的原因(errno).
f9OmsRc_API_FN(int) f9OmsRc_Initialize(const char* logFileFmt);

/// 結束 f9OmsRc 函式庫.
/// 結束前必須先將所有建立的物件刪除:
/// - f9OmsRc_CreateSession() => f9OmsRc_DestroySession();
/// - 結束系統後, 無法透過 f9OmsRc_Initialize() 重啟, 若要重啟, 必須結束程式重新執行.
f9OmsRc_API_FN(void) f9OmsRc_Finalize(void);

/// 建立一個下單連線物件, 當您不再使用時, 應呼叫 f9OmsRc_DestroySession(*result) 銷毀.
/// - 除了 params->UserData_ 可以為任意值(包含NULL), 其他指標若為 NULL, 則直接 crash!
/// - params->DevName_: 通訊設備名稱, 例如: "TcpClient";
/// - params->DevParams_: 通訊設備參數, 例如: "192.168.1.1:6601"; "dn=localhost:6601"
/// - 有可能在返回前就觸發事件, 但此時 *result 必定已經填妥已建立的 Session.
/// - retval 1 成功 *result 儲存 Session;
/// - retval 0 失敗: devName 不正確, 或尚未呼叫 f9OmsRc_Initialize();
f9OmsRc_API_FN(int)
f9OmsRc_CreateSession(f9OmsRc_ClientSession** result,
                      const f9OmsRc_ClientSessionParams* params);

/// 銷毀一個下單連線物件.
/// 在返回前, 仍然可能在其他 thread 收到事件.
/// isWait = 1 表示要等確定銷毀後才返回.
/// isWait = 0 表示在返回後仍可能收到事件, 如果您是在事件通知時呼叫, 則不能等候銷毀(會造成死結).
f9OmsRc_API_FN(void)
f9OmsRc_DestroySession(f9OmsRc_ClientSession* ses, int isWait);

inline void f9OmsRc_DestroySession_Wait(f9OmsRc_ClientSession* ses) {
   f9OmsRc_DestroySession(ses, 1);
}
inline void f9OmsRc_DestroySession_NoWait(f9OmsRc_ClientSession* ses) {
   f9OmsRc_DestroySession(ses, 0);
}

/// 用指定名稱取得「下單表單格式」.
/// - cfg 必須是在 FnOnConfig_ 事件裡面提供的那個;
/// - 這裡取得的 layout 將在下次 FnOnConfig_ 事件之後銷毀.
/// - 建議您在處理 FnOnConfig_ 事件時, 取得需要的 request layout.
/// - 可自行設定 retval->IdxUserFields_[], 不可更改 retval 的其他 members;
/// - 您也可以透過 cfg->RequestLayoutArray_[] 循序搜尋 layout.
f9OmsRc_API_FN(f9OmsRc_Layout*)
f9OmsRc_GetRequestLayout(f9OmsRc_ClientSession* ses,
                         const f9OmsRc_ClientConfig* cfg,
                         const char* reqName);

/// 用 rptTableId(從1開始) 取得「回報表單格式」, 回報表單可能會有相同名稱.
/// - cfg 必須是在 FnOnConfig_ 事件裡面提供的那個;
/// - 這裡取得的 layout 將在下次 FnOnConfig_ 事件之後銷毀.
/// - 可自行設定 retval->IdxUserFields_[], 不可更改 retval 的其他 members;
f9OmsRc_API_FN(f9OmsRc_Layout*)
f9OmsRc_GetReportLayout(f9OmsRc_ClientSession* ses,
                        const f9OmsRc_ClientConfig* cfg,
                        unsigned rptTableId);

/// 訂閱回報, ses 與 cfg 必須是 FnOnConfig_ 事件通知提供的.
f9OmsRc_API_FN(void)
f9OmsRc_SubscribeReport(f9OmsRc_ClientSession* ses,
                        const f9OmsRc_ClientConfig* cfg,
                        f9OmsRc_SNO from,
                        f9OmsRc_RptFilter filter);

/// 傳送下單要求: 已組好的字串形式.
f9OmsRc_API_FN(void)
f9OmsRc_SendRequestString(f9OmsRc_ClientSession* ses,
                          const f9OmsRc_Layout* reqLayout,
                          const fon9_CStrView reqStr);

/// 傳送下單要求: 已填妥下單欄位字串陣列: reqFieldArray[0 .. reqLayout->FieldCount_);
f9OmsRc_API_FN(void)
f9OmsRc_SendRequestFields(f9OmsRc_ClientSession* ses,
                          const f9OmsRc_Layout* reqLayout,
                          const fon9_CStrView* reqFieldArray);

/// 傳送下單要求之前, 可以自行檢查流量.
/// 傳回需要等候的 microseconds. 傳回 0 表示不需管制.
f9OmsRc_API_FN(unsigned)
f9OmsRc_FcReqCheck(f9OmsRc_ClientSession* ses);

//--------------------------------------------------------------------------//

#ifdef __cplusplus
}//extern "C"
#endif//__cplusplus
#endif//__f9omsrc_OmsRc_h__
