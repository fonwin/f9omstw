// \file f9omstw/IvacNo.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_IvacNo_hpp__
#define __f9omstw_IvacNo_hpp__
#include "fon9/StrView.hpp"

namespace f9omstw {

#ifdef f9omstw_IVACNO_NOCHKCODE
/// 只有在有 `#define f9omstw_IVACNO_NOCHKCODE` 時,
/// - 才提供 kIvacNo_NoChkCode = true;
/// - 用來確保主程式與 library 有共同的定義.
/// - 若連結(link)失敗, 則表示用錯 library 囉!
/// - 主程式的寫法:
/// \code
///   if (fon9_UNLIKELY(!f9omstw::kIvacNo_NoChkCode)) {
///      return; // 嚴重失敗, 但只要 link 成功, 就不會發生.
///   }
/// \endcode
extern const bool kIvacNo_NoChkCode;
#else
extern const bool kIvacNo_HasChkCode;
#endif

using IvacNo = uint32_t;

enum {
   kIvacNoWidth = 7
};

/// IvacNC = 6碼序號, 不含檢查碼.
/// 台灣投資人帳號(7碼數字) = 6碼序號 + 1檢查碼.
/// 如果有 `#define f9omstw_IVACNO_NOCHKCODE` 則不考慮檢查碼, 例如: 自營帳號.
enum class IvacNC : IvacNo {
   Min = 0,
#ifdef f9omstw_IVACNO_NOCHKCODE
   Max = 9999999,
#else
   Max = 999999,
#endif
   End = Max + 1,
};

inline IvacNC IvacNoToNC(IvacNo ivacNo) {
#ifdef f9omstw_IVACNO_NOCHKCODE
   return static_cast<IvacNC>(ivacNo);
#else
   return static_cast<IvacNC>(ivacNo / 10);
#endif
}

IvacNo StrToIvacNo(fon9::StrView ivacStr, const char** pend = nullptr);

inline IvacNC StrToIvacNC(fon9::StrView ivacStr, const char** pend = nullptr) {
   return IvacNoToNC(StrToIvacNo(ivacStr, pend));
}

inline IvacNC operator+ (IvacNC a, int ofs) {
   return static_cast<IvacNC>(static_cast<IvacNo>(a) + ofs);
}
inline IvacNC operator- (IvacNC a, int ofs) {
   return static_cast<IvacNC>(static_cast<IvacNo>(a) - ofs);
}
inline int operator- (IvacNC a, IvacNC b) { // 計算 distance
   return static_cast<int>(static_cast<IvacNo>(a) - static_cast<IvacNo>(b));
}

inline IvacNC operator+= (IvacNC& a, int ofs) {
   return a = static_cast<IvacNC>(static_cast<IvacNo>(a) + ofs);
}
inline IvacNC operator-= (IvacNC& a, int ofs) {
   return a = static_cast<IvacNC>(static_cast<IvacNo>(a) - ofs);
}

inline IvacNC operator++ (IvacNC& a) { // ++a;
   return a = static_cast<IvacNC>(static_cast<IvacNo>(a) + 1);
}
inline IvacNC operator-- (IvacNC& a) { // --a;
   return a = static_cast<IvacNC>(static_cast<IvacNo>(a) - 1);
}
inline IvacNC operator++ (IvacNC& a, int) { // a++;
   IvacNC r = a;
   ++a;
   return r;
}
inline IvacNC operator-- (IvacNC& a, int) { // a--;
   IvacNC r = a;
   --a;
   return r;
}

} // namespaces

namespace fon9 {
template <class RevBuffer>
void RevPrint(RevBuffer& rbuf, f9omstw::IvacNC ivacNC) {
   RevPrint(rbuf, static_cast<f9omstw::IvacNo>(ivacNC), '-');
}
} // namespace fon9
#endif//__f9omstw_IvacNo_hpp__
