// \file f9omstw/OmsIvac.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsIvac.hpp"
#include "f9omstw/OmsTools.hpp"

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

} // namespaces
