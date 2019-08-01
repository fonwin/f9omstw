/// \file f9omstw/OmsPoIvList.h
/// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsPoIvList_h__
#define __f9omstw_OmsPoIvList_h__
#include "fon9/sys/Config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

fon9_ENUM(f9oms_IvRight, uint8_t) {
   f9oms_IvRight_DenyTradingNew = 0x01,
   f9oms_IvRight_DenyTradingChgQty = 0x02,
   f9oms_IvRight_DenyTradingChgPri = 0x04,
   f9oms_IvRight_DenyTradingQuery = 0x08,
   f9oms_IvRight_DenyTradingAll = 0x0f,

   /// 允許使用者建立「回報」補單.
   /// 通常用於「委託遺失(例如:新單尚未寫檔,但系統crash,重啟後委託遺失)」的補單操作。
   f9oms_IvRight_AllowAddReport = 0x10,

   f9oms_IvRight_AllowAll = f9oms_IvRight_AllowAddReport,
   f9oms_IvRight_DenyAll = f9oms_IvRight_DenyTradingAll,

   /// 只有在 DenyTradingAll 的情況下才需要判斷此旗標.
   /// 因為只要允許任一種交易, 就必定允許訂閱回報.
   f9oms_IvRight_AllowSubscribeReport = 0x10,

   f9oms_IvRight_IsAdmin = 0x80,
};

#ifdef __cplusplus
}//extern "C"
#endif
#endif//__f9omstw_OmsPoIvList_h__
