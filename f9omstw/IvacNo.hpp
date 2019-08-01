// \file f9omstw/IvacNo.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_IvacNo_hpp__
#define __f9omstw_IvacNo_hpp__
#include "fon9/StrView.hpp"

namespace f9omstw {

using IvacNo = uint32_t;

enum {
   kIvacNoWidth = 7
};

/// IvacNC = 6碼序號, 不含檢查碼.
/// 台灣投資人帳號(7碼數字) = 6碼序號 + 1檢查碼.
enum class IvacNC : IvacNo {
   Min = 0,
   Max = 999999,
   End = Max + 1,
};
inline IvacNC IvacNoToNC(IvacNo ivacNo) {
   return static_cast<IvacNC>(ivacNo / 10);
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
