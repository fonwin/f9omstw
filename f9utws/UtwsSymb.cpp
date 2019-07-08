// \file f9utws/UtwsSymb.cpp
// \author fonwinz@gmail.com
#include "f9utws/UtwsSymb.hpp"
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
   return new Layout1(fon9_MakeField2(UtwsSymb, SymbId), treeflags,
                      new Tab{fon9::Named{"Base"}, UtwsSymb::MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable}
   );
}

} // namespaces
