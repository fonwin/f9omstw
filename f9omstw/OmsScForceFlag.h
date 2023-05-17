/// \file f9omstw/OmsScForceFlag.h
/// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsScForceFlag_h__
#define __f9omstw_OmsScForceFlag_h__
#include "fon9/sys/Config.h"
#include <stdint.h>

#ifdef __cplusplus
#include "fon9/Utility.hpp"
extern "C" {
#endif

   /// 當使用 f9fmkt_RxKind_RequestRerun, 需要檢查某些風控項目是否有強迫權限.
   /// 實際的項目應由風控模組提供, 每個風控模組的定義可能不盡相同, 所以這裡沒定義;
   /// 風控模組應自行提供 .h 檔案, 例如:
   /// \code
   ///   #ifndef __MyScForceFlag_h__
   ///   #define __MyScForceFlag_h__
   ///   #include "f9omstw/OmsScForceFlag.h"
   ///   #ifdef __cplusplus
   ///   extern "C" {
   ///   #endif
   ///     #define f9oms_ScForceFlag_XXXX     (f9oms_ScForceFlag)(0x0001)
   ///     #define f9oms_ScForceFlag_YYYY     (f9oms_ScForceFlag)(0x0002)
   ///     #define f9oms_ScForceFlag_ZZZZ     (f9oms_ScForceFlag)(0x0004)
   ///   #ifdef __cplusplus
   ///   }//extern "C"
   ///   #endif
   ///   #endif//__MyScForceFlag_h__
   /// \endcode
   fon9_ENUM(f9oms_ScForceFlag, uint64_t) {
   };

#ifdef __cplusplus
}//extern "C"
fon9_ENABLE_ENUM_BITWISE_OP(f9oms_ScForceFlag);
#endif
#endif//__f9omstw_OmsScForceFlag_h__
