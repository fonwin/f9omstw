// \file f9omstw/OmsOrder.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsOrder_hpp__
#define __f9omstw_OmsOrder_hpp__
#include "f9omstw/OmsOrderFactory.hpp"
#include "f9omstw/OmsRequestTrade.hpp"
#include "f9omstw/OmsReportFilled.hpp"
#include "f9omstw/OmsIvSymb.hpp"
#include "f9omstw/OmsSymb.hpp"

namespace f9omstw {

/// 委託書的相關風控資源.
/// - 放在 OmsOrder 裡面.
/// - 當委託有異動時, 可以快速調整風控計算, 不用再查一次表格.
struct OmsScResource {
   OmsSymbSP   Symb_;
   OmsIvBaseSP Ivr_;
   OmsIvSymbSP IvSymb_;
   /// 委託風控計算價格, 小數位數由風控計算模組決定.
   int64_t     OrdPri_{0};

   void CheckMoveFrom(OmsScResource&& rhs) {
      if (!this->Symb_)    this->Symb_   = std::move(rhs.Symb_);
      if (!this->Ivr_)     this->Ivr_    = std::move(rhs.Ivr_);
      if (!this->IvSymb_)  this->IvSymb_ = std::move(rhs.IvSymb_);
      this->OrdPri_ = rhs.OrdPri_;
   }
};

enum class OmsOrderFlag : uint8_t {
   IsNeedsOnOrderUpdated = 0x01,
   IsChildOrder = 0x02 | IsNeedsOnOrderUpdated,

   /// 此旗標, 表示 OmsOrder::BeginUpdate() 持續使用 Tail_ 進行委託異動;
   IsContinueTailUpdate = 0x04,

   /// 有此旗標: 會觸發生出子單.
   /// - 在 RequestParentNew.RunStep or OmsOrder.RerunParent() 設定。
   /// - 重啟、同步不會設定此旗標。
   IsParentRunning = 0x10,
   /// 有此旗標: 子單異動時, 同時更新母單.
   /// - 當有 IsParentRunning 時, 則必定有 IsParentEnabled;
   IsParentEnabled = 0x20,

   // 備援主機, 尚未接手母單, EraseChild() 暫時先不會移除.
   // 等母單完結後, 才會清除 this->WorkingChildRequests_;
   IsParentOrder = 0x40 | IsNeedsOnOrderUpdated,
};
fon9_ENABLE_ENUM_BITWISE_OP(OmsOrderFlag);

enum class OmsFilledFlag : uint8_t {
   /// 沒有成交; 若成交回報只是表示 [IOC、FOK、價穩...] 取消, 但沒有成交量, 則依然返回 0;
   None = 0x00,
   /// 一般單: 有成交;
   HasFilled = 0x01,
   /// 雙邊單: Bid 有成交;
   HasBidFilled = 0x11,
   /// 雙邊單: Offer 有成交;
   HasOfferFilled = 0x21,
};
fon9_ENABLE_ENUM_BITWISE_OP(OmsFilledFlag);


/// - OmsOrder 沒有擁有者, 死亡時機:
///   - 當 OmsRequestBase 死亡時
///   - 若 OmsRequestBase.LastUpdate()==OmsOrder.Tail()
///   - 則 會呼叫 OmsOrder::FreeThis()
class OmsOrder {
   fon9_NON_COPY_NON_MOVE(OmsOrder);
   OmsScResource           ScResource_;
   const OmsReportFilled*  FilledHead_{nullptr};
   const OmsReportFilled*  FilledLast_{nullptr};
   OmsOrderFactory*        Creator_{nullptr};
   const OmsOrderRaw*      Head_{nullptr};
   const OmsOrderRaw*      Tail_{nullptr};
   const OmsOrderRaw*      TailPrev_{nullptr};
   const OmsOrderRaw*      FirstPending_{nullptr};
   f9fmkt_OrderSt          LastOrderSt_{};
   OmsOrderFlag            Flags_{};
   char                    padding___[6];

   void ProcessPendingReport(const OmsRequestRunnerInCore& prevRunner);

protected:
   virtual ~OmsOrder();

