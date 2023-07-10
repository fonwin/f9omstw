// \file f9omstw/OmsBackend.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsBackend_hpp__
#define __f9omstw_OmsBackend_hpp__
#include "f9omstw/OmsRequestBase.hpp"
#include "fon9/buffer/RevBufferList.hpp"
#include "fon9/ThreadController.hpp"
#include "fon9/FileAppender.hpp"
#include "fon9/Subr.hpp"
#include <deque>
#include <vector>

namespace f9omstw {

using RxHistory = std::deque<const OmsRxItem*>;
using RxConsumer = std::function<void(OmsCore&, const OmsRxItem&)>;
using ReportSubject = fon9::Subject<RxConsumer>;

/// - item == nullptr 表示已回補完畢, 此時不理會返回值, 若有需要即時回報可在此時訂閱 ReportSubject.
/// - 返回下一個要回補的序號, 一般而言返回 item->RxSNO() + 1;
/// - 返回 0 表示停止回補, 不會再有回補訊息.
/// - 返回 <= item->RxSNO() 表示暫時停止, 等下一個回補週期.
using RxRecover = std::function<OmsRxSNO(OmsCore&, const OmsRxItem* item)>;

class OmsBackendTask : public OmsRxItem {
   fon9_NON_COPY_NON_MOVE(OmsBackendTask);
   using base = OmsRxItem;
public:
   /// 在 DoBackendTask() 之後, 若這裡有非空, 則會寫入 Backend log;
   fon9::RevBufferList  LogBuf_{128};

   OmsBackendTask() : base(f9fmkt_RxKind_Unknown) {
   }
   virtual void DoBackendTask(OmsBackend& sender) = 0;
};
using OmsBackendTaskSP = fon9::intrusive_ptr<OmsBackendTask>;

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

   struct RecoverHandler {
      OmsRxSNO    NextSNO_;
      RxRecover   Consumer_;
      RecoverHandler(OmsRxSNO from, RxRecover&& consumer)
         : NextSNO_{from}
         , Consumer_{std::move(consumer)} {
      }
   };
   using Recovers = std::vector<RecoverHandler>;

   fon9_WARN_DISABLE_PADDING;
   struct ItemsImpl {
      RxHistory   RxHistory_;
      QuItems     QuItems_;
      Recovers    Recovers_;
      bool        IsNotified_{false};

      ItemsImpl(RxHistory&& rhs) : RxHistory_{std::move(rhs)} {
         this->QuItems_.reserve(kReserveQuItems);
      }
      ItemsImpl(size_t reserveSize) {
         this->RxHistory_.resize(reserveSize);
         this->QuItems_.reserve(kReserveQuItems);
      }
      void AppendHistory(const OmsRxItem& item);
      void Append(const OmsRxItem& item, fon9::RevBufferList&& rbuf);
      const OmsRequestBase* GetRequest(OmsRxSNO sno) const {
         if (sno < this->RxHistory_.size())
            if (const auto* item = this->RxHistory_[sno])
               return static_cast<const OmsRequestBase*>(item->CastToRequest());
         return nullptr;
      }
   };
   fon9_WARN_POP;
   using Items = fon9::ThreadController<ItemsImpl, fon9::WaitPolicy_CV>;
   std::thread          Thread_;
   uintptr_t            ThreadId_{};
   Items                Items_;
   OmsRxSNO             LastSNO_{0};
   OmsRxSNO             PublishedSNO_{0};
   fon9::TimeInterval   FlushInterval_;
   int                  CpuAffinity_{-1};
   char                 Padding____[4];
   std::string          LogPath_;

   class RecoderFd : public fon9::AsyncFileAppender {
      fon9_NON_COPY_NON_MOVE(RecoderFd);
      using base = fon9::AsyncFileAppender;
   public:
      RecoderFd() = default;
      using base::GetStorage;
   };
   const fon9::intrusive_ptr<RecoderFd>   RecorderFd_{new RecoderFd};

   enum class RecoverResult {
      Empty,
      Erase,
      Continue,
   };
   RecoverResult CheckRecoverHandler(Items::Locker& items, size_t& recoverIndex);

   void ThrRun(std::string thrName);
   void SaveQuItems(QuItems& quItems);
   void CheckNotify(Items::Locker& items);
   struct Loader;

public:
   /// 回報會在 Backend thread 寫入完畢後通知.
   ReportSubject  ReportSubject_;

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
      : Items_{reserveSize} {
   }
   ~OmsBackend();

