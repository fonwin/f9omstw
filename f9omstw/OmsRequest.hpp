// \file f9omstw/OmsRequest.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequest_hpp__
#define __f9omstw_OmsRequest_hpp__
#include "f9omstw/OmsRequestPolicy.hpp"
#include "f9omstw/IvacNo.hpp"
#include "fon9/fmkt/Trading.hpp"
#include "fon9/seed/Tab.hpp"
#include "fon9/CharAry.hpp"

namespace f9omstw {

/// 每台主機自行依序編號.
using OmsRequestSNO = uint64_t;

/// Request 在多主機環境下的唯一編號.
/// - API建立下單要求時: ReqUID = OmsMakeReqUID(LocalHostId, ReqSNO).
/// - 外部單: 由外部單(回報)提供者提供編碼規則, 例如:
///   - Market + Session + BrkId[2尾碼] + OrdNo + 外部單Seq(或AfQty).
///   - Market + Session + BrkId[2尾碼] + OrdNo + 成交序號.
class OmsRequestId {
   friend class OmsRequestMgr;
   /// 此序號由 OmsRequestMgr 填入, 不列入 seed::Field;
   OmsRequestSNO  ReqSNO_;
public:
   fon9::CharAry<16> ReqUID_;

   OmsRequestId() {
      memset(this, 0, sizeof(*this));
   }

   OmsRequestSNO ReqSNO() const {
      return this->ReqSNO_;
   }

   /// fon9_MakeField(fon9::Named{"ReqSNO"}, OmsRequestBase, ReqSNO_);
   static fon9::seed::FieldSP MakeField_ReqSNO();
};

/// fon9::fmkt::TradingRequest::RequestFlags_;
enum OmsRequestFlag : uint8_t {
   OmsRequestFlag_Initiator = 0x01,
   /// 經由外部回報所建立的 request.
   OmsRequestFlag_External = 0x02,
   /// 無法進入委託流程: 無法建立 OmsOrder, 或找不到對應的 OmsOrder.
   OmsRequestFlag_Abandon = 0x04,
};
fon9_ENABLE_ENUM_BITWISE_OP(OmsRequestFlag);

/// 新、刪、改、查、成交; 都視為一種 request 共同的基底就是 OmsRequestBase;
class OmsRequestBase : public fon9::fmkt::TradingRequest, public OmsRequestId {
   fon9_NON_COPY_NON_MOVE(OmsRequestBase);
   using base = fon9::fmkt::TradingRequest;

   friend class OmsRequestRunnerInCore; // 取得修改 LastUpdated_ 的權限.
   union {
      OmsOrderRaw mutable* LastUpdated_{nullptr};
      /// 當 IsEnumContains(this->RequestFlags(), OmsRequestFlag_Abandon) 則沒有 LastUpdated_;
      /// 此時 AbandonReason_ 說明中斷要求的原因.
      std::string*   AbandonReason_;
   };
public:
   OmsRequestFactory* const   Creator_;
   OmsRequestBase(OmsRequestFactory& creator, f9fmkt_RequestKind reqKind = f9fmkt_RequestKind_Unknown)
      : base{reqKind}
      , Creator_(&creator) {
   }
   OmsRequestBase(f9fmkt_RequestKind reqKind = f9fmkt_RequestKind_Unknown)
      : base{reqKind}
      , Creator_(nullptr) {
   }
   void Initialize(OmsRequestFactory& creator) {
      *const_cast<OmsRequestFactory**>(&this->Creator_) = &creator;
   }
   /// 解構時:
   /// if(this->RequestFlags_ & OmsRequestFlag_Abandon) 則刪除 AbandonReason_.
   /// if(this->RequestFlags_ & OmsRequestFlag_Initiator) 則刪除 Order.
   ~OmsRequestBase();

   static void MakeFields(fon9::seed::Fields& flds);

   OmsRequestFlag RequestFlags() const {
      return static_cast<OmsRequestFlag>(this->RequestFlags_);
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
};

struct OmsRequestFrom {
   /// 收單處理程序名稱, 例如: Rc, FixIn...
   fon9::CharAry<8>  SesName_;
   fon9::CharAry<12> UserId_;
   fon9::CharAry<16> FromIp_;
   /// 來源別.
   fon9::CharAry<4>  Src_;

   OmsRequestFrom() {
      memset(this, 0, sizeof(*this));
   }
};

struct OmsRequestCliDef {
   fon9::CharAry<16> UsrDef_;
   fon9::CharAry<16> ClOrdId_;