   /// - 當 this->GetSymb() 時, 若 this->ScResource_.Symb_.get() == nullptr;
   /// - 則透過 this->ScResource_.Symb_ = this->FindSymb() 尋找商品.
   /// - 預設: return res.Symbs_->GetSymb(symbid);
   virtual OmsSymbSP FindSymb(OmsResource& res, const fon9::StrView& symbid);

   /// 當有設定 OmsOrderFlag::IsContinueTailUpdate 旗標時,
   /// 在 OmsRequestRunnerInCore::ForceFinish() 結束更新時, 會呼叫此處通知某次異動結束.
   /// 預設: do nothing;
   virtual void OnContinueTailEndUpdate(const OmsRequestRunnerInCore& runner);

public:
   /// 若透過類似 ObjSupplier 的機制, 無法提供建構時參數.
   /// 實際使用前再配合 InitializeByStarter() 初始化;
   OmsOrder();
   /// 若有提供 scRes, 則會將 std::move(*scRes) 用於 this->ScResource_ 的初始化.
   OmsOrderRaw* InitializeByStarter(OmsOrderFactory& creator, OmsRequestBase& starter, OmsScResource* scRes);

   /// 必須透過 FreeThis() 來刪除, 預設 delete this;
   /// 若有使用 ObjectPool 則將 this 還給 ObjectPool;
   virtual void FreeThis();

   OmsOrderFactory& Creator() const {
      assert(this->Creator_ != nullptr);
      return *this->Creator_;
   }

   /// 初始此筆委託的:「新單要求」或「回報」.
   /// - 若尚未收到新單回報, 而先收到「刪改查成交」, 會先建立 Order,
   ///   此時 Initiator() 會是 nullptr.
   /// - 等收到新單回報時才填入, 此時也會調整 Head_;
   const OmsRequestIni* Initiator() const;

   /// - 若新單回報比其他回報晚收到
   ///   則此時 Order().Head() 會改變, 所以 Order().Head()->UpdSeq() 不一定是 0;
   const OmsOrderRaw*   Head()        const  { return this->Head_; }
   const OmsOrderRaw*   Tail()        const  { return this->Tail_; }
   const OmsOrderRaw*   TailPrev()    const  { return this->TailPrev_; }
   f9fmkt_OrderSt       LastOrderSt() const  { return this->LastOrderSt_; }
   bool              IsWorkingOrder() const;
   /// \ref OmsOrderRaw::HasFilled();
   OmsFilledFlag HasFilled() const;

   /// 檢查是否有尚未完成的刪單要求.
   bool HasWorkingDelete() const;

   const OmsScResource& ScResource() const   { return this->ScResource_; }
   OmsScResource&       ScResource()         { return this->ScResource_; }

   OmsBrk* GetBrk(OmsResource& res) const;

   OmsSymb* GetSymb(OmsResource& res, const fon9::StrView& symbid) {
      if (OmsSymb* retval = this->ScResource_.Symb_.get())
         return retval;
      return (this->ScResource_.Symb_ = this->FindSymb(res, symbid)).get();
   }
   template <class SymbId>
   OmsSymb* GetSymb(OmsResource& res, const SymbId& symbid) {
      if (OmsSymb* retval = this->ScResource_.Symb_.get())
         return retval;
      return (this->ScResource_.Symb_ = this->FindSymb(res, ToStrView(symbid))).get();
   }

