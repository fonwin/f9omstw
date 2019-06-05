// \file f9omstw/OmsOrder.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsOrder_hpp__
#define __f9omstw_OmsOrder_hpp__
#include "f9omstw/OmsRequestTrade.hpp"
#include "f9omstw/OmsIvSymb.hpp"
#include "fon9/fmkt/Symb.hpp"

namespace f9omstw {

/// Name_   = 委託名稱, 例如: TwsOrd;
/// Fields_ = 委託書會隨著操作、成交而變動的欄位(例如: 剩餘量...),
///           委託的初始內容(例如: 帳號、商品...), 由 OmsRequest 負責提供;
class OmsOrderFactory : public fon9::seed::Tab {
   fon9_NON_COPY_NON_MOVE(OmsOrderFactory);
   using base = fon9::seed::Tab;

   virtual OmsOrderRaw* MakeOrderRawImpl() = 0;
   virtual OmsOrder* MakeOrderImpl() = 0;
public:
   using base::base;

   virtual ~OmsOrderFactory();

   /// 新單要求, 建立一個新的委託與其對應, 然後返回 OmsOrderRaw 開始首次更新.
   /// 若有提供 scRes, 則會將 std::move(*scRes) 用於 Order 的初始化.
   OmsOrderRaw* MakeOrder(OmsRequestIni& initiator, f9omstw::OmsScResource* scRes);

   /// 建立時須注意, 若此時 order.Last()==nullptr
   /// - 表示要建立的是 order 第一次異動.
   /// - 包含 order.Head_ 及之後的 data members、衍生類別, 都處在尚未初始化的狀態.
   OmsOrderRaw* MakeOrderRaw(OmsOrder& order, const OmsRequestBase& req);
};

fon9_WARN_DISABLE_PADDING;
struct OmsScResource {
   fon9::fmkt::SymbSP   Symb_;
   OmsIvBaseSP          Ivr_;
   OmsIvSymbSP          IvSymb_;

   uint32_t GetTwsSymbShUnit() const {
      if (const auto* symb = this->Symb_.get())
         if (0 < symb->ShUnit_)
            return symb->ShUnit_;
      return 1000; // 台灣證券大部分的商品 1張 = 1000股.
   }
   void CheckMoveFrom(OmsScResource&& rhs) {
      if (!this->Symb_)    this->Symb_   = std::move(rhs.Symb_);
      if (!this->Ivr_)     this->Ivr_    = std::move(rhs.Ivr_);
      if (!this->IvSymb_)  this->IvSymb_ = std::move(rhs.IvSymb_);
   }
};
fon9_WARN_POP;

/// - 運作中的 OmsOrder 擁有權屬於 OmsOrder::Initiator_;
///   所以當 OmsOrder::Initiator_ 死亡時, 會呼叫 OmsOrder::FreeThis();
class OmsOrder {
   fon9_NON_COPY_NON_MOVE(OmsOrder);
   friend class OmsOrderRaw; // 建構時更新 OmsOrder::Last_;
   OmsRequestMatch*  MatchHead_{nullptr};
   OmsRequestMatch*  MatchLast_{nullptr};
   OmsOrderRaw*      Last_{nullptr};
   OmsScResource     ScResource_;

protected:
   virtual ~OmsOrder();

public:
   const OmsRequestIni* const Initiator_;
   OmsOrderFactory* const     Creator_;

   /// 在建構時透過 creator 建立 Head_;
   /// 而建立 OmsOrderRaw 會參考到 this->Initiator_;
   /// 所以 Head_ 必須是 OmsOrder 最後的 data member.
   const OmsOrderRaw* const   Head_;

   OmsOrder(OmsRequestIni& initiator, OmsOrderFactory& creator, OmsScResource&& scRes);

   /// 實際使用前需配合 Initialize(OmsRequestIni& initiator, OmsScResource* scRes) 初始化;
   OmsOrder();
   /// 若有提供 scRes, 則會將 std::move(*scRes) 用於 this->ScResource_ 的初始化.
   virtual void Initialize(OmsRequestIni& initiator, OmsOrderFactory& creator, OmsScResource* scRes);

   /// 必須透過 FreeThis() 來刪除, 預設 delete this;
   /// 若有使用 ObjectPool 則將 this 還給 ObjectPool;
   virtual void FreeThis();

   const OmsOrderRaw* Last() const {
      return this->Last_;
   }

   const OmsScResource& ScResource() const {
      return this->ScResource_;
   }
   OmsScResource& ScResource() {
      return this->ScResource_;
   }