   OmsRequestCliDef() {
      memset(this, 0, sizeof(*this));
   }
};

/// 下單要求(排除成交): 新、刪、改、查; 共同的基底 OmsTradingRequest;
class OmsTradingRequest : public OmsRequestBase,
                          public OmsRequestFrom,
                          public OmsRequestCliDef {
   fon9_NON_COPY_NON_MOVE(OmsTradingRequest);
   using base = OmsRequestBase;

   OmsRequestPolicySP   Policy_;
public:
   OmsTradingRequest(OmsRequestFactory& creator, f9fmkt_RequestKind reqKind = f9fmkt_RequestKind_Unknown)
      : base{creator, reqKind} {
   }
   OmsTradingRequest(f9fmkt_RequestKind reqKind = f9fmkt_RequestKind_Unknown)
      : base{reqKind} {
   }

   void SetPolicy(OmsRequestPolicySP&& policy) {
      assert(this->Policy_.get() == nullptr);
      this->Policy_ = std::move(policy);
   }
   const OmsRequestPolicy* Policy() const {
      return this->Policy_.get();
   }

   /// 請參閱「下單流程」文件.
   /// 在建立好 req 之後, 預先檢查程序, 此時還在 user thread, 尚未進入 OmsCore.
   /// return this->Policy_ ? this->Policy_->PreCheck(reqRunner) : true;
   virtual bool PreCheck(OmsRequestRunner& reqRunner);

   static void MakeFields(fon9::seed::Fields& flds);
};

struct OmsRequestIvacNo {
   // BrkId: Tws=CharAry<4>; Twf=CharAry<7>;
   // BrkId 放在 OmsRequestNewTws; OmsRequestNewTwf; 裡面.

   IvacNo            IvacNo_;
   fon9::CharAry<12> SubacNo_;
   fon9::CharAry<8>  SalesNo_;

   OmsRequestIvacNo() {
      memset(this, 0, sizeof(*this));
   }
};

class OmsRequestNew : public OmsTradingRequest, public OmsRequestIvacNo {
   fon9_NON_COPY_NON_MOVE(OmsRequestNew);
   using base = OmsTradingRequest;

   friend class OmsOrder; // OmsOrder 建構時需要呼叫 SetInitiatorFlag() 的權限.
   void SetInitiatorFlag() {
      this->RequestFlags_ |= OmsRequestFlag_Initiator;
   }
public:
   OmsRequestNew(OmsRequestFactory& creator, f9fmkt_RequestKind reqKind = f9fmkt_RequestKind_New)
      : base{creator, reqKind} {
   }
   OmsRequestNew(f9fmkt_RequestKind reqKind = f9fmkt_RequestKind_New)
      : base{reqKind} {
   }

   static void MakeFields(fon9::seed::Fields& flds);
};

class OmsRequestUpd : public OmsTradingRequest {
   fon9_NON_COPY_NON_MOVE(OmsRequestUpd);
   using base = OmsTradingRequest;

   OmsRequestSNO  IniSNO_{0};

public:
   using base::base;
   OmsRequestUpd() = default;

   static void MakeFields(fon9::seed::Fields& flds);

   // 衍生者應覆寫 PreCheck() 檢查並設定 RequestKind, 然後透過 base::PreCheck() 繼續檢查.
   // bool PreCheck(OmsRequestRunner& reqRunner) override;
};

/// 成交回報.
/// 在 OmsOrder 提供:
/// \code
///    OmsRequestMatch* OmsOrder::MatchHead_;
///    OmsRequestMatch* OmsOrder::MatchLast_;
/// \endcode
/// 新的成交, 如果是在 MatchLast_->MatchKey_ 之後, 就直接 append; 否則就從頭搜尋.
/// 由於成交回報「大部分」是附加到尾端, 所以這樣的處理負擔應是最小.
class OmsRequestMatch : public OmsRequestBase {
   fon9_NON_COPY_NON_MOVE(OmsRequestMatch);
   using base = OmsRequestBase;

   const OmsRequestMatch mutable* Next_{nullptr};

   /// 成交回報要求與下單線路無關, 所以這裡 do nothing.
   void NoReadyLineReject(fon9::StrView) override;

   /// 將 curr 插入 this 與 this->Next_ 之間;
   void InsertAfter(const OmsRequestMatch* curr) const {
      assert(curr->Next_ == nullptr && this->MatchKey_ < curr->MatchKey_);
      assert(this->Next_ == nullptr || curr->MatchKey_ < this->Next_->MatchKey_);
      curr->Next_ = this->Next_;
      this->Next_ = curr;
   }

   OmsRequestSNO  IniSNO_;
   uint64_t       MatchKey_{0};

public:
   using MatchKey = uint64_t;

   OmsRequestMatch(OmsRequestFactory& creator)
      : base{creator, f9fmkt_RequestKind_Match} {
   }
   OmsRequestMatch()
      : base{f9fmkt_RequestKind_Match} {
   }

   static void MakeFields(fon9::seed::Fields& flds);

   /// 將 curr 依照 MatchKey_ 的順序(小到大), 加入到「成交串列」.
   /// \retval nullptr  成功將 curr 加入成交串列.
   /// \retval !nullptr 與 curr->MatchKey_ 相同的那個 request.
   static const OmsRequestMatch* Insert(const OmsRequestMatch** ppHead, const OmsRequestMatch** ppLast, const OmsRequestMatch* curr);

   const OmsRequestMatch* Next() {
      return this->Next_;
   }
};

} // namespaces
#endif//__f9omstw_OmsRequest_hpp__
