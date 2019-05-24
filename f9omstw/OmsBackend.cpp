// \file f9omstw/OmsBackend.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsBackend.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "fon9/buffer/DcQueueList.hpp"

namespace f9omstw {

OmsBackend::~OmsBackend() {
   // 必須從先建構的往後釋放, 因為前面的 request 的可能仍會用到後面的 updated(OrderRaw);
   Locker items{this->Items_};
   this->SaveQuItems(items->QuItems_);

   fon9::RevBufferList rbuf{128};
   fon9::RevPrint(rbuf, "===== OMS end @ ", fon9::UtcNow(), " =====\n");
   fon9::DcQueueList dcq{rbuf.MoveOut()};
   this->RecorderFd_.Append(dcq);

   for (OmsRxSNO L = 0; L <= this->LastSNO_; ++L) {
      if (const OmsRxItem* item = items->RxHistory_[L])
         item->OnRxItem_Release();
   }
}
void OmsBackend::WaitForEndNow() {
   this->Items_.WaitForEndNow();
   fon9::JoinThread(this->Thread_);
}
void OmsBackend::StartThread(std::string thrName) {
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
         this->Items_.WaitFor(items, std::chrono::milliseconds(10));
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
   item.OnRxItem_AddRef();
   const auto snoCurr = item.RxSNO_;
   if (fon9_UNLIKELY(snoCurr >= this->RxHistory_.size()))
      this->RxHistory_.resize(snoCurr + kReserveExpand);
   assert(item.RxSNO_ != 0);
   assert(this->RxHistory_[item.RxSNO_] == nullptr);
   this->RxHistory_[snoCurr] = &item;
}
void OmsBackend::ItemsImpl::Append(const OmsRxItem& item, fon9::RevBufferList&& rbuf) {
   this->AppendHistory(item);
   this->QuItems_.emplace_back(&item, std::move(rbuf));
}
void OmsBackend::Append(OmsRxItem& item, fon9::RevBufferList&& rbuf) {
   assert(item.RxSNO_ == 0 || item.RxSNO_ == this->LastSNO_);
   if (item.RxSNO_ == 0)
      item.RxSNO_ = ++this->LastSNO_;
   Items::Locker{this->Items_}->Append(item, std::move(rbuf));
}
void OmsBackend::OnAfterOrderUpdated(OmsRequestRunnerInCore& runner) {
   assert(runner.Resource_.Core_.IsThisThread());
   assert(runner.OrderRaw_.Order_->Last() == &runner.OrderRaw_);

   const bool isNeedsReqAppend = (runner.OrderRaw_.Request_->LastUpdated_ == nullptr);
   assert(!isNeedsReqAppend || runner.OrderRaw_.Request_->RxSNO() == this->LastSNO_);

   assert(runner.OrderRaw_.RxSNO_ == 0);
   runner.OrderRaw_.RxSNO_ = ++this->LastSNO_;

   runner.OrderRaw_.Request_->LastUpdated_ = &runner.OrderRaw_;
   runner.OrderRaw_.UpdateTime_ = fon9::UtcNow();

   Items::Locker  items{this->Items_};
   if (isNeedsReqAppend) {
      // req 首次異動, 應先將 req 加入 backend.
      assert(runner.OrderRaw_.Request_->AbandonReason() == nullptr); // 必定沒有 request abandon.
      assert(runner.OrderRaw_.Request_->RxSNO() != 0);               // 必定已經編過號.
      items->Append(*runner.OrderRaw_.Request_, std::move(runner.ExLogForReq_));
   }
   items->Append(runner.OrderRaw_, std::move(runner.ExLogForUpd_));
   if (!items->IsNotified_ && items->QuItems_.size() > kReserveQuItems / 2) {
      items->IsNotified_ = true;
      this->Items_.NotifyOne(items);
   }
}

} // namespaces