   using OpenResult = fon9::File::Result;
   OpenResult OpenReload(std::string logFileName, OmsResource& resource,
                         fon9::FileMode fmode = fon9::FileMode::CreatePath | fon9::FileMode::Append
                                              | fon9::FileMode::Read | fon9::FileMode::DenyWrite);
   void StartThread(std::string thrName, fon9::TimeInterval flushInterval, int cpuAffinity);
   uintptr_t ThreadId() const {
      return this->ThreadId_;
   }
   bool IsThisThread() const;
   /// 通知並等候 thread 結束.
   void WaitThreadEnd();
   /// 通知並等候 thread 結束, 然後儲存剩餘的資料.
   /// 呼叫時機: 在 OmsCore 解構, 且必須在 OmsCoreMgr 死亡之前.
   /// 因為儲存剩餘資料時需要用到 OrderFactory, RequestFactory...
   void OnBeforeDestroy();

   const OmsRxItem* GetItem(OmsRxSNO sno) const {
      Items::ConstLocker items{this->Items_};
      return(sno > this->LastSNO_ ? nullptr : items->RxHistory_[sno]);
   }
   const OmsRxItem* GetItem(OmsRxSNO sno, const Locker& items) const {
      return(sno > this->LastSNO_ ? nullptr : items->RxHistory_[sno]);
   }
   Locker Lock() {
      return Locker{this->Items_};
   }
   /// 只會在 OmsCore 保護下執行.
   OmsRxSNO FetchSNO(OmsRxItem& item) {
      return item.SetRxSNO(++this->LastSNO_);
   }
   /// 僅提供參考使用, 例如: unit test 檢查是否符合預期.
   OmsRxSNO LastSNO() const {
      return this->LastSNO_;
   }
   /// 最後回報的序號, 在 Recover 事件裡面, 可取得正確的值, 否則僅供參考.
   OmsRxSNO PublishedSNO() const {
      return this->PublishedSNO_;
   }

   /// - item.RxSNO_ 必定為 this->LastSNO_ (例: Abandon request), 或為 0 (返回前由 this 編製新的序號).
   /// - 返回前會呼叫 item.OnRxItem_AddRef(); 並在 this 解構時呼叫 item.OnRxItem_Release();
   /// - 在呼叫 Append() 之後就不應再變動 item 的內容.
   void Append(OmsRxItem& item, fon9::RevBufferList&& rbuf);

   /// - assert(item.RxSNO() == 0);
   /// - 不會將 item 加入 history. 所以不會改變 item.RxSNO();
   /// - 將 item 內容使用 ExInfo 寫入 log.
   /// - 訂閱者會收到 item, 訂閱者必須自行考慮 item.RxSNO()==0; 的情況.
   void LogAppend(OmsRxItem& item, fon9::RevBufferList&& rbuf);

   /// 將自訂內容寫入 log.
   void LogAppend(fon9::RevBufferList&& rbuf) {
      this->LogAppend(this->Lock(), std::move(rbuf));
   }
   static void LogAppend(const Locker& items, fon9::RevBufferList&& rbuf) {
      items->QuItems_.emplace_back(nullptr, std::move(rbuf));
   }
   /// 不是 thread safe, 必須在 OpenReload() 之後才能安全的使用.
   const std::string& LogPath() const {
      return this->LogPath_;
   }

   void ReportRecover(OmsRxSNO fromSNO, RxRecover&& consumer);

   /// 強制處理回報訊息 (喚醒回報處理 thread);
   void Flush() {
      auto lk = this->Items_.Lock();
      this->Flush(lk);
   }
   void Flush(Locker& items);

   /// 在 backend thread 執行 task,
   /// 放在 OmsRxItem 隊列之中, 依序執行.
   void RunTask(OmsBackendTaskSP task) {
      assert(task.get() != nullptr);
      assert(task->RxKind() == f9fmkt_RxKind_Unknown);
      assert(task->RxSNO() == 0);
      Items::Locker{this->Items_}->QuItems_.emplace_back(task.detach(), fon9::RevBufferList{0});
   }

private:
   friend class OmsRequestRunnerInCore; // ~OmsRequestRunnerInCore() 解構時呼叫 this->OnBefore_Order_EndUpdate();
   void OnBefore_Order_EndUpdate(OmsRequestRunnerInCore& runner);
   void EndAppend(Locker& items, OmsRequestRunnerInCore& runner, bool isNeedsReqAppend);
};

} // namespaces
#endif//__f9omstw_OmsBackend_hpp__
