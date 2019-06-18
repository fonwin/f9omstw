// \file f9omstw/OmsCoreByThread.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCoreByThread_hpp__
#define __f9omstw_OmsCoreByThread_hpp__
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {

//using OmsCoreByThread_WaitPolicy = fon9::WaitPolicy_SpinBusy;
using OmsCoreByThread_WaitPolicy = fon9::WaitPolicy_CV;

class OmsCoreByThread;
struct OmsThreadTaskHandler;
using OmsThread = fon9::MessageQueue<OmsThreadTaskHandler,
                                    OmsCoreTask,
                                    std::deque<OmsCoreTask>,
                                    OmsCoreByThread_WaitPolicy>;

struct OmsThreadTaskHandler {
   fon9_NON_COPY_NON_MOVE(OmsThreadTaskHandler);
   using MessageType = OmsCoreTask;
   OmsCoreByThread* const Owner_;
   std::vector<OmsRequestRunner> PendingReqs_;

   OmsThreadTaskHandler(OmsThread&);
   void OnMessage(OmsCoreTask& task);
   void OnThreadEnd(const std::string& threadName);
   void OnAfterWakeup(OmsThread::Locker& queue);
};

//--------------------------------------------------------------------------//

class OmsCoreByThread : public OmsThread, public OmsCore {
   fon9_NON_COPY_NON_MOVE(OmsCoreByThread);
   using base = OmsCore;
   using baseThread = OmsThread;
   using PendingReqs = std::vector<OmsRequestRunner>;
   PendingReqs PendingReqs_;

protected:
   friend struct OmsThreadTaskHandler;
   StartResult Start(fon9::TimeStamp tday, std::string logFileName);

   bool MoveToCoreImpl(OmsRequestRunner&& runner) override;

public:
   using base::base;

   void RunCoreTask(OmsCoreTask&& task) override;
};

//--------------------------------------------------------------------------//

inline void OmsThreadTaskHandler::OnMessage(OmsCoreTask& task) {
   task(*this->Owner_);
}

} // namespaces
#endif//__f9omstw_OmsCoreByThread_hpp__
