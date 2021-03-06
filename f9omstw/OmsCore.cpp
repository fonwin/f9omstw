﻿// \file f9omstw/OmsCore.cpp
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
OmsCore::StartResult OmsCore::Start(std::string logFileName) {
   auto res = this->Backend_.OpenReload(std::move(logFileName), *this);
   if (res.IsError()) {
      this->SetCoreSt(OmsCoreSt::BadCore);
      return res;
   }
   this->Backend_.StartThread(this->Name_ + "_Backend");
   this->Plant();
   return res;
}
//--------------------------------------------------------------------------//
void OmsCore::EventToCoreImpl(OmsEventSP&& omsEvent) {
   this->RunCoreTask([omsEvent](OmsResource& res) {
      res.Core_.EventInCore(OmsEventSP{omsEvent});
   });
}
void OmsCore::EventInCore(OmsEventSP&& omsEvent) {
   fon9::RevBufferList rbuf{128};
   this->Owner_->OnEventInCore(*this, *omsEvent, rbuf);
   this->Backend_.Append(*omsEvent, std::move(rbuf));
   this->Owner_->OmsEvent_.Publish(*static_cast<OmsResource*>(this), *omsEvent, nullptr/*isReload*/);
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
         if (this->Owner_->FnSetRequestLgOut_) {
            assert(dynamic_cast<OmsRequestTrade*>(runner.Request_.get()) != nullptr);
            this->Owner_->FnSetRequestLgOut_(*this, *static_cast<OmsRequestTrade*>(runner.Request_.get()), ordraw->Order());
         }
         OmsRequestRunnerInCore inCoreRunner{*this, *ordraw, std::move(runner.ExLog_), 256};
         if (OmsIsOrdNoEmpty(ordraw->OrdNo_) && *(ordraw->Request().OrdNo_.end() - 1) != '\0') {
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
