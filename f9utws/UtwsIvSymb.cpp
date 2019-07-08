// \file f9utws/UtwsIvSymb.cpp
// \author fonwinz@gmail.com
#include "f9utws/UtwsIvSymb.hpp"
#include "fon9/seed/PodOp.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

static fon9::seed::Fields UtwsIvSymb_MakeFields() {
   fon9::seed::Fields   flds;
   flds.Add(fon9_MakeField2(UtwsIvSymbSc, GnQty));
   flds.Add(fon9_MakeField2(UtwsIvSymbSc, CrQty));
   flds.Add(fon9_MakeField2(UtwsIvSymbSc, CrAmt));
   flds.Add(fon9_MakeField2(UtwsIvSymbSc, DbQty));
   flds.Add(fon9_MakeField2(UtwsIvSymbSc, DbAmt));
   return flds;
}
fon9::seed::LayoutSP UtwsIvSymb::MakeLayout() {
   using namespace fon9::seed;
   return new LayoutN(fon9_MakeField2(UtwsIvSymb, SymbId), TreeFlag::AddableRemovable,
      new Tab{fon9::Named{"Bal"},    UtwsIvSymb_MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable},
      new Tab{fon9::Named{"Ord"},    UtwsIvSymb_MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable},
      new Tab{fon9::Named{"Filled"}, UtwsIvSymb_MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable}
   );
   #define kTabIndex_Bal      0
   #define kTabIndex_Ord      1
   #define kTabIndex_Filled   2
}

template <class RawBase>
struct UtwsIvSymbRaw : public RawBase {
   fon9_NON_COPY_NON_MOVE(UtwsIvSymbRaw);
   static fon9::byte* CastToRawPointer(int tabIndex, UtwsIvSymb& symb) {
      switch (tabIndex) {
      case kTabIndex_Bal:     return fon9::seed::CastToRawPointer(&symb.Bal_);
      case kTabIndex_Ord:     return fon9::seed::CastToRawPointer(&symb.Ord_);
      case kTabIndex_Filled:  return fon9::seed::CastToRawPointer(&symb.Filled_);
      }
      assert(!"UomsIvSymbRaw|err=Unknown tabIndex");
      return nullptr;
   }
   UtwsIvSymbRaw(int tabIndex, UtwsIvSymb& symb)
      : RawBase{CastToRawPointer(tabIndex, symb)} {
   }
};
using UtwsIvSymbRd = UtwsIvSymbRaw<fon9::seed::SimpleRawRd>;
using UtwsIvSymbWr = UtwsIvSymbRaw<fon9::seed::SimpleRawWr>;

void UtwsIvSymb::MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) {
   if (tab)
      FieldsCellRevPrint(tab->Fields_, UtwsIvSymbRd{tab->GetIndex(), *this}, rbuf, fon9::seed::GridViewResult::kCellSplitter);
   fon9::RevPrint(rbuf, this->SymbId_);
}

struct UtwsIvSymb::PodOp : public fon9::seed::PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = fon9::seed::PodOpDefault;
public:
   UtwsIvSymb* Symb_;
   PodOp(fon9::seed::Tree& sender, UtwsIvSymb* symb, const fon9::StrView& strKeyText)
      : base(sender, fon9::seed::OpResult::no_error, strKeyText)
      , Symb_{symb} {
   }
   void BeginRead(fon9::seed::Tab& tab, fon9::seed::FnReadOp fnCallback) override {
      assert(OmsIvSymbTree::IsInOmsThread(this->Sender_));
      this->BeginRW(tab, std::move(fnCallback), UtwsIvSymbRd{tab.GetIndex(), *this->Symb_});
   }
   void BeginWrite(fon9::seed::Tab& tab, fon9::seed::FnWriteOp fnCallback) override {
      assert(OmsIvSymbTree::IsInOmsThread(this->Sender_));
      this->BeginRW(tab, std::move(fnCallback), UtwsIvSymbWr{tab.GetIndex(), *this->Symb_});
   }
};
void UtwsIvSymb::OnPodOp(OmsTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) {
   PodOp op{ownerTree, this, strKeyText};
   fnCallback(op, &op);
}

} // namespaces