   OmsBrk* GetBrk(OmsResource& res) const;
};

/// 每次委託異動增加一個 OmsOrderRaw;
/// - 從檔案重新載入.
///   - new OmsOrderRaw(必要參數),
///   - 然後透過 OmsOrderFactory::Form_ 填入全部的欄位;
/// - OmsOrderFactory::MakeOrderRaw() 使用 ObjectPool 機制:
///   - 使用 OmsOrderRaw 預設建構, 放到 ObjectPool.
///   - 使用前再透過 OmsOrderRaw::Initialize() 初始化.
class OmsOrderRaw : public OmsRxItem {
   fon9_NON_COPY_NON_MOVE(OmsOrderRaw);
   const OmsOrderRaw mutable* Next_{nullptr};

   friend class OmsOrderFactory;

   /// 初始化給 OmsOrder 的首次異動.
   /// - 設定 this->Order_; this->Request_;
   /// - 為了速度, 此處應盡可能的簡化, 必要的初值應在「預設建構」裡面處理.
   virtual void Initialize(OmsOrder& order);

   /// - 設定 this->Order_; this->Request_; this->Prev_; this->UpdSeq_; ...
   /// - 呼叫 this->ContinuePrevUpdate(); 設定初始化內容.
   virtual void Initialize(const OmsOrderRaw* prev, const OmsRequestBase& req);

   static void MakeFieldsImpl(fon9::seed::Fields& flds);
protected:
   template <class Derived>
   static void MakeFields(fon9::seed::Fields& flds) {
      static_assert(fon9_OffsetOf(Derived, OrderSt_) == fon9_OffsetOf(OmsOrderRaw, OrderSt_),
                    "'OmsOrderRaw' must be the first base class in derived.");
      MakeFieldsImpl(flds);
   }

   /// 若 OmsOrderFactory::MakeOrderRaw() 使用 ObjectPool 機制, 則可能先建立 OmsOrderRaw,
   /// 然後在 OmsOrderFactory::MakeOrderRaw() 返回前, 再透過 OmsOrderRaw::Initialize() 初始化.
   /// 在此預設建構裡面, 應先將一些欄位初始化, 讓 OmsOrderRaw::Initialize(order) 可以快一些.
   OmsOrderRaw() : Order_{nullptr}, Request_{nullptr}, OrdNo_{""} {
   }
   virtual ~OmsOrderRaw();
   const OmsOrderRaw* CastToOrder() const override;

public:
   OmsOrder* const             Order_;
   const OmsRequestBase* const Request_;
   const OmsOrderRaw* const    Prev_{nullptr};
   const uint32_t              UpdSeq_{0};

   f9fmkt_OrderSt              OrderSt_;
   f9fmkt_TradingRequestSt     RequestSt_;
   OmsErrCode                  ErrCode_{OmsErrCode_NoError};
   OmsOrdNo                    OrdNo_;
   char                        padding___[3];
   /// 異動時的本機時間.
   fon9::TimeStamp             UpdateTime_;
   /// 若訊息長度沒有超過 fon9::CharVector::kMaxBinsSize (在x64系統, 大約23 bytes),
   /// 則可以不用分配記憶體, 一般而言常用的訊息不會超過(例如: "Sending by BBBB-SS", "Queuing"),
   /// 通常在有錯誤時才會使用較長的訊息.
   fon9::CharVector            Message_;

   /// 請注意: 此時的 order 還在建構階段.
   /// 此處返回後才會設定 order.Head_;
   OmsOrderRaw(OmsOrder& order) : Order_(&order), Request_(order.Initiator_), OrdNo_{""} {
   }
   /// 使用此方式建構, 在使用前必須自行呼叫 ContinuePrevUpdate();
   OmsOrderRaw(const OmsOrderRaw* prev, const OmsRequestBase& req);

   /// 必須透過 FreeThis() 來刪除, 預設 delete this; 不處理 this->Next_; this->Prev_;
   /// 若有使用 ObjectPool 則將 this 還給 ObjectPool;
   virtual void FreeThis();

   void RevPrint(fon9::RevBuffer& rbuf) const override;

   const OmsOrderRaw* Next() const {
      return this->Next_;
   }

   /// 延續 this->Prev_ 之後的異動.
   /// - 從 this->Prev_ 複製必要的內容: this->OrderSt_ = this->Prev_->OrderSt_;
   /// - 設定(清除)部分欄位, 例如: this->ErrCode_ = OmsErrCode_NoError;
   /// - 不設定 this->UpdateTime_ = fon9::UtcNow(); 等到結束異動 ~OmsRequestRunnerInCore() 時設定.
   virtual void ContinuePrevUpdate();
};

//--------------------------------------------------------------------------//

inline OmsOrderRaw* OmsOrderFactory::MakeOrder(OmsRequestIni& initiator, f9omstw::OmsScResource* scRes) {
   OmsOrder* ord = this->MakeOrderImpl();
   ord->Initialize(initiator, *this, scRes);
   return const_cast<OmsOrderRaw*>(ord->Last());
}

} // namespaces
#endif//__f9omstw_OmsOrder_hpp__