   /// 透過 this->Creator_->MakeOrderRawImpl() 建立委託異動資料.
   /// - 通常配合 OmsRequestRunnerInCore 建立 runner; 然後執行下單(或回報)步驟.
   /// - 或是 Backend.Reload 時.
   OmsOrderRaw* BeginUpdate(const OmsRequestBase& req);
   /// 呼叫通常來自 Backend 或 this->RunnerEndUpdate();
   /// last 必定是 this->BeginUpdate() 的返回值.
   /// \retval false 亂序回報(last.UpdateOrderSt_ == f9fmkt_OrderSt_ReportPending),
   ///               不更新  this->LastOrderSt_; 可能設定 this->FirstPending_;
   /// \retval true  依序回報(last.UpdateOrderSt_ != f9fmkt_OrderSt_ReportPending),
   ///               若 (last.UpdateOrderSt_ >= f9fmkt_OrderSt_IniAccepted) 則會更新 this->LastOrderSt_;
   bool OrderEndUpdate(const OmsOrderRaw& last);
   /// if (this->OrderEndUpdate(runner.OrderRaw())) {
   ///    若有需要: this->ProcessPendingReport(runner);
   ///    若有需要: this->OnOrderUpdated(runner);
   /// }
   void RunnerEndUpdate(const OmsRequestRunnerInCore& runner);
   /// 設定持續使用 Tail 來處理異動;
   /// - 不支援 [第一次執行] 時, 設定此旗標;
   /// - 必須在 BeginUpdate() 之後, 再次呼叫 BeginUpdate() 之前設定, 才會生效;
   /// - 必須在最後一次結束異動前取消設定;
   void SetContinueTailUpdate(bool value) {
      assert(this->Tail_ != this->Head_);
      if (value)
         this->Flags_ |= OmsOrderFlag::IsContinueTailUpdate;
      else
         this->Flags_ -= OmsOrderFlag::IsContinueTailUpdate;
   }
   bool IsContinueTailUpdate() const {
      return IsEnumContains(this->Flags_, OmsOrderFlag::IsContinueTailUpdate);
   }
   bool CheckContinueTailUpdate(const OmsRequestRunnerInCore& runner) {
      if (fon9_LIKELY(!this->IsContinueTailUpdate()))
         return false;
      this->OnContinueTailEndUpdate(runner);
      return true;
   }

   const OmsReportFilled* InsertFilled(const OmsReportFilled* currFilled) {
      return OmsReportFilled::Insert(&this->FilledHead_, &this->FilledLast_, currFilled);
   }
   const OmsReportFilled* FindFilled(OmsReportFilled::MatchKey matchKey) {
      return OmsReportFilled::Find(this->FilledHead_, this->FilledLast_, matchKey);
   }
   const OmsReportFilled* FirstFilled() const {
      return this->FilledHead_;
   }
   /// 建立一個 [刪單要求].
   /// 如果無法建立(沒有設定刪單要求Factory), 則返回 nullptr;
   /// 不考慮 this->IsWorkingOrder();
   OmsRequestTradeSP MakeDeleteRequest() const;

   // ----- 底下為 [子母單機制] 的支援 {
   /// 在 OmsOrder::InitializeByStarter() 裡面呼叫;
   /// 預設 assert(!"Not support");
   virtual void SetParentOrder(OmsOrder& parentOrder);
   /// 若 this->IsChildOrder()
   /// 則透過 this->Head()->Request_->GetParentRequest() 取得 ParentOrder;
   OmsOrder* GetParentOrder() const;
   /// 在 OmsRequestBase::OnOrderUpdated():
   /// 若沒有 ParentRequest, 但有 ParentOrder, 則呼叫 ParentOrder.OnChildOrderUpdated();
   /// 只有 [成交回報 or 手動刪改子單回報] 才會來到這裡.
   /// 預設 do nothing;
   virtual void OnChildOrderUpdated(const OmsRequestRunnerInCore& childRunner);
   /// 重新執行 Parent, 預設 do nothing, 由 OmsParentOrder::RerunParent() 處理.
   virtual bool RerunParent(OmsResource& resource);
   /// 設定 OmsOrderFlag::IsParentRunning 旗標.
   /// SetParentRunning() 只能執行一次.
   void SetParentRunning() {
      assert(!IsEnumContains(this->Flags_, OmsOrderFlag::IsParentRunning));
      this->Flags_ |= OmsOrderFlag::IsParentRunning | OmsOrderFlag::IsParentEnabled;
   }
   /// 停止繼續生出子單, 但仍會持續與已有的子單連動.
   /// 通常在母單執行完畢時呼叫 SetParentStopRun();
   /// 但後續仍可能會有子單異動後的更新.
   void SetParentStopRun() {
      this->Flags_ -= OmsOrderFlag::IsParentRunning;
   }
   bool IsParentRunning() const {
      return IsEnumContains(this->Flags_,OmsOrderFlag::IsParentRunning);
   }
   /// 設定母單與現有子單連動更新.
   /// 底下2種情況會設定 IsParentEnabled 旗標, 但不會有 SetParentDisabled() 的需求:
   /// - 在 RequestParentNew.RunStep or OmsOrder.RerunParent() 呼叫 SetParentRunning();
   /// - 主機重啟後會在 OmsParentRequestIni::OnAfterBackendReload() 重新設定 [本機母單] 的狀態.
   /// - 一旦設定 IsParentRunning 或 IsParentEnabled 之後, 就會取得母單的所有權, 直到主機死亡.
   /// - 擁有母單的主機: 負責更新母單的狀態. 若同時有 IsParentRunning 則需負責建立子單.
   void SetParentEnabled() {
      this->Flags_ |= OmsOrderFlag::IsParentEnabled;
   }
   // void SetParentDisabled() {
   //    this->Flags_ -= OmsOrderFlag::IsParentEnabled;
   // }
   bool IsParentEnabled() const {
      return IsEnumContains(this->Flags_, OmsOrderFlag::IsParentEnabled);
   }

