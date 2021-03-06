﻿// \file f9omstw/OmsBackend.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsBackend.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/ThreadTools.hpp"
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
   fon9::RevPrint(rbuf, "===== OMS end @ ", fon9::UtcNow(), " =====\n");
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
void OmsBackend::StartThread(std::string thrName, fon9::TimeInterval flushInterval) {
   this->FlushInterval_ = flushInterval;
   this->Items_.OnBeforeThreadStart(1);
   this->Thread_ = std::thread(&OmsBackend::ThrRun, this, std::move(thrName));
}
void OmsBackend::ThrRun(std::string thrName) {
   fon9_LOG_ThrRun("OmsBackend.ThrRun|name=", thrName);
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
         // quItems: 寫檔、回報...
         this->SaveQuItems(quItems);
         // Report.
         for (QuItem& qi : quItems) {
            if (qi.Item_ == nullptr)
               continue;
            if (fon9_LIKELY(qi.Item_->RxSNO() != 0)) {
               assert(qi.Item_->RxSNO() == this->PublishedSNO_ + 1);
               this->PublishedSNO_ = qi.Item_->RxSNO();
               this->ReportSubject_.Publish(core, *qi.Item_);
            }
            else { // 不加入 History(無法回補), 但需要發行訊息的回報.
               if ((qi.Item_->RxItemFlags() & OmsRequestFlag_ForcePublish) == OmsRequestFlag_ForcePublish)
                  this->ReportSubject_.Publish(core, *qi.Item_);
               intrusive_ptr_release(qi.Item_);
            }
         }
         quItems.clear();
         // ------------
         items.lock();
      }
      this->Items_.OnBeforeThreadEnd(items);
      items->Recovers_.clear(); // 清理 Recover (RecoverHandler.Consumer_) 的資源.
   }
   fon9_LOG_ThrRun("OmsBackend.ThrRun.End|name=", thrName);
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
void OmsBackend::OnAfterOrderUpdated(OmsRequestRunnerInCore& runner) {
   // 由於此時是在 core thread, 所以只要保護 core 與 backend 之間共用的物件.
   assert(runner.Resource_.Core_.IsThisThread());
   assert(runner.OrderRaw_.Order().Tail() == &runner.OrderRaw_);

   // 如果 isNeedsReqAppend == true: req 進入 core, 首次與 order 連結之後的異動.
   const bool  isNeedsReqAppend = (runner.OrderRaw_.Request().LastUpdated() == nullptr);
   // req 有可能因為需要編製 RequestId 而先呼叫了 FetchSNO(),
   // 等流程告一段落時才會來到這裡, 所以此時 req 的 SNO 必定等於 LastSNO_;
   assert(!isNeedsReqAppend || runner.OrderRaw_.Request().RxSNO() == this->LastSNO_);
   assert(runner.OrderRaw_.RxSNO() == 0);

   runner.OrderRaw_.SetRxSNO(++this->LastSNO_);
   runner.OrderRaw_.Request().SetLastUpdated(runner.OrderRaw_);
   runner.OrderRaw_.UpdateTime_ = fon9::UtcNow();
   runner.Resource_.Core_.Owner_->UpdateSc(runner);
   assert(runner.Resource_.Brks_.get() != nullptr);
   FetchScResourceIvr(runner.Resource_, runner.OrderRaw_.Order());

   Items::Locker  items{this->Items_};
   if (isNeedsReqAppend) {
      // req 首次異動, 應先將 req 加入 backend.
      assert(!runner.OrderRaw_.Request().IsAbandoned()); // 必定沒有 Abandon.
      assert(runner.OrderRaw_.Request().RxSNO() != 0);   // 必定已經編過號.
      items->Append(runner.OrderRaw_.Request(), std::move(runner.ExLogForReq_));
   }
   items->Append(runner.OrderRaw_, std::move(runner.ExLogForUpd_));
   if (!items->IsNotified_ && items->QuItems_.size() > kReserveQuItems / 2) {
      items->IsNotified_ = true;
      this->Items_.NotifyOne(items);
   }
}

} // namespaces
