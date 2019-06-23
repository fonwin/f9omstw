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

enum f9_OmsRcOpKind  fon9_ENUM_underlying(uint8_t) {
   f9_OmsRcOpKind_Config,

   /// C <- S.
   /// 如果 server 正在建立新的 OmsCore, 則 Client 可能先收到 TDayChanged 然後才收到 Config.
   /// 此時 client 應先將 TDay 保留, 等收到 Config 時, 如果 TDay 不一致, 則送出 TDayConfirm;
   f9_OmsRcOpKind_TDayChanged = 1,
   /// C -> S.
   f9_OmsRcOpKind_TDayConfirm = 1,

   f9_OmsRcOpKind_ReportRecover,
   f9_OmsRcOpKind_ReportSubscribe,
};

struct f9_OmsRcLayout {
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
};

#ifdef __cplusplus
}//extern "C"
#endif
#endif//__f9omstw_OmsRc_h__
