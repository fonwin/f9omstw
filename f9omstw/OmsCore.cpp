// \file f9omstw/OmsCore.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "fon9/ThreadId.hpp"

namespace f9omstw {

OmsCore::~OmsCore() {
   /// 在 Owner_ 死亡前, 通知 Backend 結束, 並做最後的存檔.
   /// 因為存檔時會用到 Owner_ 的 OrderFactory, RequestFactory...
   this->Backend_.OnBeforeDestroy();
}

bool OmsCore::IsThisThread() const {
   return this->ThreadId_ == fon9::GetThisThreadId().ThreadId_;
}
void OmsCore::SetThisThreadId() {
   assert(this->ThreadId_ == fon9::ThreadId::IdType{});
   this->ThreadId_ = fon9::GetThisThreadId().ThreadId_;
}
OmsCore::StartResult OmsCore::Start(fon9::TimeStamp tday, std::string logFileName, uint32_t forceTDay) {
   assert(forceTDay < fon9::kOneDaySeconds);
   this->TDay_ = fon9::TimeStampResetHHMMSS(tday) + fon9::TimeInterval_Second(forceTDay);
   auto res = this->Backend_.OpenReload(std::move(logFileName), *this);
   if (res.IsError())
      return res;
   this->Backend_.StartThread(this->Name_ + "_Backend");
   this->Plant();
   return res;
}
//--------------------------------------------------------------------------//
bool OmsCore::MoveToCore(OmsRequestRunner&& runner) {
   if (fon9_LIKELY(runner.Request_->IsReportIn() || runner.ValidateInUser()))
      return this->MoveToCoreImpl(std::move(runner));
   return false;
}
void OmsCore::RunInCore(OmsRequestRunner&& runner) {
   if (fon9_UNLIKELY(runner.Request_->IsReportIn())) {
      runner.Request_->RunReportInCore(OmsReportChecker{*this, std::move(runner)});
      return;
   }
   this->FetchRequestId(*runner.Request_);
   if (auto* step = runner.Request_->Creator().RunStep_.get()) {
      if (OmsOrderRaw* ordraw = runner.BeforeReqInCore(*this)) {
         OmsRequestRunnerInCore inCoreRunner{*this, *ordraw, std::move(runner.ExLog_), 256};
         if (ordraw->OrdNo_.empty1st() && *(ordraw->Request().OrdNo_.end() - 1) != '\0') {
            assert(runner.Request_->RxKind() == f9fmkt_RxKind_RequestNew);
            // 新單委託還沒填委託書號, 但下單要求有填委託書號.
            // => 執行下單步驟前, 應先設定委託書號對照.
            // => AllocOrdNo() 會檢查櫃號權限.
            if (!inCoreRunner.AllocOrdNo(ordraw->Request().OrdNo_))
               return;
         }
         step->RunRequest(std::move(inCoreRunner));
      }
      else { // runner.BeforeReqInCore() 失敗, 必定已經呼叫過 Request.Abandon();
         assert(runner.Request_->IsAbandoned());
      }
   }
   else { // 沒有下單要求!
      runner.RequestAbandon(this, OmsErrCode_RequestStepNotFound);
   }
}

} // namespaces
