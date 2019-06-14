// \file f9omstw/OmsBackend.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsBackend.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "fon9/buffer/DcQueueList.hpp"

namespace f9omstw {

OmsBackend::~OmsBackend() {
}
void OmsBackend::WaitForEndNow() {
   this->Items_.WaitForEndNow();
   fon9::JoinThread(this->Thread_);

   // 必須從先建構的往後釋放, 因為前面的 request 的可能仍會用到後面的 updated(OrderRaw);
   Locker items{this->Items_};
   if (this->LastSNO_ == 0 && !this->RecorderFd_.IsOpened())
      return;

   this->SaveQuItems(items->QuItems_);

   fon9::RevBufferList rbuf{128};
   fon9::RevPrint(rbuf, "===== OMS end @ ", fon9::UtcNow(), " =====\n");
   fon9::DcQueueList dcq{rbuf.MoveOut()};
   this->RecorderFd_.Append(dcq);

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

      Locker   items{this->Items_};
      while (this->Items_.GetState(items) == fon9::ThreadState::ExecutingOrWaiting) {
         this->Items_.WaitFor(items, this->FlushInterval_.ToDuration());
         if (items->QuItems_.empty())
            continue;
         auto cursz = items->RxHistory_.size();
         if (cursz < this->LastSNO_ + this->kReserveExpand)
            items->RxHistory_.resize(cursz + kReserveExpand * 2);
         items->IsNotified_ = false;
         quItems.swap(items->QuItems_);
         items.unlock();
         // ------------
         // quItems: 寫檔、回報...
         this->SaveQuItems(quItems);
         if (0);// TODO: Report to client.
         quItems.clear();
         // ------------
         items.lock();
      }
      this->Items_.OnBeforeThreadEnd(items);
   }
   fon9_LOG_ThrRun("OmsBackend.ThrRun.End|name=", thrName);
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
void OmsBackend::OnAfterOrderUpdated(OmsRequestRunnerInCore& runner) {
   assert(runner.Resource_.Core_.IsThisThread());
   assert(runner.OrderRaw_.Order_->Last() == &runner.OrderRaw_);

   const bool isNeedsReqAppend = (runner.OrderRaw_.Request_->LastUpdated_ == nullptr);
   assert(!isNeedsReqAppend || runner.OrderRaw_.Request_->RxSNO() == this->LastSNO_);

   assert(runner.OrderRaw_.RxSNO() == 0);
   runner.OrderRaw_.SetRxSNO(++this->LastSNO_);

   runner.OrderRaw_.Request_->LastUpdated_ = &runner.OrderRaw_;
   runner.OrderRaw_.UpdateTime_ = fon9::UtcNow();

   Items::Locker  items{this->Items_};
   if (isNeedsReqAppend) {
      // req 首次異動, 應先將 req 加入 backend.
      assert(!runner.OrderRaw_.Request_->IsAbandoned()); // 必定沒有 Abandon.
      assert(runner.OrderRaw_.Request_->RxSNO() != 0);   // 必定已經編過號.
      items->Append(*runner.OrderRaw_.Request_, std::move(runner.ExLogForReq_));
   }
   items->Append(runner.OrderRaw_, std::move(runner.ExLogForUpd_));
   if (!items->IsNotified_ && items->QuItems_.size() > kReserveQuItems / 2) {
      items->IsNotified_ = true;
      this->Items_.NotifyOne(items);
   }
}

} // namespaces
