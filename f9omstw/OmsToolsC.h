// \file f9omstw/OmsToolsC.h
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsToolsC_h__
#define __f9omstw_OmsToolsC_h__
#include "fon9/sys/Config.h"

#ifdef __cplusplus
extern "C" {
#endif

   /// 「數字字串」 + 1;
   /// 返回 1 表示成功, 0 = overflow;
   /// 例如: "1234" => "1235" 返回 1;  "9999" => "0000" 返回 0;
   int f9omstw_IncStrDec(char* pbeg, char* pend);

   /// 「文數字字串(0-9,A-Z,a-z)」 + 1;
   int f9omstw_IncStrAlpha(char* pbeg, char* pend);

   /// 「文數字字串(0-9,A-Z)」 + 1;
   int f9omstw_IncStrDecUpper(char* pbeg, char* pend);

#ifdef __cplusplus
}
#endif
#endif//__f9omstw_OmsToolsC_h__
