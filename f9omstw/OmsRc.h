// \file f9omstw/OmsRc.h
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRc_h__
#define __f9omstw_OmsRc_h__
#include "fon9/sys/Config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// 在 server 回覆 Config 時, 提供 table 資料時,
/// 行的第1個字元, 如果是 *fon9_kCSTR_LEAD_TABLE, 則表示一個 table 的開始.
/// ```
/// fon9_kCSTR_LEAD_TABLE + TableNamed\n
/// FieldNames\n
/// FieldValues\n    可能有 0..n 行
/// ```
#define fon9_kCSTR_LEAD_TABLE    "\x02"

enum f9OmsRc_OpKind  fon9_ENUM_underlying(uint8_t) {
   f9OmsRc_OpKind_Config,

   /// C <- S.
   /// 如果 server 正在建立新的 OmsCore, 則 Client 可能先收到 TDayChanged 然後才收到 Config.
   /// 此時 client 應先將 TDay 保留, 等收到 Config 時, 如果 TDay 不一致, 則送出 TDayConfirm;
   f9OmsRc_OpKind_TDayChanged = 1,
   /// C -> S.
   f9OmsRc_OpKind_TDayConfirm = 1,

   f9OmsRc_OpKind_ReportSubscribe,
};

enum f9OmsRc_RptFilter  fon9_ENUM_underlying(uint8_t) {
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

enum f9OmsRc_RptKind  fon9_ENUM_underlying(uint8_t) {
   f9OmsRc_RptKind_Request,
   f9OmsRc_RptKind_RequestIni,
   f9OmsRc_RptKind_RequestAbandon,
   f9OmsRc_RptKind_Order,
   f9OmsRc_RptKind_Event,
};

struct f9OmsRc_Layout {
   int   Kind_;
   int   Market_;
   int   SessionId_;
   int   BrkId_;
   int   IvacNo_;
   int   SubacNo_;
   int   IvacNoFlag_;
   int   UsrDef_;
   int   ClOrdId_;
   int   Side_;
   int   OType_;
   int   Symbol_;
   int   PriType_;
   int   Pri_;
   int   Qty_;
   int   OrdNo_;
   int   PosEff_;
   int   TIF_;
   int   IniSNO_;
   int   Pri2_;

   // 底下的欄位為回報專用.
   int   ReqUID_;
   int   CrTime_;
   int   UpdateTime_;
   int   OrdSt_;
   int   ReqSt_;
   int   UserId_;
   int   FromIp_;
   int   Src_;
   int   SesName_;
   int   IniQty_;
   int   IniOType_;
   int   IniTIF_;

   int   ChgQty_;
   int   ErrCode_;
   int   Message_;
   int   BeforeQty_;
   int   AfterQty_;
   int   LeavesQty_;
   int   LastExgTime_;
   int   LastFilledTime_;
   int   CumQty_;
   int   CumAmt_;
   int   MatchKey_;
   int   MatchTime_;
   int   MatchQty_;
   int   MatchPri_;

   // 第2腳成交.
   int   CumQty2_;
   int   CumAmt2_;
   int   MatchQty2_;
   int   MatchPri2_;
};

//--------------------------------------------------------------------------//

enum f9OmsRc_ClientLogFlag  fon9_ENUM_underlying(uint8_t) {
   f9OmsRc_ClientLogFlag_None = 0,
   f9OmsRc_ClientLogFlag_Request = 0x01,
   f9OmsRc_ClientLogFlag_Report = 0x02,
   f9OmsRc_ClientLogFlag_Config = 0x04,
   f9OmsRc_ClientLogFlag_All = 0xff,
};

struct f9OmsRc_ClientSession {
   void*                   UserData_;
   f9OmsRc_ClientLogFlag   LogFlags_;
   char                    Reserved7_[7];
};

struct f9OmsRc_ClientConfig {
   uint32_t HostId_;
   uint32_t YYYYMMDD_;
   /// 相同 TDay 的啟動次數.
   uint32_t UpdatedCount_;
};

typedef uint64_t  f9OmsRc_SNO;

struct f9OmsRc_StrView {
   const char* Begin_;
   const char* End_;
};
struct f9OmsRc_ClientReport {
   f9OmsRc_SNO             ReportSNO_;
   f9OmsRc_SNO             ReferenceSNO_;
   const f9OmsRc_Layout*   Layout_;
   const f9OmsRc_StrView*  FieldArray_;
   unsigned                FieldCount_;
   int                     Reserved_;
};

typedef void (*f9OmsRc_FnOnConfig)(f9OmsRc_ClientSession*, const f9OmsRc_ClientConfig*);
typedef void (*f9OmsRc_FnOnReport)(f9OmsRc_ClientSession*, const f9OmsRc_ClientReport*);

/// 客戶端 OmsRc API 的處理程序.
struct f9OmsRc_ClientHandler {
   /// 通知時機:
   /// - 當登入成功後, Server 回覆現在交易日, 使用者設定.
   /// - 當 Server 切換到新的交易日.
   /// - 您應該在收到此事件時「回補 & 訂閱」回報.
   f9OmsRc_FnOnConfig   FnOnConfig_;

   /// 當收到回報時通知.
   f9OmsRc_FnOnReport   FnOnReport_;
};

//--------------------------------------------------------------------------//

extern void f9OmsRc_SubscribeReport(f9OmsRc_ClientSession*, const f9OmsRc_ClientConfig* cfg,
                                    f9OmsRc_SNO from, f9OmsRc_RptFilter filter);

#ifdef __cplusplus
}//extern "C"
#endif
#endif//__f9omstw_OmsRc_h__
