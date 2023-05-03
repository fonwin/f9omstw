// \file f9omstw/OmsBackend.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsBackend.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/ThreadTools.hpp"
#include "fon9/ThreadId.hpp"
#include "fon9/Log.hpp"

namespace f9omstw {

OmsBackend::~OmsBackend() {
}
void OmsBackend::WaitThreadEnd() {
   this->Items_.WaitForEndNow();
   fon9::JoinThread(this->Thread_);
}
void OmsBackend::OnBeforeDestroy() {
   this->WaitThreadEnd();

   Locker items{this->Items_};
   if (this->LastSNO_ == 0 && !this->RecorderFd_.IsOpened())
      return;

   this->SaveQuItems(items->QuItems_);
   items->QuItems_.clear();

   fon9::RevBufferList rbuf{128};
   fon9::RevPrint(rbuf, "===== OMS end @ ", fon9::LocalNow(), " =====\n");
   fon9::DcQueueList dcq{rbuf.MoveOut()};
   this->RecorderFd_.Append(dcq);

   // 必須從先建構的往後釋放, 因為前面的 request 的可能仍會用到後面的 updated(OrderRaw);
   for (OmsRxSNO L = 0; L <= this->LastSNO_; ++L) {
      if (const OmsRxItem* item = items->RxHistory_[L])
         intrusive_ptr_release(item);
   }
   this->LastSNO_ = 0;
   this->RecorderFd_.Close();
}
bool OmsBackend::IsThisThread() const {
   return(this->ThreadId_ == fon9::GetThisThreadId().ThreadId_);
}
void OmsBackend::StartThread(std::string thrName, fon9::TimeInterval flushInterval) {
   this->FlushInterval_ = flushInterval;
   this->Items_.OnBeforeThreadStart(1);
   this->Thread_ = std::thread(&OmsBackend::ThrRun, this, std::move(thrName));
}
void OmsBackend::ThrRun(std::string thrName) {
   this->ThreadId_ = fon9::GetThisThreadId().ThreadId_;
   fon9_LOG_ThrRun("OmsBackend.ThrRun|name=", thrName, "|flushInterval=", this->FlushInterval_);
   {
      QuItems  quItems;
      quItems.reserve(kReserveQuItems);

      OmsCore& core = fon9::ContainerOf(*this, &OmsResource::Backend_).Core_;
      size_t   recoverIndex = 0;
      Locker   items{this->Items_};
      while (this->Items_.GetState(items) == fon9::ThreadState::ExecutingOrWaiting) {
         if (items->QuItems_.empty() && !items->IsNotified_) {
            switch (this->CheckRecoverHandler(items, recoverIndex)) {
            case RecoverResult::Empty:
               this->Items_.WaitFor(items, this->FlushInterval_.ToDuration());
               break;
            case RecoverResult::Erase:
               items->Recovers_.erase(items->Recovers_.begin() + fon9::signed_cast(recoverIndex));
               break;
            case RecoverResult::Continue:
               break;
            }
            assert(items.owns_lock());
            continue;
         }
         auto cursz = items->RxHistory_.size();
         if (cursz < this->LastSNO_ + this->kReserveExpand)
            items->RxHistory_.resize(cursz + kReserveExpand * 2);
         items->IsNotified_ = false;
         quItems.swap(items->QuItems_);
         items.unlock();
         // ------------
         // ----- quItems: 寫檔、回報...
         // this->SaveQuItems(quItems); // 為了加快回報速度, 所以改成: 先處理回報, 再處理存檔. 但這樣會增加資料遺失的風險.
         // ----- Report.
         for (QuItem& qi : quItems) {
            if (qi.Item_ == nullptr)
               continue;
            if (fon9_LIKELY(qi.Item_->RxSNO() != 0)) {
               assert(qi.Item_->RxSNO() == this->PublishedSNO_ + 1);
               this->PublishedSNO_ = qi.Item_->RxSNO();
               this->ReportSubject_.Publish(core, *qi.Item_);
            }
            else { // 不加入 History(無法回補), 但需要發行訊息的回報.
               if (fon9_UNLIKELY(qi.Item_->RxKind() == f9fmkt_RxKind_Unknown)) {
                  if (auto* task = const_cast<OmsBackendTask*>(dynamic_cast<const OmsBackendTask*>(qi.Item_))) {
                     task->DoBackendTask(*this);
                     qi.Item_ = nullptr; // 讓 OmsBackend::SaveQuItems() 僅記錄 log;
                     qi.ExLog_ = std::move(task->LogBuf_);
                     intrusive_ptr_release(task);
                     continue;
                  }
               }
               if ((qi.Item_->RxItemFlags() & OmsRequestFlag_ForcePublish) == OmsRequestFlag_ForcePublish)
                  this->ReportSubject_.Publish(core, *qi.Item_);
               // intrusive_ptr_release(qi.Item_); 應在 SaveQuItems() 時處理.
            }
         }
         // ----- quItems: 寫檔、回報...
         this->SaveQuItems(quItems);
         // ------------
         quItems.clear();
         items.lock();
      }
      this->Items_.OnBeforeThreadEnd(items);
      items->Recovers_.clear(); // 清理 Recover (RecoverHandler.Consumer_) 的資源.
   }
   fon9_LOG_ThrRun("OmsBackend.ThrRun.End|name=", thrName);
   this->ThreadId_ = 0;
}
OmsBackend::RecoverResult OmsBackend::CheckRecoverHandler(Items::Locker& items, size_t& recoverIndex) {
   const unsigned kMaxRecoverCount = 100; // 每次每個 RecoverHandler 最多回補 100 筆.
   if (items->Recovers_.empty())
      return RecoverResult::Empty;
   recoverIndex = (recoverIndex % items->Recovers_.size());
   auto*       handler = &items->Recovers_[recoverIndex];
   OmsRxSNO    nextSNO = handler->NextSNO_;
   RxRecover   consumer = std::move(handler->Consumer_);
   OmsCore&    core = fon9::ContainerOf(*this, &OmsResource::Backend_).Core_;
   if (nextSNO > this->PublishedSNO_) { // 回補完畢 or handler->NextSNO_ 有誤!
      consumer(core, nullptr);
      return RecoverResult::Erase;
   }
   const OmsRxItem*  rxHistory[kMaxRecoverCount];
   OmsRxSNO          count = (this->PublishedSNO_ - nextSNO) + 1;
   if (count > kMaxRecoverCount)
      count = kMaxRecoverCount;
   auto iHistory = items->RxHistory_.begin() + fon9::signed_cast(nextSNO);
   std::copy(iHistory, iHistory + fon9::signed_cast(count), rxHistory);
   items.unlock();

   for (size_t L = 0; L < count;) {
      if (nextSNO > this->PublishedSNO_)
         break;
      const OmsRxItem* item = rxHistory[L];
      if (item == nullptr) {
         ++L;
         ++nextSNO;
         continue;
      }
      if ((nextSNO = consumer(core, item)) > item->RxSNO()) {
         L += (nextSNO - item->RxSNO());
         continue;
      }
      if (nextSNO != 0)
         break;
      items.lock();
      return RecoverResult::Erase;
   }
   // 有 items.unlock(), 可能 items->Recovers_ 會被改變, 所以先前的 handler 可能已經失效.
   items.lock();

   if (nextSNO > this->PublishedSNO_) { // 回補完畢.
      consumer(core, nullptr);
      return RecoverResult::Erase;
   }
   handler = &items->Recovers_[recoverIndex++];
   handler->Consumer_ = std::move(consumer);
   handler->NextSNO_ = nextSNO;
   return RecoverResult::Continue;
}
void OmsBackend::ReportRecover(OmsRxSNO fromSNO, RxRecover&& consumer) {
   Items::Locker  items{this->Items_};
   bool           needNotify = items->Recovers_.empty();
   items->Recovers_.emplace_back(fromSNO, std::move(consumer));
   if (needNotify)
      this->Items_.NotifyAll(items);
}

//--------------------------------------------------------------------------//

void OmsBackend::ItemsImpl::AppendHistory(const OmsRxItem& item) {
   intrusive_ptr_add_ref(&item);
   const auto snoCurr = item.RxSNO();
   if (fon9_UNLIKELY(snoCurr >= this->RxHistory_.size()))
      this->RxHistory_.resize(snoCurr + kReserveExpand);
   assert(item.RxSNO() != 0);
   assert(this->RxHistory_[item.RxSNO()] == nullptr);
   this->RxHistory_[snoCurr] = &item;
}
void OmsBackend::ItemsImpl::Append(const OmsRxItem& item, fon9::RevBufferList&& rbuf) {
   this->AppendHistory(item);
   this->QuItems_.emplace_back(&item, std::move(rbuf));
}
void OmsBackend::Append(OmsRxItem& item, fon9::RevBufferList&& rbuf) {
   assert(item.RxSNO() == 0 || item.RxSNO() == this->LastSNO_);
   if (item.RxSNO() == 0)
      item.SetRxSNO(++this->LastSNO_);
   Items::Locker{this->Items_}->Append(item, std::move(rbuf));
}
void OmsBackend::LogAppend(OmsRxItem& item, fon9::RevBufferList&& rbuf) {
   assert(item.RxSNO() == 0);
   intrusive_ptr_add_ref(&item);
   Items::Locker{this->Items_}->QuItems_.emplace_back(&item, std::move(rbuf));
}
void OmsBackend::OnBefore_Order_EndUpdate(OmsRequestRunnerInCore& runner) {
   // 由於此時是在 core thread, 所以只要保護 core 與 backend 之間共用的物件.
   // !this->Thread_.joinable() = Reloading;
   auto& ordraw = runner.OrderRaw();
   assert(runner.Resource_.Core_.IsThisThread() || !this->Thread_.joinable());
   assert(ordraw.Order().Tail() == &ordraw);

   // 如果 isNeedsReqAppend == true: req 進入 core, 首次與 order 連結之後的異動.
   const bool  isNeedsReqAppend = (ordraw.Request().LastUpdated() == nullptr);
   // req 有可能因為需要編製 RequestId 而先呼叫了 FetchSNO(),
   // 等流程告一段落時才會來到這裡, 所以此時 req 的 SNO 必定等於 LastSNO_;
   assert(!isNeedsReqAppend || ordraw.Request().RxSNO() == this->LastSNO_);
   assert(ordraw.RxSNO() == 0);

   ordraw.SetRxSNO(++this->LastSNO_);
   ordraw.Request().SetLastUpdated(ordraw);
   ordraw.UpdateTime_ = fon9::UtcNow();
   if (fon9_LIKELY(runner.Resource_.Core_.CoreSt() != OmsCoreSt::Loading)) {
      runner.Resource_.Core_.Owner_->UpdateSc(runner);
   }
   else {
      // 系統重新載入中:
      // - 來自 OmsRequestBase::OnAfterBackendReload() 的異動: [Queuing, WaitingCond] 的處置.
      // - or OmsParentRequestIni::OnAfterBackendReload() 母單: 系統重啟後的異動.
      // - 在 OmsBackend::OpenReload() 最終還是會呼叫 RecalcSc(); 所以這裡不可呼叫 UpdateSc();
   }
   assert(runner.Resource_.Brks_.get() != nullptr);
   FetchScResourceIvr(runner.Resource_, ordraw.Order());
   if (fon9_LIKELY(runner.BackendLocker_ == nullptr)) {
      Locker items{this->Items_};
      this->EndAppend(items, runner, isNeedsReqAppend);
   }
   else {
      this->EndAppend(*reinterpret_cast<Locker*>(runner.BackendLocker_), runner, isNeedsReqAppend);
   }
}
void OmsBackend::EndAppend(Locker& items, OmsRequestRunnerInCore& runner, bool isNeedsReqAppend) {
   if (isNeedsReqAppend) {
      // req 首次異動, 應先將 req 加入 backend.
      assert(!runner.OrderRaw().Request().IsAbandoned()); // 必定沒有 Abandon.
      assert(runner.OrderRaw().Request().RxSNO() != 0);   // 必定已經編過號.
      items->Append(runner.OrderRaw().Request(), std::move(runner.ExLogForReq_));
   }
   items->Append(runner.OrderRaw(), std::move(runner.ExLogForUpd_));
   if (!items->IsNotified_ && items->QuItems_.size() > kReserveQuItems / 2) {
      items->IsNotified_ = true;
      this->Items_.NotifyOne(items);
   }
}
void OmsBackend::Flush(Locker& items) {
   if (!items->IsNotified_) {
      items->IsNotified_ = true;
      this->Items_.NotifyOne(items);
   }
}

} // namespaces