   bool IsParentOrder() const {
      return IsEnumContains(this->Flags_, OmsOrderFlag::IsParentOrder);
   }
   bool IsChildOrder() const {
      return IsEnumContains(this->Flags_, OmsOrderFlag::IsChildOrder);
   }
protected:
   /// 設定 OmsOrderFlag::IsParentOrder 旗標.
   void SetThisIsParentOrder() {
      this->Flags_ |= OmsOrderFlag::IsParentOrder;
   }
   /// 設定 OmsOrderFlag::IsChildOrder 旗標.
   void SetThisIsChildOrder(OmsOrder& parentOrder) {
      (void)parentOrder;
      assert(!IsEnumContains(this->Flags_, OmsOrderFlag::IsChildOrder)); // SetThisIsChildOrder() 只能執行一次.
      this->Flags_ |= OmsOrderFlag::IsChildOrder;
   }
   /// 只有在有提供 OmsOrderFlag::IsNeedsOnOrderUpdated 才會觸發此事件.
   /// 預設: runner.OrderRaw().Request().OnOrderUpdated(runner);
   virtual void OnOrderUpdated(const OmsRequestRunnerInCore& runner);
   // ----- 以上為 [子母單機制] 的支援 };
};

class OmsOrderContinueUpdate {
   fon9_NON_COPY_NON_MOVE(OmsOrderContinueUpdate);
public:
   OmsOrder&   FromOrder_;
   const bool  KeepContinueTailUpdate_;
   char        Padding____[7];
   OmsOrderContinueUpdate(OmsOrder& from) : FromOrder_(from), KeepContinueTailUpdate_{from.IsContinueTailUpdate()} {
      from.SetContinueTailUpdate(true);
   }
   ~OmsOrderContinueUpdate() {
      this->FromOrder_.SetContinueTailUpdate(this->KeepContinueTailUpdate_);
   }
};

//--------------------------------------------------------------------------//

enum class OmsOrderRawFlag : uint8_t {
   IsRequestFirstUpdate = 0x01,
   IsForceInternal = 0x02,
   /// 是否有異動風控資料?
   /// 用來協助快速排除 [沒有列入風控] 計算的 ordraw;
   IsScUpdated = 0x04,
};
fon9_ENABLE_ENUM_BITWISE_OP(OmsOrderRawFlag);

/// 每次委託異動增加一個 OmsOrderRaw;
/// - 從檔案重新載入.
///   - new OmsOrderRaw(必要參數),
///   - 然後透過 OmsOrderFactory::Form_ 填入全部的欄位;
/// - OmsOrderFactory::MakeOrderRaw() 使用 ObjectPool 機制:
///   - 使用 OmsOrderRaw 預設建構, 放到 ObjectPool.
///   - 使用前再透過 OmsOrderRaw::InitializeBy*() 初始化.
class OmsOrderRaw : public OmsRxItem {
   fon9_NON_COPY_NON_MOVE(OmsOrderRaw);
   using base = OmsRxItem;

