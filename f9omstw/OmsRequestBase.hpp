// \file f9omstw/OmsRequestBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestBase_hpp__
#define __f9omstw_OmsRequestBase_hpp__
#include "f9omstw/OmsRequestId.hpp"
#include "fon9/seed/Tab.hpp"

namespace f9omstw {

/// OmsRequestBase 定義 fon9::fmkt::TradingRxItem::RxItemFlags_;
enum OmsRequestFlag : uint8_t {
   OmsRequestFlag_Initiator = 0x01,
   /// 經由外部回報所建立的 request.
   OmsRequestFlag_External = 0x02,
   /// 無法進入委託流程: 無法建立 OmsOrder, 或找不到對應的 OmsOrder.
   OmsRequestFlag_Abandon = 0x04,
};
fon9_ENABLE_ENUM_BITWISE_OP(OmsRequestFlag);

/// 市場唯一編號:
/// - 需要再配合 fon9::fmkt::TradingRequest 裡面的 Market_; SessionId_;
/// - 才能組成完整的「市場委託唯一Key」:「BrkId-Market-SessionId-OrdNo」;
struct OmsOrdKey {
   OmsBrkId BrkId_;
   OmsOrdNo OrdNo_;
   char     padding_____[3];

   OmsOrdKey() {
      memset(this, 0, sizeof(*this));
   }
};

/// 「新、刪、改、查、成交」的共同基底.
class OmsRequestBase : public fon9::fmkt::TradingRequest, public OmsRequestId, public OmsOrdKey {
   fon9_NON_COPY_NON_MOVE(OmsRequestBase);
   using base = fon9::fmkt::TradingRequest;

   friend class OmsBackend; // 取得修改 LastUpdated_ 的權限.
   union {
      OmsOrderRaw mutable* LastUpdated_{nullptr};
      /// 當 IsEnumContains(this->RequestFlags(), OmsRequestFlag_Abandon) 則沒有 LastUpdated_;
      /// 此時 AbandonReason_ 說明中斷要求的原因.
      std::string*   AbandonReason_;
   };
   fon9::TimeStamp   CrTime_;

   static void MakeFieldsImpl(fon9::seed::Fields& flds);
protected:
   template <class Derived>
   static void MakeFields(fon9::seed::Fields& flds) {
      static_assert(fon9_OffsetOf(Derived, RxKind_) == fon9_OffsetOf(OmsRequestBase, RxKind_),
                    "'OmsRequestBase' must be the first base class in derived.");
      MakeFieldsImpl(flds);
   }

   /// 檢查取出此筆委託要操作的原始新單要求.
   /// - 如果有提供 iniSNO, 則 this.OrdKey 如果有填則必須正確, 如果沒填則自動填入正確值.
   /// - 如果沒提供 iniSNO, 則使用 this->OrdKey 尋找.
   /// - 如果找不到, 則在返回前會呼叫 this->Abandon("Order not found.");
   const OmsRequestBase* PreCheckIniRequest(OmsRxSNO* pIniSNO, OmsResource& res);

public:
   OmsRequestFactory* const   Creator_;
   OmsRequestBase(OmsRequestFactory& creator, f9fmkt_RxKind reqKind = f9fmkt_RxKind_Unknown)
      : base(reqKind)
      , CrTime_{fon9::UtcNow()}
      , Creator_(&creator) {
   }
   OmsRequestBase(f9fmkt_RxKind reqKind = f9fmkt_RxKind_Unknown)
      : base(reqKind)
      , Creator_(nullptr) {
   }
   void Initialize(OmsRequestFactory& creator, fon9::TimeStamp now = fon9::UtcNow()) {
      *const_cast<OmsRequestFactory**>(&this->Creator_) = &creator;
      this->CrTime_ = now;
   }
   /// 解構時:
   /// if(this->RequestFlags() & OmsRequestFlag_Abandon) 則刪除 AbandonReason_.
   /// if(this->RequestFlags() & OmsRequestFlag_Initiator) 則刪除 Order.
   ~OmsRequestBase();

   /// 這裡只是提供欄位的型別及名稱, 不應使用此欄位存取 RxSNO_;
   /// fon9_MakeField2_const(OmsRequestBase, RxSNO);
   static fon9::seed::FieldSP MakeField_RxSNO();

   const OmsRequestBase* CastToRequest() const override;

   void RevPrint(fon9::RevBuffer& rbuf) const override;

   OmsRequestFlag RequestFlags() const {
      return static_cast<OmsRequestFlag>(this->RxItemFlags_);
   }
   bool IsInitiator() const {
      return (this->RxItemFlags_& OmsRequestFlag_Initiator) == OmsRequestFlag_Initiator;
   }
   const OmsOrderRaw* LastUpdated() const {
      return IsEnumContains(this->RequestFlags(), OmsRequestFlag_Abandon) ? nullptr : this->LastUpdated_;
   }
   /// 如果傳回非nullptr, 則表示此筆下單要求失敗, 沒有進入系統.
   /// - Abandon 原因可能為: 欄位錯誤(例如: 買賣別無效), 不是可用帳號...
   /// - 進入系統後的失敗會用 Reject 機制, 透過 OmsOrderRaw 處理.
   const std::string* AbandonReason() const {
      return IsEnumContains(this->RequestFlags(), OmsRequestFlag_Abandon) ? this->AbandonReason_ : nullptr;
   }
   void Abandon(std::string reason) {
      assert(this->AbandonReason_ == nullptr);
      assert((this->RxItemFlags_ & (OmsRequestFlag_Abandon | OmsRequestFlag_Initiator)) == OmsRequestFlag{});
      this->AbandonReason_ = new std::string(std::move(reason));
      this->RxItemFlags_ |= OmsRequestFlag_Abandon;
   }
};

} // namespaces
#endif//__f9omstw_OmsRequestBase_hpp__
