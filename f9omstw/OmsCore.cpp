// \file f9omstw/OmsCore.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCore.hpp"
#include "fon9/ThreadId.hpp"

namespace f9omstw {

OmsThreadTaskHandler::OmsThreadTaskHandler(OmsThread& owner) {
   assert(static_cast<OmsCore*>(&owner)->ThreadId_ == fon9::ThreadId::IdType{});
   static_cast<OmsCore*>(&owner)->ThreadId_ = fon9::GetThisThreadId().ThreadId_;
}
bool OmsCore::IsThisThread() const {
   return this->ThreadId_ == fon9::GetThisThreadId().ThreadId_;
}
//--------------------------------------------------------------------------//
void OmsCore::Start() {
   this->Plant();
   this->StartThread(1, &this->Name_);
}

} // namespaces
