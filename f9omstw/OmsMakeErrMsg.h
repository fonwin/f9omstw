// \file f9omstw/OmsMakeErrMsg.h
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsMakeErrMsg_h__
#define __f9omstw_OmsMakeErrMsg_h__
#include "f9omstw/OmsErrCode.h"
#include "fon9/CStrView.h"

#ifdef __cplusplus
extern "C" {
#endif

   typedef struct f9omstw_ErrCodeTx    f9omstw_ErrCodeTx;

   /// 載入錯誤代碼文字翻譯檔.
   /// - cfgfn = 設定檔名稱.
   /// - language = "en"; "zh"; ...
   ///   若指定的翻譯不存在, 則使用 "en"; 如果 "en" 也不存在, 則使用第1個設定.
   /// - 載入失敗則傳回 NULL, 透過 log 機制記錄原因.
   const f9omstw_ErrCodeTx* f9omstw_LoadOmsErrMsgTx(const char* cfgfn, const char* language);
   /// cfg = "filename:en"; => f9omstw_LoadOmsErrMsgTx("filename", "en");
   const f9omstw_ErrCodeTx* f9omstw_LoadOmsErrMsgTx1(const char* cfg);

   /// 釋放「錯誤代碼文字翻譯」資源.
   void f9omstw_FreeOmsErrMsgTx(const f9omstw_ErrCodeTx*);

   /// 建立錯誤訊息.
   /// - 如果 ec 在 txRes 沒設定, 則不改變 outbuf, 直接返回 orgmsg;
   /// - buflen 必須包含 EOS 的空間, 如果空間不足則輸出 orgmsg.
   fon9_CStrView f9omstw_MakeErrMsg(const f9omstw_ErrCodeTx* txRes,
                                    char* outbuf, unsigned buflen,
                                    OmsErrCode ec, fon9_CStrView orgmsg);

#ifdef __cplusplus
}
#endif
#endif//__f9omstw_OmsToolsC_h__
