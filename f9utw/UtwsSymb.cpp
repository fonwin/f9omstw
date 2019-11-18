// \file f9utw/UtwsSymb.cpp
// \author fonwinz@gmail.com
#include "f9utw/UtwsSymb.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

static const int32_t kUtwsSymbOffset[]{
   0,
   // fon9_OffsetOf(UtwsSymb, Ref_),
};
static inline fon9::fmkt::SymbData* GetUtwsSymbData(UtwsSymb* pthis, int tabid) {
   return static_cast<size_t>(tabid) < fon9::numofele(kUtwsSymbOffset)
      ? reinterpret_cast<fon9::fmkt::SymbData*>(reinterpret_cast<char*>(pthis) + kUtwsSymbOffset[tabid])
      : nullptr;
}
fon9::fmkt::SymbData* UtwsSymb::GetSymbData(int tabid) {
   return GetUtwsSymbData(this, tabid);
}
fon9::fmkt::SymbData* UtwsSymb::FetchSymbData(int tabid) {
   return GetUtwsSymbData(this, tabid);
}
fon9::seed::LayoutSP UtwsSymb::MakeLayout(fon9::seed::TreeFlag treeflags) {
   using namespace fon9::seed;
   auto flds = UtwsSymb::MakeFields();
   flds.Add(fon9_MakeField_const(UtwsSymb, SymbId_, "ShortId"));
   flds.Add(fon9_MakeField2_const(UtwsSymb, LongId));
   return new Layout1(fon9_MakeField2(UtwsSymb, SymbId), treeflags,
                      new Tab{fon9::Named{"Base"}, std::move(flds), TabFlag::NoSapling_NoSeedCommand_Writable}
   );
}

} // namespaces
