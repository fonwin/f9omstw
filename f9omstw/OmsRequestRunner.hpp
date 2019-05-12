// \file f9omstw/OmsRequestRunner.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestRunner_hpp__
#define __f9omstw_OmsRequestRunner_hpp__
#include "f9omstw/OmsOrder.hpp"
#include "fon9/RevPrint.hpp"

namespace f9omstw {

class OmsRequestRunner {
   fon9_NON_COPYABLE(OmsRequestRunner);
public:
   OmsTradingRequestSP  Request_;
   fon9::RevBufferList  ExLog_;

   OmsRequestRunner() : ExLog_{128} {
   }
   OmsRequestRunner(fon9::StrView exlog) : ExLog_{static_cast<fon9::BufferNodeSize>(exlog.size() + 128)} {
      fon9::RevPrint(this->ExLog_, exlog, '\n');
   }

   OmsRequestRunner(OmsRequestRunner&&) = default;
   OmsRequestRunner& operator=(OmsRequestRunner&&) = default;

   /// \copydoc bool OmsTradingRequest::PreCheck(OmsRequestRunner&);
   bool PreCheck() {
      return this->Request_->PreCheck(*this);
   }
};

/// - 禁止使用 new 的方式建立 OmsRequestRunnerInCore;
///   只能用「堆疊變數」的方式, 並在解構時自動結束更新.
/// - OmsOrderRaw::OrderRaw_
///   - 已是 this->OrderRaw_.Owner_->Last() == &this->OrderRaw_;
///   - 但 this->OrderRaw_.Request_->LastUpdated_ 尚未設定成 &this->OrderRaw_;
///   - ~OmsRequestRunnerInCore() 解構時才會設定 this->OrderRaw_.Request_->LastUpdated_;
class OmsRequestRunnerInCore {
   fon9_NON_COPY_NON_MOVE(OmsRequestRunnerInCore);
public:
   OmsResource&         Resource_;
   /// 在 OmsRequestRunnerInCore 解構時, 才將 OrderRaw_ 填入 Request::LastUpdated_;
   OmsOrderRaw&         OrderRaw_;
   fon9::RevBufferList  ExLog_;

   template <class ExLogArg>
   OmsRequestRunnerInCore(OmsResource& resource, OmsOrderRaw& ordRaw, ExLogArg&& exLogArg)
      : Resource_(resource)
      , OrderRaw_(ordRaw)
      , ExLog_{std::forward<ExLogArg>(exLogArg)} {
   }

   /// 解構時結束 OmsOrderRaw 的更新.
   ~OmsRequestRunnerInCore();
};

class OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(OmsRequestRunStep);
   OmsRequestRunStepSP  NextStep_;
protected:
   void ToNextStep(OmsRequestRunnerInCore&& runner) {
      if (this->NextStep_)
         this->NextStep_->Run(std::move(runner));
   }

public:
   OmsCore& Core_;
   OmsRequestRunStep(OmsCore& core) : Core_(core) {
   }
   explicit OmsRequestRunStep(OmsRequestRunStep* prev) : Core_(prev->Core_) {
      assert(prev->NextStep_.get() == nullptr);
      prev->NextStep_.reset(this);
   }
   virtual ~OmsRequestRunStep();

   virtual void Run(OmsRequestRunnerInCore&&) = 0;
};

} // namespaces
#endif//__f9omstw_OmsRequestRunner_hpp__
