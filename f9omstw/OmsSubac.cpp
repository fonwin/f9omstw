// \file f9omstw/OmsSubac.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsSubac.hpp"
#include "fon9/seed/RawWr.hpp"

namespace f9omstw {

OmsSubac::~OmsSubac() {
}
void OmsSubac::MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) {
   if (tab)
      FieldsCellRevPrint(tab->Fields_, fon9::seed::SimpleRawRd{*this}, rbuf);
   fon9::RevPrint(rbuf, this->SubacNo_);
}
//--------------------------------------------------------------------------//
void OmsSubac::PodOp::BeginRead(fon9::seed::Tab& tab, fon9::seed::FnReadOp fnCallback) {
   assert(OmsSubacTree::IsInOmsThread(this->Sender_));
   this->BeginRW(tab, std::move(fnCallback), fon9::seed::SimpleRawRd{*this->Subac_});
}
void OmsSubac::PodOp::BeginWrite(fon9::seed::Tab& tab, fon9::seed::FnWriteOp fnCallback) {
   assert(OmsSubacTree::IsInOmsThread(this->Sender_));
   this->BeginRW(tab, std::move(fnCallback), fon9::seed::SimpleRawWr{*this->Subac_});
}
fon9::seed::TreeSP OmsSubac::PodOp::MakeIvSymbTree(fon9::seed::Tab& tab, FnIvSymbMaker fnIvSymbMaker, OmsIvSymbs& ivSymbs) const {
   assert(OmsSubacTree::IsInOmsThread(this->Sender_));
   return new OmsIvSymbTree(static_cast<OmsSubacTree*>(this->Sender_)->OmsCore_, tab.SaplingLayout_,
                            this->Subac_, ivSymbs, fnIvSymbMaker);
}

} // namespaces
