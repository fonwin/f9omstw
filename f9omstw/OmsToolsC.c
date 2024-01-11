// \file f9omstw/OmsToolsC.c
// \author fonwinz@gmail.com
#include "f9omstw/OmsToolsC.h"
#include "fon9/Assert.h"

int f9omstw_IncStrDec(char* pbeg, char* pend) {
   while (pbeg < pend) {
      if (fon9_LIKELY(*--pend < '9')) {
         assert('0' <= *pend);
         ++(*pend);
         return 1;
      }
      *pend = '0';
   }
   return 0;
}
int f9omstw_IncStrAlpha(char* pbeg, char* pend) {
   while (pbeg < pend) {
      const char ch = *--pend;
      if (fon9_LIKELY(ch < '9')) {
         assert('0' <= ch);
         ++(*pend);
         return 1;
      }
      if (fon9_UNLIKELY(ch == '9')) {
         *pend = 'A';
         return 1;
      }
      if (fon9_LIKELY(ch < 'Z')) {
         assert('A' <= ch);
         ++(*pend);
         return 1;
      }
      if (fon9_UNLIKELY(ch == 'Z')) {
         *pend = 'a';
         return 1;
      }
      if (fon9_LIKELY(ch < 'z')) {
         assert('a' <= ch);
         ++(*pend);
         return 1;
      }
      *pend = '0';
   }
   return 0;
}
int f9omstw_IncStrDecUpper(char* pbeg, char* pend) {
   while (pbeg < pend) {
      const char ch = *--pend;
      if (fon9_LIKELY(ch < '9')) {
         assert('0' <= ch);
         ++(*pend);
         return 1;
      }
      if (fon9_UNLIKELY(ch == '9')) {
         *pend = 'A';
         return 1;
      }
      if (fon9_LIKELY(ch < 'Z')) {
         assert('A' <= ch);
         ++(*pend);
         return 1;
      }
      *pend = '0';
   }
   return 0;
}
