// \file f9omstw/OmsIvSymb.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsIvSymb.hpp"
#include "fon9/seed/RawWr.hpp"

namespace f9omstw {

OmsIvSymb::~OmsIvSymb() {
}
void OmsIvSymb::MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) {
   if (tab)
      FieldsCellRevPrint(tab->Fields_, fon9::seed::SimpleRawRd{*this}, rbuf);
   fon9::RevPrint(rbuf, this->SymbId_);
}
//--------------------------------------------------------------------------//
void OmsIvSymb::PodOp::BeginRead(fon9::seed::Tab& tab, fon9::seed::FnReadOp fnCallback) {
   assert(OmsIvSymbTree::IsInOmsThread(this->Sender_));
   this->BeginRW(tab, std::move(fnCallback), fon9::seed::SimpleRawRd{*this->IvSymb_});
}
void OmsIvSymb::PodOp::BeginWrite(fon9::seed::Tab& tab, fon9::seed::FnWriteOp fnCallback) {
   assert(OmsIvSymbTree::IsInOmsThread(this->Sender_));
   this->BeginRW(tab, std::move(fnCallback), fon9::seed::SimpleRawWr{*this->IvSymb_});
}
void OmsIvSymb::OnPodOp(OmsTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) {
   PodOp op{ownerTree, this, strKeyText};
   fnCallback(op, &op);
}

} // namespaces
