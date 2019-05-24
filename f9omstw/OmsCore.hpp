// \file f9omstw/OmsCore.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCore_hpp__
#define __f9omstw_OmsCore_hpp__
#include "f9omstw/OmsResource.hpp"
#include "fon9/MessageQueue.hpp"

namespace f9omstw {

using OmsThreadTask = std::function<void()>;
struct OmsThreadTaskHandler;
using OmsThread = fon9::MessageQueue<OmsThreadTaskHandler, OmsThreadTask>;

struct OmsThreadTaskHandler {
   fon9_NON_COPY_NON_MOVE(OmsThreadTaskHandler);
   using MessageType = OmsThreadTask;
   OmsCore* const Owner_;
   OmsThreadTaskHandler(OmsThread&);

   void OnMessage(OmsThreadTask& task) {
      task();
   }
   void OnThreadEnd(const std::string& threadName);
};

//--------------------------------------------------------------------------//

/// - 管理 OMS 所需的資源.
/// - 不同市場應建立各自的 OmsCore, 例如: 台灣證券的OmsCore, 台灣期權的OmsCore;
/// - 由衍生者自行初始化、啟動及結束:
///   - 建立 this->Symbs_;
///   - 建立 this->Brks_; 初始化 this->Brks_->Initialize();
///   - 將表格管理加入 Sapling, 啟動 Thread: this->Start();
///   - 解構前: this->WaitForEndNow();
class OmsCore : public OmsThread, protected OmsResource {
   fon9_NON_COPY_NON_MOVE(OmsCore);
   using base = OmsThread;

   friend struct OmsThreadTaskHandler;
   uintptr_t   ThreadId_{};

protected:
   using StartResult = OmsBackend::OpenResult;
   /// - 將 this->Symbs_; this->Brks_; 加入 this->Sapling.
   /// - 啟動 thread.
   StartResult Start(fon9::TimeStamp tday, std::string logFileName);

public:
   fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list
   template <class... NamedArgsT>
   OmsCore(NamedArgsT&&... namedargs)
      : OmsResource(*this, std::forward<NamedArgsT>(namedargs)...) {
   }
   fon9_MSC_WARN_POP;

   bool IsThisThread() const;

   fon9::TimeStamp TDay() const {
      return this->TDay_;
   }

   inline friend void intrusive_ptr_add_ref(const OmsCore* p) {
      intrusive_ptr_add_ref(static_cast<const OmsResource*>(p));
   }
   inline friend void intrusive_ptr_release(const OmsCore* p) {
      intrusive_ptr_release(static_cast<const OmsResource*>(p));
   }
};
using OmsCoreSP = fon9::intrusive_ptr<OmsCore>;

} // namespaces
#endif//__f9omstw_OmsCore_hpp__
