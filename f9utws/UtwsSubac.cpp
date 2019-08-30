// \file f9utws/UtwsSubac.cpp
// \author fonwinz@gmail.com
#include "f9utws/UtwsSubac.hpp"
#include "f9utws/UtwsIvSymb.hpp"
#include "fon9/seed/PodOp.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

fon9::seed::LayoutSP UtwsSubac::MakeLayout() {
   using namespace fon9::seed;
   Fields infoflds;
   infoflds.Add(fon9_MakeField(UtwsSubac, Info_.OrderLimit_, "OrderLimit"));
   return new LayoutN(fon9_MakeField2(UtwsSubac, SubacNo), TreeFlag::AddableRemovable,
      new Tab{fon9::Named{"Info"}, std::move(infoflds), TabFlag::NoSeedCommand | TabFlag::Writable | TabFlag::NoSapling},
      new Tab{fon9::Named{"Sc"}, UtwsIvSc::MakeFields(fon9_OffsetOfRawPointer(UtwsSubac, Sc_)), UtwsIvSymb::MakeLayout(), TabFlag::NoSeedCommand | TabFlag::Writable | TabFlag::HasSapling}
   );
   #define kTabIndex_Info     0
   #define kTabIndex_Sc       1
}
struct UtwsSubac::PodOp : public UtwsSubac::base::PodOp {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = UtwsSubac::base::PodOp;
public:
   using base::base;
   fon9::seed::TreeSP GetSapling(fon9::seed::Tab& tab) override {
      if (tab.GetIndex() == kTabIndex_Sc)
         return this->MakeIvSymbTree(tab, &UtwsIvSymb::SymbMaker,
                                     static_cast<UtwsSubac*>(this->Subac_)->Sc_.Symbs_);
      return nullptr;
   }
};
void UtwsSubac::OnPodOp(OmsSubacTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) {
   PodOp op{ownerTree, this, strKeyText};
   fnCallback(op, &op);
}

} // namespaces
