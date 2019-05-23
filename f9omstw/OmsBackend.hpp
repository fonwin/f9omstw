// \file f9omstw/OmsBackend.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsBackend_hpp__
#define __f9omstw_OmsBackend_hpp__
#include "f9omstw/OmsRxItem.hpp"
#include "fon9/buffer/RevBufferList.hpp"
#include "fon9/ThreadController.hpp"
#include "fon9/File.hpp"
#include <deque>
#include <vector>

namespace f9omstw {

using RxItems = std::deque<const OmsRxItem*>;

/// - OmsCore 發生的「各類要求、委託異動、事件...」依序記錄(保留)在此.
/// - Append 之後的 OmsRxItem 就不應再有變動, 因為可能已寫入記錄檔.
///   如果還有異動, 將造成記憶體中的資料與檔案記錄的不同.
class OmsBackend {
   fon9_NON_COPY_NON_MOVE(OmsBackend);

   class Saver;
   struct QuItem {
      fon9_NON_COPYABLE(QuItem);
      const OmsRxItem*     Item_;
      fon9::RevBufferList  ExLog_;
      QuItem(const OmsRxItem* item, fon9::RevBufferList&& exLog)
         : Item_{item}
         , ExLog_{std::move(exLog)} {
      }
      QuItem(QuItem&&) = default;
      QuItem& operator=(QuItem&&) = default;
   };
   using QuItems = std::vector<QuItem>;

   fon9_WARN_DISABLE_PADDING;
   struct ItemsImpl {
      RxItems  RxItems_;
      QuItems  QuItems_;
      bool     IsNotified_{false};
      ItemsImpl(RxItems&& rhs) : RxItems_{std::move(rhs)} {
         this->QuItems_.reserve(kReserveQuItems);
      }
      ItemsImpl(size_t reserveSize) {
         this->RxItems_.resize(reserveSize);
         this->QuItems_.reserve(kReserveQuItems);
      }
      void Append(const OmsRxItem& item, fon9::RevBufferList&& rbuf);
   };
   fon9_WARN_POP;
   using Items = fon9::ThreadController<ItemsImpl, fon9::WaitPolicy_CV>;
   std::thread Thread_;
   Items       Items_;
   OmsRxSNO    LastSNO_;
   fon9::File  RecorderFd_;

   void ThrRun(std::string thrName);
   void SaveQuItems(QuItems& quItems);
   fon9::File::Result OpenLoad(std::string logFileName);

public:
   using Locker = Items::Locker;

   enum : size_t {
   #ifdef NDEBUG
      /// 建構時, 容器預留的大小.
      kReserveWhenCtor = (1024 * 1024 * 10),
      /// 當新增 Request 容量不足時, 擴充的大小.
      kReserveExpand = (1024 * 10),
   #else
      kReserveWhenCtor = (16),
      kReserveExpand = (4),
   #endif
      kReserveQuItems = (1024 * 10),
   };

   OmsBackend(size_t reserveSize = kReserveWhenCtor)
      : Items_{reserveSize}
      , LastSNO_{0} {
   }
   ~OmsBackend();

   using StartResult = fon9::File::Result;
   StartResult StartThread(std::string thrName, std::string logFileName);
   void WaitForEndNow();

   const OmsRxItem* GetItem(OmsRxSNO sno) const {
      Items::ConstLocker items{this->Items_};
      return(sno > this->LastSNO_ ? nullptr : items->RxItems_[sno]);
   }
   /// 只會在 OmsCore 保護下執行.
   OmsRxSNO FetchSNO(OmsRxItem& item) {
      assert(item.RxSNO_ == 0);
      return item.RxSNO_ = ++this->LastSNO_;
   }

   /// - item.RxSNO_ 必定為 this->LastSNO_ (例: Abandon request), 或為 0 (返回前由 this 編製新的序號).
   /// - 返回前會呼叫 item.OnRxItem_AddRef(); 並在 this 解構時呼叫 item.OnRxItem_Release();
   /// - 在呼叫 Append() 之後就不應再變動 item 的內容.
   void Append(OmsRxItem& item, fon9::RevBufferList&& rbuf);

private:
   friend class OmsRequestRunnerInCore; // ~OmsRequestRunnerInCore() 解構時呼叫 OnAfterOrderUpdated();
   void OnAfterOrderUpdated(OmsRequestRunnerInCore& runner);
};

} // namespaces
#endif//__f9omstw_OmsBackend_hpp__
