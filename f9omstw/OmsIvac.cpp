// \file f9omstw/OmsIvac.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsIvac.hpp"
#include "f9omstw/OmsIvacTree.hpp"
#include "fon9/seed/RawWr.hpp"

namespace f9omstw {

OmsIvac::~OmsIvac() {
}
void OmsIvac::OnParentSeedClear() {
   this->Subacs_.clear();
}
OmsSubacSP OmsIvac::RemoveSubac(fon9::StrView subacNo) {
   return ContainerRemove(this->Subacs_, subacNo);
}
OmsSubac* OmsIvac::GetSubac(fon9::StrView subacNo) const {
   auto ifind = this->Subacs_.find(subacNo);
   return (ifind == this->Subacs_.end() ? nullptr : ifind->get());
}
OmsSubac* OmsIvac::FetchSubac(fon9::StrView subacNo) {
   auto ifind = this->Subacs_.find(subacNo);
   if (ifind == this->Subacs_.end())
      ifind = this->Subacs_.insert(this->MakeSubac(subacNo)).first;
   return ifind->get();
}
void OmsIvac::MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) {
   if (tab)
      FieldsCellRevPrint(tab->Fields_, fon9::seed::SimpleRawRd{*this}, rbuf);
   fon9::RevPrint(rbuf, this->IvacNo_);
}
//--------------------------------------------------------------------------//
void OmsIvac::PodOp::BeginRead(fon9::seed::Tab& tab, fon9::seed::FnReadOp fnCallback) {
   assert(OmsIvacTree::IsInOmsThread(this->Sender_));
   this->BeginRW(tab, std::move(fnCallback), fon9::seed::SimpleRawRd{*this->Ivac_});
}
void OmsIvac::PodOp::BeginWrite(fon9::seed::Tab& tab, fon9::seed::FnWriteOp fnCallback) {
   assert(OmsIvacTree::IsInOmsThread(this->Sender_));
   this->BeginRW(tab, std::move(fnCallback), fon9::seed::SimpleRawWr{*this->Ivac_});
}
fon9::seed::TreeSP OmsIvac::PodOp::MakeSubacTree(fon9::seed::Tab& tab, FnSubacMaker fnSubacMaker) const {
   assert(OmsIvacTree::IsInOmsThread(this->Sender_));
   return new OmsSubacTree(static_cast<OmsIvacTree*>(this->Sender_)->OmsCore_, tab.SaplingLayout_,
                           this->Ivac_, this->Ivac_->Subacs_, fnSubacMaker);
}
fon9::seed::TreeSP OmsIvac::PodOp::MakeIvSymbTree(fon9::seed::Tab& tab, FnIvSymbMaker fnIvSymbMaker, OmsIvSymbs& ivSymbs) const {
   assert(OmsIvacTree::IsInOmsThread(this->Sender_));
   return new OmsIvSymbTree(static_cast<OmsIvacTree*>(this->Sender_)->OmsCore_, tab.SaplingLayout_,
                            this->Ivac_, ivSymbs, fnIvSymbMaker);
}

} // namespaces
