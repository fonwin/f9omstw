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
   CurrentCore,
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
   /// - 如果需要重新啟動 TDay core,
   ///   則 forceTDay 必須大於上一個 TDay core 的 forceTDay, 但小於 fon9::kOneDaySeconds;
   StartResult Start(fon9::TimeStamp tday, std::string logFileName, uint32_t forceTDay = 0);

   /// 執行 runner.ValidateInUser();  成功之後,
   /// 由衍生者實作將 runner 移到 core 執行.
   virtual bool MoveToCoreImpl(OmsRequestRunner&& runner) = 0;

   /// 先執行一些前置作業後, 透過 runner.Request_->Creator_->RunStep_ 執行下單步驟.
   /// - this->PutRequestId(*runner.Request_);
   /// - runner.BeforeRunInCore(*this);
   /// - 若 runner.OrderRaw_ 還沒編委託書號, 但 runner.OrderRaw_.Request_ 有填完整委託書號,
   ///   則在 step->RunRequest() 之前, 會先建立委託書號對照.
   /// - step->RunRequest(OmsRequestRunnerInCore{...});
   void RunInCore(f9omstw::OmsRequestRunner&& runner);

public:
   const OmsCoreMgrSP   Owner_;
   const std::string    SeedPath_;

   fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list
   template <class... NamedArgsT>
   OmsCore(OmsCoreMgrSP owner, std::string seedPath, NamedArgsT&&... namedargs)
      : OmsResource(*this, std::forward<NamedArgsT>(namedargs)...)
      , Owner_{std::move(owner)}
      , SeedPath_{std::move(seedPath)} {
   }
   fon9_MSC_WARN_POP;

   ~OmsCore();

   /// 先呼叫 runner.ValidateInUser(); 如果成功, 則將 runner 移到 core 之後執行下單步驟.
   /// \retval false 失敗, 已呼叫 runner.RequestAbandon(), 沒有將要求送到 OmsCore.
   /// \retval true  成功.
   ///            在返回前可能已在另一 thread (或 this_thread) 執行完畢, 且可能已有回報通知.
   ///            當然也有可能尚未執行, 或正在執行.
   bool MoveToCore(OmsRequestRunner&& runner);

   virtual void RunCoreTask(OmsCoreTask&& task) = 0;

   bool IsThisThread() const;

   OmsCoreSt CoreSt() const {
      return this->CoreSt_;
   }

   using OmsResource::TDay;
   using OmsResource::ReportRecover;
   using OmsResource::ReportSubject;
   using OmsResource::LogAppend;
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

//--------------------------------------------------------------------------//

using OmsTDayChangedHandler = std::function<void(OmsCore&)>;

using OmsRequestFactoryPark = OmsFactoryPark_WithKeyMaker<OmsRequestFactory, &OmsRequestBase::MakeField_RxSNO, fon9::seed::TreeFlag::Unordered>;
using OmsRequestFactoryParkSP = fon9::intrusive_ptr<OmsRequestFactoryPark>;

using OmsOrderFactoryPark = OmsFactoryPark_NoKey<OmsOrderFactory, fon9::seed::TreeFlag::Unordered>;
using OmsOrderFactoryParkSP = fon9::intrusive_ptr<OmsOrderFactoryPark>;

using OmsEventFactoryPark = OmsFactoryPark_NoKey<OmsEventFactory, fon9::seed::TreeFlag::Unordered>;
using OmsEventFactoryParkSP = fon9::intrusive_ptr<OmsEventFactoryPark>;

/// 管理 OmsCore 的生死.
/// - 每個 OmsCore 僅處理一交易日的資料, 不處理換日、清檔,
///   交易日結束時, 由 OmsCoreMgr 在適當時機刪除過時的 OmsCore.
/// - 新交易日開始時, 建立新的 OmsCore.
///   - OmsCore 建立時, 轉入隔日有效單.
///   - 匯入商品資料、投資人資料、庫存...
class OmsCoreMgr : public fon9::seed::MaTree {
   fon9_NON_COPY_NON_MOVE(OmsCoreMgr);
   using base = fon9::seed::MaTree;
   OmsCoreSP   CurrentCore_;
   bool        IsTDayChanging_{false};

protected:
   OmsOrderFactoryParkSP   OrderFactoryPark_;
   OmsRequestFactoryParkSP RequestFactoryPark_;
   OmsEventFactoryParkSP   EventFactoryPark_;

   void OnMaTree_AfterAdd(Locker&, fon9::seed::NamedSeed& seed) override;
   void OnMaTree_AfterClear() override;

public:
   using base::base;

   using TDayChangedEvent = fon9::Subject<OmsTDayChangedHandler>;
   TDayChangedEvent  TDayChangedEvent_;

   /// 取得 CurrentCore, 僅供參考.
   /// - 返回前有可能會收到 TDayChanged 事件.
   /// - 傳回 nullptr 表示目前沒有 CurrentCore.
   OmsCoreSP CurrentCore() const {
      ConstLocker locker{this->Container_};
      return this->CurrentCore_;
   }

   void SetRequestFactoryPark(OmsRequestFactoryParkSP&& facPark) {
      assert(this->RequestFactoryPark_.get() == nullptr);
      this->RequestFactoryPark_ = std::move(facPark);
   }
   void SetOrderFactoryPark(OmsOrderFactoryParkSP&& facPark) {
      assert(this->OrderFactoryPark_.get() == nullptr);
      this->OrderFactoryPark_ = std::move(facPark);
   }
   void SetEventFactoryPark(OmsEventFactoryParkSP&& facPark) {
      assert(this->EventFactoryPark_.get() == nullptr);
      this->EventFactoryPark_ = std::move(facPark);
   }
   const OmsRequestFactoryPark& RequestFactoryPark() const {
      return *this->RequestFactoryPark_;
   }
   const OmsOrderFactoryPark& OrderFactoryPark() const {
      return *this->OrderFactoryPark_;
   }
   const OmsEventFactoryPark& EventFactoryPark() const {
      return *this->EventFactoryPark_;
   }
};
fon9_WARN_POP;

} // namespaces
#endif//__f9omstw_OmsCore_hpp__
