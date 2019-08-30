// \file f9omstw/OmsCoreByThread.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCoreByThread_hpp__
#define __f9omstw_OmsCoreByThread_hpp__
#include "f9omstw/OmsCore.hpp"
#include "fon9/MessageQueue.hpp"
#include "fon9/Tools.hpp"

namespace f9omstw {

class OmsThreadTaskHandler;

fon9_WARN_DISABLE_PADDING;
class OmsCoreByThreadBase : public OmsCore {
   fon9_NON_COPY_NON_MOVE(OmsCoreByThreadBase);
   using base = OmsCore;

protected:
   friend class OmsThreadTaskHandler;
   int   CpuId_{-1};

   using PendingReqs = std::vector<OmsRequestRunner>;
   PendingReqs PendingReqs_;

   using base::base;

   virtual void OnBeforeDestroy() = 0;
   virtual void StartThrRun() = 0;

   StartResult Start(fon9::TimeStamp tday, std::string logFileName) {
      this->PendingReqs_.reserve(1024 * 10);
      return base::Start(tday, std::move(logFileName));
   }

   void OnParentTreeClear(fon9::seed::Tree& tree) override {
      base::OnParentTreeClear(tree);
      this->OnBeforeDestroy();
   }

public:
   using FnInit = std::function<void(OmsResource&)>;

   template <class... ArgsT>
   OmsCoreByThreadBase(const FnInit& fnInit, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...) {
      fnInit(*this);
   }

   /// - 載入 log: this->Start(tday, logFileName);
   /// - 設定 Title(logFileName), Description(載入失敗訊息);
   /// - 啟動 thread: this->StartThrRun();
   /// - retval: this 加入 OmsCoreMgr tree 機制.
   bool StartToCoreMgr(fon9::TimeStamp tday, std::string logFileName, int cpuId);
};
using OmsCoreByThreadBaseSP = fon9::intrusive_ptr<OmsCoreByThreadBase>;
fon9_WARN_POP;

class OmsThreadTaskHandler {
   fon9_NON_COPY_NON_MOVE(OmsThreadTaskHandler);
   using MessageType = OmsCoreTask;
   OmsCoreByThreadBase&          Owner_;
   std::vector<OmsRequestRunner> PendingReqsInThr_;
   void InitializeFromMQ();

public:
   template <class BaseMQ>
   OmsThreadTaskHandler(BaseMQ&);

   OmsThreadTaskHandler(OmsCoreByThreadBase& owner);

   void OnThreadEnd(const std::string& threadName);

   void OnMessage(OmsCoreTask& task) {
      task(this->Owner_);
   }

   template <class Locker>
   void OnAfterWakeup(Locker& queue) {
      for (;;) {
         if (this->Owner_.PendingReqs_.empty())
            return;
         // 使用 swap() 不釋放 PendingReqsInThr_ 已分配的 memory.
         this->PendingReqsInThr_.swap(this->Owner_.PendingReqs_);
         queue.unlock();
         for (OmsRequestRunner& runner : this->PendingReqsInThr_) {
            this->Owner_.RunInCore(std::move(runner));
         }
         this->PendingReqsInThr_.clear();
         queue.lock();
      }
   }
};

//--------------------------------------------------------------------------//

template <class WaitPolicyT>
class OmsCoreByThread : public OmsCoreByThreadBase,
                        public fon9::MessageQueue<OmsThreadTaskHandler,
                                                  OmsCoreTask,
                                                  std::deque<OmsCoreTask>,
                                                  WaitPolicyT> {
   fon9_NON_COPY_NON_MOVE(OmsCoreByThread);
   using base = OmsCoreByThreadBase;

protected:
   bool MoveToCoreImpl(OmsRequestRunner&& runner) override {
      auto locker{this->Lock()};
      this->CheckNotify(locker, this->PendingReqs_.empty());
      this->PendingReqs_.emplace_back(std::move(runner));
      return true;
   }

   /// this->WaitForEndNow(); 並完成剩餘的工作.
   void OnBeforeDestroy() override {
      this->WaitForEndNow();
      // 完成剩餘的工作, 避免 memory leak.
      this->ThreadId_ = 0;
      OmsThreadTaskHandler handler{*this};
      this->WaitForEndNow(handler);
   }
   void StartThrRun() override {
      this->StartThread(1, &this->Name_);
   }

public:
   using base::base;

   ~OmsCoreByThread() {
      this->OnBeforeDestroy();
   }

   void RunCoreTask(OmsCoreTask&& task) override {
      this->EmplaceMessage(std::move(task));
   }

   static OmsCoreByThreadBaseSP Creator(const FnInit& fnInit,
                                        OmsCoreMgrSP coreMgr,
                                        std::string seedName,
                                        std::string coreName) {
      return new OmsCoreByThread(fnInit,
                                 std::move(coreMgr),
                                 std::move(seedName),
                                 std::move(coreName));
   }
};
using OmsCoreByThread_CV = OmsCoreByThread<fon9::WaitPolicy_CV>;
using OmsCoreByThread_Busy = OmsCoreByThread<fon9::WaitPolicy_SpinBusy>;
typedef OmsCoreByThreadBaseSP (*FnOmsCoreByThreadCreator)(const OmsCoreByThreadBase::FnInit& fnInit,
                                                          OmsCoreMgrSP coreMgr,
                                                          std::string seedName,
                                                          std::string coreName);

inline FnOmsCoreByThreadCreator GetOmsCoreByThreadCreator(fon9::HowWait howWait) {
   if (howWait == fon9::HowWait::Busy)
      return &OmsCoreByThread_Busy::Creator;
   return &OmsCoreByThread_CV::Creator;
}

template <class BaseMQ>
OmsThreadTaskHandler::OmsThreadTaskHandler(BaseMQ& mq)
   : Owner_(*static_cast<OmsCoreByThread<typename BaseMQ::WaitPolicyType>*>(&mq)) {
   this->InitializeFromMQ();
}

} // namespaces
#endif//__f9omstw_OmsCoreByThread_hpp__
