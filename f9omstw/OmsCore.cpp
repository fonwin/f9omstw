// \file f9omstw/OmsCore.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCore.hpp"
#include "fon9/ThreadId.hpp"

namespace f9omstw {

OmsThreadTaskHandler::OmsThreadTaskHandler(OmsThread& owner) : Owner_{static_cast<OmsCore*>(&owner)} {
   assert(static_cast<OmsCore*>(&owner)->ThreadId_ == fon9::ThreadId::IdType{});
   static_cast<OmsCore*>(&owner)->ThreadId_ = fon9::GetThisThreadId().ThreadId_;
}
void OmsThreadTaskHandler::OnThreadEnd(const std::string& threadName) {
   (void)threadName;
   this->Owner_->Backend_.WaitForEndNow();
}
bool OmsCore::IsThisThread() const {
   return this->ThreadId_ == fon9::GetThisThreadId().ThreadId_;
}
//--------------------------------------------------------------------------//
OmsCore::StartResult OmsCore::Start(fon9::TimeStamp tday, std::string logFileName) {
   this->TDay_ = tday;
   auto res = this->Backend_.OpenReload(std::move(logFileName), *this);
   if (res.IsError())
      return res;
   this->Backend_.StartThread(this->Name_ + "_Backend");
   this->Plant();
   this->StartThread(1, &this->Name_);
   return res;
}

} // namespaces
