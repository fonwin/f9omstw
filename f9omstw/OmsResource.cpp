// \file f9omstw/OmsResource.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsResource.hpp"
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {

OmsResource::UsrDefObj::~UsrDefObj() {
}

OmsResource::~OmsResource() {
}
void OmsResource::Plant() {
   this->Sapling_->AddNamedSapling(this->Symbs_, fon9::Named{"Symbs"});
   this->Sapling_->AddNamedSapling(this->Brks_, fon9::Named{"Brks"});
}

} // namespaces
