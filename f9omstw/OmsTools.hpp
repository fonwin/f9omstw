// \file f9omstw/OmsTools.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsTools_hpp__
#define __f9omstw_OmsTools_hpp__
#include "f9omstw/OmsBase.hpp"
#include "f9omstw/OmsToolsC.h"
#include "fon9/seed/Tab.hpp"

namespace f9omstw {

/// 「數字字串」 + 1;
/// 返回 true 表示成功, false = overflow;
typedef int (*FnIncStr)(char* pbeg, char* pend);

/// Container = fon9::SortedVectorSet<smart_pointer>;
/// 可用 fon9::StrView keyText; 尋找元素.
template <class Container>
typename Container::value_type ContainerRemove(Container& container, fon9::StrView keyText) {
   auto ifind = container.find(keyText);
   if (ifind == container.end())
      return typename Container::value_type{};
   typename Container::value_type retval = std::move(*ifind);
   container.erase(ifind);
   return retval;
}

/// 返回 Type::MakeFields(flds); 建立的 flds;
template <class Type>
inline fon9::seed::Fields MakeFieldsT() {
   fon9::seed::Fields flds;
   Type::MakeFields(flds);
   return flds;
}

/// tab.Name_ + chSplitter + fields(使用 chSplitter 分隔);
/// - 頭尾不加上其他字元.
/// - 欄位輸出: fld->CellRevPrint(rd, nullptr, rbuf);
void RevPrintFields(fon9::RevBuffer& rbuf, const fon9::seed::Tab& tab, const fon9::seed::RawRd& rd, char chSplitter = *fon9_kCSTR_CELLSPL);

/// tab.Name_ + chSplitter + fields(使用 chSplitter 分隔);
/// - 頭尾不加上其他字元.
/// - field: "TypeId Name"
void RevPrintTabFieldNames(fon9::RevBuffer& rbuf, const fon9::seed::Tab& tab, char chSplitter = *fon9_kCSTR_CELLSPL);

/// "ReqName|ReqFields(TypeId Name)\n"
/// "OrdName|OrdFields(TypeId Name)\n"
void RevPrintLayout(fon9::RevBuffer& rbuf, const fon9::seed::Layout& layout);

/// 正規化輸出 ivSrc: "BRK-9999999-Subac";
/// - 如果 ivSrc=OmsSubac
///   - 如果 ivEnd = ivSrc->Parent_         (OmsIvac) 則輸出就只有 "Subac";
///   - 如果 ivEnd = ivSrc->Parent_->Parent (OmsBrk)  則輸出就只有 "9999999-Subac";
///   - 如果 ivEnd = nullptr                          則輸出就是   "BRK-9999999-Subac";
/// - 如果 ivSrc=OmsIvac
///   - 如果 ivEnd = ivSrc->Parent_ (OmsBrk)  則輸出就只有 "9999999";
///   - 如果 ivEnd = nullptr                  則輸出就是   "BRK-9999999";
/// - 如果 ivSrc=Brk, 則輸出是 "BRK"
void RevPrintIvKey(fon9::RevBuffer& rbuf, const OmsIvBase* ivSrc, const OmsIvBase* ivEnd);

} // namespaces
#endif//__f9omstw_OmsTools_hpp__
