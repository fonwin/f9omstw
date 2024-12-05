/// \file f9omstw/OmsCx.h
/// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCx_h__
#define __f9omstw_OmsCx_h__
#include "fon9/sys/Config.h"
#include <stdint.h>

#ifdef __cplusplus
#include "fon9/Utility.hpp"
extern "C" {
#endif

   enum f9oms_CondOp_Compare : int8_t {
      f9oms_CondOp_Compare_Unknown = -1,
      /// 等於, ==, =
      f9oms_CondOp_Compare_EQ = 0,
      /// 不等於, !=, ≠
      f9oms_CondOp_Compare_NE = 1,
      /// 小於等於, <=, ≤
      f9oms_CondOp_Compare_LEQ = 2,
      /// 大於等於, >=, ≥
      f9oms_CondOp_Compare_GEQ = 3,
      /// 小於, <
      f9oms_CondOp_Compare_Less = 4,
      /// 大於, >
      f9oms_CondOp_Compare_Grea = 5,
   };
   /// op2 必須是: "==", "!=", "<", "<=", ">", ">=";
   /// 否則傳回 f9oms_CondOp_Compare_Unknown;
   static inline f9oms_CondOp_Compare f9oms_CondOp_Compare_Parse(const char op2[2]) {
      switch (op2[0]) {
      case '!':
         if (op2[1] == '=') // "!="
            return f9oms_CondOp_Compare_NE;
         break;
      case '=':
         if (op2[1] == '=') // "=="
            return f9oms_CondOp_Compare_EQ;
         break;
      case '<':
         switch (op2[1]) {
         case '=': // "<="
            return f9oms_CondOp_Compare_LEQ;
         case '\0': // "<"
            return f9oms_CondOp_Compare_Less;
         }
         break;
      case '>':
         switch (op2[1]) {
         case '=': // ">="
            return f9oms_CondOp_Compare_GEQ;
         case '\0': // ">"
            return f9oms_CondOp_Compare_Grea;
         }
         break;
      }
      return f9oms_CondOp_Compare_Unknown;
   }

#ifdef __cplusplus
}//extern "C"
#endif
#endif//__f9omstw_OmsCx_h__
