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

   /// 允許使用 OmsRequestIni 建立「刪、改、查」下單要求.
   /// 通常用於「委託遺失(例如:新單尚未寫檔,但系統crash,重啟後委託遺失)」的補單操作。
   f9oms_IvRight_AllowRequestIni = 0x10,
   f9oms_IvRight_AllowTradingAll = f9oms_IvRight_AllowRequestIni,

   f9oms_IvRight_DenyAll = f9oms_IvRight_DenyTradingAll,

   /// 只有在 DenyTradingAll 的情況下才需要判斷此旗標.
   /// 因為只要允許任一種交易, 就必定允許訂閱回報.
   f9oms_IvRight_AllowReport = 0x10,

   f9oms_IvRight_IsAdmin = 0x80,
};

#ifdef __cplusplus
}//extern "C"
#endif
#endif//__f9omstw_OmsPoIvList_h__
