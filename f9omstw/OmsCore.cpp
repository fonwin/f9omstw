// \file f9omstw/OmsCore.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "fon9/ThreadId.hpp"

namespace f9omstw {

OmsCore::StartResult OmsCore::Start(fon9::TimeStamp tday, std::string logFileName) {
   this->TDay_ = tday;
   auto res = this->Backend_.OpenReload(std::move(logFileName), *this);
   if (res.IsError())
      return res;
   this->Backend_.StartThread(this->Name_ + "_Backend");
   this->Plant();
   return res;
}
bool OmsCore::MoveToCore(OmsRequestRunner&& runner) {
   if (runner.ValidateInUser())
      return this->MoveToCoreImpl(std::move(runner));
   return false;
}
void OmsCore::RunInCore(OmsRequestRunner&& runner) {
   this->PutRequestId(*runner.Request_);
   if (auto* step = runner.Request_->Creator_->RunStep_.get()) {
      if (OmsOrderRaw* ordraw = runner.BeforeRunInCore(*this)) {
         OmsRequestRunnerInCore inCoreRunner{*this, *ordraw, std::move(runner.ExLog_), 256};
         if (ordraw->OrdNo_.empty1st() && *(ordraw->Request_->OrdNo_.end() - 1) != '\0') {
            // 委託還沒填委託書號, 但下單要求有填委託書號.
            // => 「新單」或「補單」.
            // => 執行下單步驟前, 應先設定委託書號對照.
            if (!inCoreRunner.AllocOrdNo(ordraw->Request_->OrdNo_))
               return;
         }
         step->RunRequest(std::move(inCoreRunner));
      }
      else {
         assert(runner.Request_->IsAbandoned());
      }
   }
   else {
      runner.RequestAbandon(this, OmsErrCode_RequestStepNotFound);
   }
}

//--------------------------------------------------------------------------//

OmsThreadTaskHandler::OmsThreadTaskHandler(OmsThread& owner)
   : Owner_{static_cast<OmsCoreByThread*>(&owner)} {
   assert(static_cast<OmsCoreByThread*>(&owner)->ThreadId_ == fon9::ThreadId::IdType{});
   static_cast<OmsCoreByThread*>(&owner)->ThreadId_ = fon9::GetThisThreadId().ThreadId_;
}
void OmsThreadTaskHandler::OnThreadEnd(const std::string& threadName) {
   (void)threadName;
   this->Owner_->Backend_.WaitForEndNow();
}
bool OmsCore::IsThisThread() const {
   return this->ThreadId_ == fon9::GetThisThreadId().ThreadId_;
}

OmsCore::StartResult OmsCoreByThread::Start(fon9::TimeStamp tday, std::string logFileName) {
   auto res = base::Start(tday, std::move(logFileName));
   if (!res.IsError())
      this->StartThread(1, &this->Name_);
   return res;
}
void OmsCoreByThread::EmplaceMessage(OmsCoreTask&& task) {
   baseThread::EmplaceMessage(std::move(task));
}
bool OmsCoreByThread::MoveToCoreImpl(OmsRequestRunner&& runner) {
   if (0); runner.RequestAbandon(nullptr, OmsErrCode_OrderNotFound, "OmsCoreByThread::MoveToCoreImpl() not impl.");
   return false;
}

} // namespaces