   const OmsOrderRaw mutable* Next_{nullptr};
   const OmsRequestBase*      Request_{nullptr};
   OmsOrder*                  Order_{nullptr};
   uint32_t                   UpdSeq_{0};

   /// 給 OmsOrder 的首次異動, 等同於 OmsOrderRaw(OmsOrder& order, const OmsRequestBase& req);
   /// - 設定 this->Order_; this->Request_;
   /// - 為了速度, 此處應盡可能的簡化, 必要的初值應在「預設建構」裡面處理.
   void InitializeByStarter(OmsOrder& order, const OmsRequestBase& starter);
   /// - 設定 this->Order_; this->Request_; this->UpdSeq_; ...
   /// - 呼叫 this->ContinuePrevUpdate(tail); 設定初始化內容.
   void InitializeByTail(const OmsOrderRaw& tail, const OmsRequestBase& req);
   friend class OmsOrder; // 在 OmsOrder::BeginUpdate() 時需要呼叫 InitializeByTail();

   static void MakeFieldsImpl(fon9::seed::Fields& flds);
protected:
   template <class Derived>
   static void MakeFields(fon9::seed::Fields& flds) {
      fon9_GCC_WARN_DISABLE("-Winvalid-offsetof");
      static_assert(offsetof(Derived, UpdateOrderSt_) == offsetof(OmsOrderRaw, UpdateOrderSt_),
                    "'OmsOrderRaw' must be the first base class in derived.");
      fon9_GCC_WARN_POP;

      MakeFieldsImpl(flds);
   }

   /// 若 OmsOrderFactory::MakeOrderRaw() 使用 ObjectPool 機制, 則可能先建立 OmsOrderRaw,
   /// 然後在 OmsOrder::BeginUpdate() 返回前, 再透過 OmsOrderRaw::InitializeBy*() 初始化.
   /// 在此預設建構裡面, 應先將一些欄位初始化, 讓 OmsOrderRaw::InitializeBy*() 可以快一些.
   OmsOrderRaw() : base(f9fmkt_RxKind_Order) {
   }
   virtual ~OmsOrderRaw();
   const OmsOrderRaw* CastToOrder() const override;

public:
   /// 此筆異動期望的 OrderSt, 但不一定會改變 Order::LastOrderSt();
   f9fmkt_OrderSt          UpdateOrderSt_;
   f9fmkt_TradingRequestSt RequestSt_;
   OmsErrCode              ErrCode_{};

   // -----
   // 在 64bits 系統上, OrdNo_(5 bytes) 之後會有 3 bytes padding.
   // 所以將衍生者可能用到的少量資料放在 OrdNo_ 之後, 降低記憶體的負擔.
   OmsOrdNo                OrdNo_{nullptr};
   /// 用在「報價回報」時, 若僅回報單邊, 則透過此處表明此次回報 Bid 或 Offer.
   f9fmkt_Side             QuoteReportSide_{f9fmkt_Side_Unknown};
   /// 剩餘量的改變不可再影響風控統計; 通常在收盤事件, 設定此旗標;
   /// 若之後有成交回報, 或其他刪改回報, 都不可再改變風控剩餘未成交量.
   mutable bool            IsFrozeScLeaves_{false};
   OmsOrderRawFlag         Flags_{};

   /// this->Request() 的第一次 ordraw 異動.
   bool IsRequestFirstUpdate() const {
      return IsEnumContains(this->Flags_, OmsOrderRawFlag::IsRequestFirstUpdate);
   }
   /// 不論 this->Request() 的原始來源, 強制將此次的[異動來源]設為本機.
   bool IsForceInternal() const {
      return IsEnumContains(this->Flags_, OmsOrderRawFlag::IsForceInternal);
   }
   void SetForceInternal() {
      this->Flags_ |= OmsOrderRawFlag::IsForceInternal;
   }
   // -----
   bool IsScUpdated() const {
      return IsEnumContains(this->Flags_, OmsOrderRawFlag::IsScUpdated);
   }
   void SetScUpdated() {
      this->Flags_ |= OmsOrderRawFlag::IsScUpdated;
   }
   // -----

