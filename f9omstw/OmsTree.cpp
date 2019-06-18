// \file f9omstw/OmsTree.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsTree.hpp"
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {

void OmsTree::OnTreeOp(fon9::seed::FnTreeOp fnCallback) {
   fon9::intrusive_ptr<OmsTree> pthis{this};
   this->OmsCore_.RunCoreTask([pthis, fnCallback](OmsResource&) {
      pthis->InThr_OnTreeOp(std::move(fnCallback));
   });
}
void OmsTree::InThr_OnParentSeedClear() {
}
void OmsTree::OnParentSeedClear() {
   fon9::intrusive_ptr<OmsTree> pthis{this};
   this->OmsCore_.RunCoreTask([pthis](OmsResource&) {
      pthis->InThr_OnParentSeedClear();
   });
}
bool OmsTree::IsInOmsThread(fon9::seed::Tree* tree) {
   if(auto pthis = dynamic_cast<OmsTree*>(tree))
      return pthis->OmsCore_.IsThisThread();
   return false;
}

} // namespaces
