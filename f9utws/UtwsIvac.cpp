// \file f9utws/UtwsIvac.cpp
// \author fonwinz@gmail.com
#include "f9utws/UtwsIvac.hpp"
#include "f9utws/UtwsSubac.hpp"
#include "f9utws/UtwsIvSymb.hpp"
#include "f9omstw/OmsIvacTree.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

OmsSubacSP UtwsIvac::MakeSubac(fon9::StrView subacNo) {
   return new UtwsSubac(subacNo, this);
}
//--------------------------------------------------------------------------//
fon9::seed::LayoutSP UtwsIvac::MakeLayout(fon9::seed::TreeFlag treeflags) {
   using namespace fon9::seed;
   Fields   infoflds;
   infoflds.Add(fon9_MakeField(UtwsIvac, Info_.Name_,       "Name"));
   infoflds.Add(fon9_MakeField(UtwsIvac, Info_.OrderLimit_, "OrderLimit"));
   infoflds.Add(fon9_MakeField2(UtwsIvac, LgOut));
   return new LayoutN(fon9_MakeField2(UtwsIvac, IvacNo), treeflags,
      new Tab{fon9::Named{"Info"}, std::move(infoflds), UtwsSubac::MakeLayout(), TabFlag::NoSeedCommand | TabFlag::Writable | TabFlag::HasSapling},
      new Tab{fon9::Named{"Sc"}, UtwsIvSc::MakeFields(fon9_OffsetOfRawPointer(UtwsIvac, Sc_)), UtwsIvSymb::MakeLayout(), TabFlag::NoSeedCommand | TabFlag::Writable | TabFlag::HasSapling}
   );
   #define kTabIndex_Info     0
   #define kTabIndex_Sc       1
}
struct UtwsIvac::PodOp : public UtwsIvac::base::PodOp {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = UtwsIvac::base::PodOp;
public:
   using base::base;

   static OmsSubacSP SubacMaker(const fon9::StrView& subacNo, OmsIvBase* owner) {
      assert(dynamic_cast<UtwsIvac*>(owner) != nullptr);
      return new UtwsSubac(subacNo, static_cast<UtwsIvac*>(owner));
   }
   fon9::seed::TreeSP GetSapling(fon9::seed::Tab& tab) override {
      assert(OmsIvacTree::IsInOmsThread(this->Sender_));
      switch (tab.GetIndex()) {
      case kTabIndex_Info:
         return this->MakeSubacTree(tab, &SubacMaker);
      case kTabIndex_Sc:
         return this->MakeIvSymbTree(tab, &UtwsIvSymb::SymbMaker,
                                     static_cast<UtwsIvac*>(this->Ivac_)->Sc_.Symbs_);
      }
      return nullptr;
   }
};
void UtwsIvac::OnPodOp(OmsIvacTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) {
   PodOp op{ownerTree, this, strKeyText};
   fnCallback(op, &op);
}

} // namespaces