   /// 異動時的本機時間.
   /// 在 OmsBackend::OnBefore_Order_EndUpdate() 填入當時的時間.
   fon9::TimeStamp         UpdateTime_;
   /// 若訊息長度沒有超過 fon9::CharVector::kMaxInternalSize (在x64系統, 大約 23 bytes),
   /// 則可以不用分配記憶體, 一般而言常用的訊息不會超過(例如: "Sending by BBBB-SS", "Queuing"),
   /// 通常在有錯誤時才會使用較長的訊息.
   fon9::CharVector        Message_;

   /// 必須透過 FreeThis() 來刪除, 預設 delete this; 不處理 this->Next_;
   /// 若有使用 ObjectPool 則將 this 還給 ObjectPool;
   virtual void FreeThis();

   void RevPrint(fon9::RevBuffer& rbuf) const override;

   const OmsOrderRaw*    Next()    const { return this->Next_; }
   const OmsRequestBase& Request() const { return *this->Request_; }
   OmsOrder&             Order()   const { return *this->Order_; }

   /// Order 異動次數的記錄.
   uint32_t UpdSeq()  const { return this->UpdSeq_; }

   /// 延續 prev 的異動.
   /// - 一般而言 prev 來自 order->Tail();
   /// - 從 prev 複製必要的內容, 例: this->OrdNo_ = prev.OrdNo_;
   /// - this->UpdateOrderSt_ = this->Order_->LastOrderSt();
   /// - 設定(清除)部分欄位, 例如: this->ErrCode_ = OmsErrCode_Null;
   /// - 不設定 this->UpdateTime_ = fon9::UtcNow(); 等到結束異動 ~OmsRequestRunnerInCore() 時設定.
   virtual void ContinuePrevUpdate(const OmsOrderRaw& prev);

   /// 在 OmsRequestRunnerInCore::Update() 判斷新單被拒絕時會呼叫這裡.
   /// 新單被拒絕, 應清除 AfterQty, LeavesQty.
   /// 預設: do nothing.
   virtual void OnOrderReject();

   /// 預設傳回 true; 表示條件符合.
   /// 範例: OmsTwsOrderRaw 檢查 OType 是否存在於 act.OTypes_;
   virtual bool CheckErrCodeAct(const OmsErrCodeAct& act) const;

   /// 通常為: LeavesQty_ > 0;
   /// 但也有例外: 詢價, 報價. 
   virtual bool IsWorking() const = 0;
   /// 是否有任何的成交?
   virtual OmsFilledFlag HasFilled() const = 0;
   /// 若 ErrCodeAct 設定為 Rerun, 且確實將要進行 step->RerunRequest(); 則在此通知;
   /// 此時若為新單 Rerun, 則應重新設定 LeavesQty = AfterQty = BeforeQty;
   /// 返回 true(預設) 表示允許執行 step->RerunRequest();
   virtual bool OnBeforeRerun(const OmsReportRunnerInCore& runner);

   LgOut GetLgOut() const {
      assert(dynamic_cast<const OmsRequestTrade*>(this->Request_) != nullptr);
      auto  lgOut = static_cast<const OmsRequestTrade*>(this->Request_)->LgOut_;
      if (fon9_LIKELY(lgOut != LgOut::Unknown))
         return lgOut;
      if (auto* ini = this->Order().Initiator())
         return ini->LgOut_;
      return lgOut;
   }
};
//--------------------------------------------------------------------------//
template <class OrderBase>
class OmsChildOrderT : public OrderBase {
   fon9_NON_COPY_NON_MOVE(OmsChildOrderT);
   using base = OrderBase;
public:
   using base::base;
   OmsChildOrderT() = default;

