// \file f9omstw/OmsCore.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCore_hpp__
#define __f9omstw_OmsCore_hpp__
#include "f9omstw/OmsResource.hpp"
#include "f9omstw/OmsEventFactory.hpp"

namespace f9omstw {

enum class OmsCoreSt {
   /// 剛建立 OmsCore, 變成 CurrentCore 之前.
   Loading,
   /// 啟動成功.
   CurrentCoreReady,
   /// 啟動失敗.
   BadCore,
   /// 解構中.
   Disposing,
};

using OmsCoreTask = std::function<void(OmsResource&)>;

fon9_WARN_DISABLE_PADDING;
/// - 管理 OMS 所需的資源.
/// - 不同市場應建立各自的 OmsCore, 例如: 台灣證券的OmsCore, 台灣期權的OmsCore;
/// - 不同交易日應建立各自的 OmsCore.
/// - 由衍生者自行初始化、啟動及結束:
///   - 建立 this->Symbs_;
///   - 建立 this->Brks_; 初始化 this->Brks_->Initialize();
///   - 將表格管理加入 Sapling, 啟動 Thread: this->Start();
///   - 解構前: this->WaitForEndNow();
class OmsCore : protected OmsResource {
   fon9_NON_COPY_NON_MOVE(OmsCore);

   friend class OmsCoreMgr;// for: SetCoreSt();
   OmsCoreSt   CoreSt_{OmsCoreSt::Loading};
   void SetCoreSt(OmsCoreSt st) {
      this->CoreSt_ = st;
   }

protected:
   uintptr_t   ThreadId_{};
   void SetThisThreadId();

   using StartResult = OmsBackend::OpenResult;
   /// - 將 this->Symbs_; this->Brks_; 加入 this->Sapling.
   /// - 啟動 thread.
   virtual StartResult Start(std::string logFileName);

   /// 執行 runner.ValidateInUser();  成功之後,
   /// 由衍生者實作將 runner 移到 core 執行.
   /// - 可能用一個大鎖, 鎖定保護之後, 直接呼叫執行 RunInCore();
   /// - 也可能用 MessageQueue 丟到另一個 thread 執行 RunInCore();
   virtual bool MoveToCoreImpl(OmsRequestRunner&& runner) = 0;
   /// 預設使用 this->RunCoreTask() 呼叫 this->EventInCore();
   virtual void EventToCoreImpl(OmsEventSP&& omsEvent);

   /// - 回報, runner.Request_->IsReportIn():
   ///   runner.Request_->RunReportInCore(OmsReportChecker{std::move(runner)}, *this);
   /// - 下單要求,  先執行一些前置作業後, 透過 runner.Request_->Creator_->RunStep_ 執行下單步驟.
   ///   - this->FetchRequestId(*runner.Request_);
   ///   - runner.BeforeReqInCore(*this);
   ///   - 若 runner.OrderRaw_ 還沒編委託書號, 但 runner.OrderRaw_.Request_ 有填完整委託書號,
   ///     則在 step->RunRequest() 之前, 會先建立委託書號對照.
   ///   - step->RunRequest(OmsRequestRunnerInCore{...});
   void RunInCore(OmsRequestRunner&& runner);
   /// - 處發 this->Owner_->OnEventInCore();
   /// - 呼叫 this->Backend_.Append();
   void EventInCore(OmsEventSP&& omsEvent);

public:
   const OmsCoreMgrSP   Owner_;
   const std::string    SeedPath_;

   fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list
   /// - 如果需要重新啟動 TDay core:
   ///   則 forceTDay 必須大於上一個 TDay core 的 forceTDay, 但小於 fon9::kOneDaySeconds;
   template <class... NamedArgsT>
   OmsCore(fon9::TimeStamp tday, uint32_t forceTDay, OmsCoreMgrSP owner, std::string seedPath, NamedArgsT&&... namedargs)
      : OmsResource(tday, forceTDay, *this, std::forward<NamedArgsT>(namedargs)...)
      , Owner_{std::move(owner)}
      , SeedPath_{std::move(seedPath)} {
   }
   fon9_MSC_WARN_POP;

   template <class... NamedArgsT>
   OmsCore(fon9::TimeStamp tday, OmsCoreMgrSP owner, std::string seedPath, NamedArgsT&&... namedargs)
      : OmsCore(tday, 0, std::move(owner), std::move(seedPath), std::forward<NamedArgsT>(namedargs)...) {
   }

   ~OmsCore();

   /// 先呼叫 runner.ValidateInUser(); 如果成功, 則將 runner 移到 core 之後執行下單步驟.
   /// \retval false 失敗, 已呼叫 runner.RequestAbandon(), 沒有將要求送到 OmsCore.
   /// \retval true  成功.
   ///            在返回前可能已在另一 thread (或 this_thread) 執行完畢, 且可能已有回報通知.
   ///            當然也有可能尚未執行, 或正在執行.
   bool MoveToCore(OmsRequestRunner&& runner);
   /// 到 core 處理 OmsEvent.
   void EventToCore(OmsEventSP&& omsEvent) {
      this->EventToCoreImpl(std::move(omsEvent));
   }

   /// \retval true   返回前, task 有可能已在 this thread 執行完畢, 或在另一個 thread 執行完畢;
   ///                也有可能還在排隊等候執行;
   /// \retval false  task 沒有機會被執行;
   virtual bool RunCoreTask(OmsCoreTask&& task) = 0;

   bool IsThisThread() const;

   OmsCoreSt CoreSt() const {
      return this->CoreSt_;
   }

   using OmsResource::TDay;
   using OmsResource::ForceTDayId;
   using OmsResource::ReportRecover;
   using OmsResource::ReportSubject;
   using OmsResource::LogAppend;
   using OmsResource::Name_;

   OmsRxSNO PublishedSNO() const {
      return this->Backend_.PublishedSNO();
   }

   inline friend void intrusive_ptr_add_ref(const OmsCore* p) {
      intrusive_ptr_add_ref(static_cast<const OmsResource*>(p));
   }
   inline friend void intrusive_ptr_release(const OmsCore* p) {
      intrusive_ptr_release(static_cast<const OmsResource*>(p));
   }
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

template <class TreeBase>
inline void OmsSapling<TreeBase>::OnTreeOp(fon9::seed::FnTreeOp fnCallback) {
   fon9::intrusive_ptr<OmsSapling> pthis{this};
   this->OmsCore_.RunCoreTask([pthis, fnCallback](OmsResource&) {
      pthis->base::OnTreeOp(std::move(fnCallback));
   });
}
template <class TreeBase>
inline void OmsSapling<TreeBase>::OnParentSeedClear() {
   fon9::intrusive_ptr<OmsSapling> pthis{this};
   this->OmsCore_.RunCoreTask([pthis](OmsResource&) {
      pthis->base::OnParentSeedClear();
   });
}
template <class TreeBase>
inline bool OmsSapling<TreeBase>::IsInOmsThread(fon9::seed::Tree* tree) {
   if (OmsSapling* pthis = dynamic_cast<OmsSapling*>(tree))
      return pthis->OmsCore_.IsThisThread();
   return false;
}

} // namespaces
#endif//__f9omstw_OmsCore_hpp__
