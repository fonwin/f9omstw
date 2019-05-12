// \file f9omstw/OmsTools.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsTools_hpp__
#define __f9omstw_OmsTools_hpp__
#include "fon9/seed/Tab.hpp"

namespace f9omstw {

/// 「數字字串」 + 1;
/// 返回 true 表示成功, false = overflow;
typedef bool (*FnIncStr)(char* pbeg, char* pend);
bool IncStrDec(char* pbeg, char* pend);
bool IncStrAlpha(char* pbeg, char* pend);

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
/// - field: "TypeId Name"
void RevPrintTabFieldNames(fon9::RevBuffer& rbuf, const fon9::seed::Tab& tab, char chSplitter = *fon9_kCSTR_CELLSPL);

/// - lnHead = "R" fon9_kCSTR_CELLSPL;
///   "R|ReqName|ReqFields(TypeId Name)\n"
/// - lnHead = "O" fon9_kCSTR_CELLSPL;
///   "O|OrdName|OrdFields(TypeId Name)\n"
void RevPrintLayout(fon9::RevBuffer& rbuf, fon9::StrView lnHead, const fon9::seed::Layout& layout);

class OmsIvBase;
void RevPrintIvKey(fon9::RevBuffer& rbuf, const OmsIvBase* ivSrc, const OmsIvBase* ivEnd);

} // namespaces
#endif//__f9omstw_OmsTools_hpp__