   void SetParentOrder(OmsOrder& parentOrder) override {
      this->SetThisIsChildOrder(parentOrder);
   }
};
//--------------------------------------------------------------------------//
inline OmsOrderRaw* OmsOrderFactory::MakeOrder(OmsRequestBase& starter, OmsScResource* scRes) {
   return this->MakeOrderImpl()->InitializeByStarter(*this, starter, scRes);
}
// -----
inline const OmsRequestIni* OmsOrder::Initiator() const {
   assert(this->Head_ != nullptr);
   if (fon9_UNLIKELY(this->Head_->UpdateOrderSt_ < f9fmkt_OrderSt_IniAccepted))
      return nullptr;
   assert(dynamic_cast<const OmsRequestIni*>(this->Head_->Request_) != nullptr);
   return static_cast<const OmsRequestIni*>(this->Head_->Request_);
}
inline bool OmsOrder::OrderEndUpdate(const OmsOrderRaw& last) {
   assert(this->Tail_ == &last);
   if (fon9_LIKELY(last.UpdateOrderSt_ != f9fmkt_OrderSt_ReportPending)) {
      if (last.UpdateOrderSt_ >= f9fmkt_OrderSt_IniAccepted)
         this->LastOrderSt_ = last.UpdateOrderSt_;
      return true;
   }
   if (this->FirstPending_ == nullptr)
      this->FirstPending_ = &last;
   return false;
}
inline bool OmsOrder::IsWorkingOrder() const {
   if (const auto* tail = this->Tail_)
      return !tail->IsFrozeScLeaves_ && tail->IsWorking();
   return false;
}
inline OmsFilledFlag OmsOrder::HasFilled() const {
   if (const auto* tail = this->Tail_)
      return tail->HasFilled();
   return OmsFilledFlag::None;
}
//--------------------------------------------------------------------------//
inline void OmsRequestBase::SetLastUpdated(OmsOrderRaw& lastupd) const {
   // 若 lastupd 來自 Parent, 但 this 為 child 的成交, 則不可改變 this->LastUpdated_;
   assert(!this->IsAbandoned());
   auto* curlast = this->LastUpdated();
   if (fon9_LIKELY(curlast == nullptr || &curlast->Order() == &lastupd.Order()))
      this->LastUpdatedOrReportRef_ = &lastupd;
}
inline const OmsOrderRaw* OmsRequestBase::LastUpdated() const {
   if (auto* curitem = this->LastUpdatedOrReportRef_)
      if (fon9_LIKELY(!this->IsAbandoned() && curitem->RxKind() == f9fmkt_RxKind_Order)) {
         assert(dynamic_cast<const OmsOrderRaw*>(curitem) != nullptr);
         return static_cast<const OmsOrderRaw*>(curitem);
      }
   return nullptr;
}
inline const OmsRequestBase* OmsRequestBase::ReportRef() const {
   if (auto* curitem = this->LastUpdatedOrReportRef_)
      if (!this->IsAbandoned() && curitem->RxKind() != f9fmkt_RxKind_Order) {
         assert(dynamic_cast<const OmsRequestBase*>(curitem) != nullptr);
         return static_cast<const OmsRequestBase*>(curitem);
      }
   return nullptr;
}
inline OmsOrder* GetRequestOrder(const OmsRequestBase* pthis) {
   for (;;) {
      auto* curitem = pthis->LastUpdatedOrReportRef_;
      if (fon9_UNLIKELY(curitem == nullptr || pthis->IsAbandoned()))
         return nullptr;
      if (fon9_LIKELY(curitem->RxKind() == f9fmkt_RxKind_Order)) {
         assert(dynamic_cast<const OmsOrderRaw*>(curitem) != nullptr);
         return &static_cast<const OmsOrderRaw*>(curitem)->Order();
      }
      assert(dynamic_cast<const OmsRequestBase*>(curitem) != nullptr);
      pthis = static_cast<const OmsRequestBase*>(curitem);
   }
}

static inline bool IsWorkingDelete(const OmsOrderRaw& iord) {
   const auto& req = iord.Request();
   if (req.RxKind() != f9fmkt_RxKind_RequestDelete)
      return false;
   const OmsOrderRaw* ilast = req.LastUpdated();
   return((ilast ? ilast : &iord)->RequestSt_ < f9fmkt_TradingRequestSt_Done);
}

} // namespaces
#endif//__f9omstw_OmsOrder_hpp__
