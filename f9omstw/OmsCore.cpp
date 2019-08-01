// \file f9omstw/OmsCore.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
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
   if (fon9_LIKELY(IsEnumContains(runner.Request_->RequestFlags(), OmsRequestFlag_ReportIn)
                   || runner.ValidateInUser()))
      return this->MoveToCoreImpl(std::move(runner));
   return false;
}
void OmsCore::RunInCore(OmsRequestRunner&& runner) {
   if (fon9_UNLIKELY(IsEnumContains(runner.Request_->RequestFlags(), OmsRequestFlag_ReportIn))) {
      runner.Request_->RunReportInCore(OmsReportRunner{*this, std::move(runner)});
      return;
   }
   this->FetchRequestId(*runner.Request_);
   if (auto* step = runner.Request_->Creator().RunStep_.get()) {
      if (OmsOrderRaw* ordraw = runner.BeforeRunInCore(*this)) {
         OmsRequestRunnerInCore inCoreRunner{*this, *ordraw, std::move(runner.ExLog_), 256};
         if (ordraw->OrdNo_.empty1st() && *(ordraw->Request_->OrdNo_.end() - 1) != '\0') {
            assert(runner.Request_->RxKind() == f9fmkt_RxKind_RequestNew);
            // 新單委託還沒填委託書號, 但下單要求有填委託書號.
            // => 執行下單步驟前, 應先設定委託書號對照.
            // => AllocOrdNo() 會檢查櫃號權限.
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
void OmsCoreMgr::OnMaTree_AfterClear() {
   ConstLocker locker{this->Container_};
   this->CurrentCore_.reset();
}
void OmsCoreMgr::OnMaTree_AfterAdd(Locker& treeLocker, fon9::seed::NamedSeed& seed) {
   static_assert(std::is_base_of<fon9::seed::NamedSeed, OmsResource>::value, "OmsResource is not derived from NamedSeed?");
   OmsResource* coreResource = dynamic_cast<OmsResource*>(&seed);
   if (coreResource == nullptr)
      return;
   // 如果真有必要重新啟動 TDay core, 則使用 TDay + SecondOfDay.
   // OmsCore::Start() 設定 TDay 的地方也要略為調整.
   if (this->CurrentCore_ && this->CurrentCore_->TDay() >= coreResource->TDay())
      return;
   OmsCoreSP old = std::move(this->CurrentCore_);
   this->CurrentCore_.reset(&coreResource->Core_);
   if (this->IsTDayChanging_)
      return;
   this->IsTDayChanging_ = true;
   for (;;) {
      OmsCoreSP cur = this->CurrentCore_;
      cur->SetCoreSt(OmsCoreSt::CurrentCore);
      if (old)
         old->SetCoreSt(OmsCoreSt::Disposing);
      treeLocker.unlock();

      this->TDayChangedEvent_.Publish(*cur);
      if (old)
         this->Remove(&old->Name_);

      treeLocker.lock();
      if (cur == this->CurrentCore_)
         break;
      // 在 Publish() 或 Remove(old) 期間, CurrentCore 又改變了?!
      // 即使這裡一輩子也用不到, 但也不能不考慮.
      old = std::move(cur);
   }
   this->IsTDayChanging_ = false;
}

} // namespaces
