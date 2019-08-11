// \file f9omstw/OmsOrder.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsOrder_hpp__
#define __f9omstw_OmsOrder_hpp__
#include "f9omstw/OmsOrderFactory.hpp"
#include "f9omstw/OmsRequestTrade.hpp"
#include "f9omstw/OmsReportFilled.hpp"
#include "f9omstw/OmsIvSymb.hpp"
#include "fon9/fmkt/Symb.hpp"

namespace f9omstw {

fon9_WARN_DISABLE_PADDING;
/// 委託書的相關風控資源.
/// - 放在 OmsOrder 裡面.
/// - 當委託有異動時, 可以快速調整風控計算, 不用再查一次表格.
struct OmsScResource {
   fon9::fmkt::SymbSP   Symb_;
   OmsIvBaseSP          Ivr_;
   OmsIvSymbSP          IvSymb_;

   void CheckMoveFrom(OmsScResource&& rhs) {
      if (!this->Symb_)    this->Symb_   = std::move(rhs.Symb_);
      if (!this->Ivr_)     this->Ivr_    = std::move(rhs.Ivr_);
      if (!this->IvSymb_)  this->IvSymb_ = std::move(rhs.IvSymb_);
   }
};
fon9_WARN_POP;

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
   const OmsOrderRaw*      FirstPending_{nullptr};
   f9fmkt_OrderSt          LastOrderSt_{};
   char                    padding___[7];

   void ProcessPendingReport(OmsResource& res);

protected:
   virtual ~OmsOrder();

public:
   /// 若透過類似 ObjSupplier 的機制, 無法提供建構時參數.
   /// 實際使用前再配合 Initialize(OmsOrderFactory& creator, OmsScResource* scRes) 初始化;
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
   f9fmkt_OrderSt       LastOrderSt() const  { return this->LastOrderSt_; }

   const OmsScResource& ScResource() const   { return this->ScResource_; }
   OmsScResource&       ScResource()         { return this->ScResource_; }

   OmsBrk* GetBrk(OmsResource& res) const;

   template <class SymbId>
   fon9::fmkt::Symb* GetSymb(OmsResource& res, const SymbId& symbid);

   /// 透過 this->Creator_->MakeOrderRawImpl() 建立委託異動資料.
   /// - 通常配合 OmsRequestRunnerInCore 建立 runner; 然後執行下單(或回報)步驟.
   /// - 或是 Backend.Reload 時.
   OmsOrderRaw* BeginUpdate(const OmsRequestBase& req);
   /// last 必定是 this->BeginUpdate() 的返回值.
   /// - 如果 res == nullptr 則不考慮 ProcessPendingReport(); 用在 Backend.Reload 時.
   void EndUpdate(const OmsOrderRaw& last, OmsResource* res);

   const OmsReportFilled* InsertFilled(const OmsReportFilled* currFilled) {
      return OmsReportFilled::Insert(&this->FilledHead_, &this->FilledLast_, currFilled);
   }
};

//--------------------------------------------------------------------------//

/// 每次委託異動增加一個 OmsOrderRaw;
/// - 從檔案重新載入.
///   - new OmsOrderRaw(必要參數),
///   - 然後透過 OmsOrderFactory::Form_ 填入全部的欄位;
/// - OmsOrderFactory::MakeOrderRaw() 使用 ObjectPool 機制:
///   - 使用 OmsOrderRaw 預設建構, 放到 ObjectPool.
///   - 使用前再透過 OmsOrderRaw::Initialize() 初始化.
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
      static_assert(fon9_OffsetOf(Derived, UpdateOrderSt_) == fon9_OffsetOf(OmsOrderRaw, UpdateOrderSt_),
                    "'OmsOrderRaw' must be the first base class in derived.");
      MakeFieldsImpl(flds);
   }

   /// 若 OmsOrderFactory::MakeOrderRaw() 使用 ObjectPool 機制, 則可能先建立 OmsOrderRaw,
   /// 然後在 OmsOrder::BeginUpdate() 返回前, 再透過 OmsOrderRaw::Initialize() 初始化.
   /// 在此預設建構裡面, 應先將一些欄位初始化, 讓 OmsOrderRaw::Initialize() 可以快一些.
   OmsOrderRaw() : base(f9fmkt_RxKind_Order) {
   }
   virtual ~OmsOrderRaw();
   const OmsOrderRaw* CastToOrder() const override;

public:
   f9fmkt_OrderSt              UpdateOrderSt_;
   f9fmkt_TradingRequestSt     RequestSt_;
   OmsErrCode                  ErrCode_{OmsErrCode_NoError};
   OmsOrdNo                    OrdNo_{nullptr};
   char                        padding___[3];

   /// 異動時的本機時間.
   fon9::TimeStamp             UpdateTime_;
   /// 若訊息長度沒有超過 fon9::CharVector::kMaxBinsSize (在x64系統, 大約 23 bytes),
   /// 則可以不用分配記憶體, 一般而言常用的訊息不會超過(例如: "Sending by BBBB-SS", "Queuing"),
   /// 通常在有錯誤時才會使用較長的訊息.
   fon9::CharVector            Message_;

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
   /// - 從 prev 複製必要的內容, 例: this->OrdNo_ = prev.OrdNo_;
   /// - this->UpdateOrderSt_ = this->Order_->LastOrderSt();
   /// - 設定(清除)部分欄位, 例如: this->ErrCode_ = OmsErrCode_NoError;
   /// - 不設定 this->UpdateTime_ = fon9::UtcNow(); 等到結束異動 ~OmsRequestRunnerInCore() 時設定.
   virtual void ContinuePrevUpdate(const OmsOrderRaw& prev);

   /// 在 OmsRequestRunnerInCore::Update() 判斷新單被拒絕時會呼叫這裡.
   /// 新單被拒絕, 應清除 AfterQty, LeavesQty.
   /// 預設: do nothing.
   virtual void OnOrderReject();
};

//--------------------------------------------------------------------------//

inline OmsOrderRaw* OmsOrderFactory::MakeOrder(OmsRequestBase& starter, OmsScResource* scRes) {
   return this->MakeOrderImpl()->InitializeByStarter(*this, starter, scRes);
}

inline const OmsRequestIni* OmsOrder::Initiator() const {
   assert(this->Head_ != nullptr);
   if (fon9_UNLIKELY(this->Head_->UpdateOrderSt_ < f9fmkt_OrderSt_NewStarting))
      return nullptr;
   assert(dynamic_cast<const OmsRequestIni*>(this->Head_->Request_) != nullptr);
   return static_cast<const OmsRequestIni*>(this->Head_->Request_);

}
inline void OmsOrder::EndUpdate(const OmsOrderRaw& last, OmsResource* res) {
   assert(this->Tail_ == &last);
   if (fon9_UNLIKELY(last.UpdateOrderSt_ == f9fmkt_OrderSt_ReportPending)) {
      if (this->FirstPending_ == nullptr)
         this->FirstPending_ = &last;
   }
   else {
      if (last.UpdateOrderSt_ >= f9fmkt_OrderSt_NewStarting)
         this->LastOrderSt_ = last.UpdateOrderSt_;
      if (fon9_UNLIKELY(res && this->FirstPending_ != nullptr
                        && last.RequestSt_ > f9fmkt_TradingRequestSt_LastRunStep))
         this->ProcessPendingReport(*res);
   }
}

} // namespaces
#endif//__f9omstw_OmsOrder_hpp__
