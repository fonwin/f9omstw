// \file f9omstw/OmsCoreByThread.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCoreByThread.hpp"

namespace f9omstw {

OmsThreadTaskHandler::OmsThreadTaskHandler(OmsThread& owner)
   : Owner_{static_cast<OmsCoreByThread*>(&owner)} {
   static_cast<OmsCoreByThread*>(&owner)->SetThisThreadId();
   this->PendingReqs_.reserve(1024 * 10);
}
void OmsThreadTaskHandler::OnThreadEnd(const std::string& threadName) {
   (void)threadName;
   this->Owner_->Backend_.WaitForEndNow();
}
//--------------------------------------------------------------------------//
void OmsThreadTaskHandler::OnAfterWakeup(OmsThread::Locker& queue) {
   if (this->Owner_->PendingReqs_.empty())
      return;
   // 使用 swap() 不釋放 PendingReqs_ 已分配的 memory.
   this->PendingReqs_.swap(this->Owner_->PendingReqs_);
   queue.unlock();
   for (OmsRequestRunner& runner : this->PendingReqs_) {
      this->Owner_->RunInCore(std::move(runner));
   }
   this->PendingReqs_.clear();
   queue.lock();
}
//--------------------------------------------------------------------------//
OmsCore::StartResult OmsCoreByThread::Start(fon9::TimeStamp tday, std::string logFileName) {
   this->PendingReqs_.reserve(1024 * 10);
   auto res = base::Start(tday, std::move(logFileName));
   if (!res.IsError())
      this->StartThread(1, &this->Name_);
   return res;
}
void OmsCoreByThread::RunCoreTask(OmsCoreTask&& task) {
   baseThread::EmplaceMessage(std::move(task));
}
bool OmsCoreByThread::MoveToCoreImpl(OmsRequestRunner&& runner) {
   auto locker{this->Lock()};
   this->CheckNotify(locker, this->PendingReqs_.empty());
   this->PendingReqs_.emplace_back(std::move(runner));
   return true;
}

} // namespaces
