// \file f9omstw/OmsRequestTrade.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestTrade_hpp__
#define __f9omstw_OmsRequestTrade_hpp__
#include "f9omstw/OmsRequestBase.hpp"
#include "f9omstw/OmsRequestPolicy.hpp"
#include "f9omstw/IvacNo.hpp"

namespace f9omstw {

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

/// 下單要求(排除成交): 新、刪、改、查; 共同的基底 OmsRequestTrade;
class OmsRequestTrade : public OmsRequestBase,
                        public OmsRequestFrom,
                        public OmsRequestCliDef {
   fon9_NON_COPY_NON_MOVE(OmsRequestTrade);
   using base = OmsRequestBase;

   OmsRequestPolicySP   Policy_;

   static void MakeFieldsImpl(fon9::seed::Fields& flds);
protected:
   template <class Derived>
   static void MakeFields(fon9::seed::Fields& flds) {
      static_assert(fon9_OffsetOf(Derived, SesName_) == fon9_OffsetOf(OmsRequestTrade, SesName_),
                    "'OmsRequestTrade' must be the first base class in derived.");
      MakeFieldsImpl(flds);
   }

public:
   OmsRequestTrade(OmsRequestFactory& creator, f9fmkt_RxKind reqKind = f9fmkt_RxKind_Unknown)
      : base{creator, reqKind} {
   }
   OmsRequestTrade(f9fmkt_RxKind reqKind = f9fmkt_RxKind_Unknown)
      : base{reqKind} {
   }

   void SetPolicy(OmsRequestPolicySP policy) {
      assert(this->Policy_.get() == nullptr);
      this->Policy_ = std::move(policy);
   }
   const OmsRequestPolicy* Policy() const {
      return this->Policy_.get();
   }

   /// 請參閱「下單流程」文件.
   /// 在建立好 req 之後的 req 驗證程序, 此時還在 user thread, 尚未進入 OmsCore.
   /// 預設傳回 true;
   virtual bool ValidateInUser(OmsRequestRunner&) {
      return true;
   }

   /// 執行下單步驟的前置作業:
   /// - assert(runner.Request_.get() == this);
   /// - res.FetchRequestId(*this);
   /// - OmsRequestIni
   ///   - iniReq = this->PreCheck_OrdKey();
   ///   - iniReq->PreCheck_IvRight();
   ///   - 新單 or 補單操作: this->Creator_->OrderFactory_->MakeOrder();
   ///   - 一般刪改查, 返回: iniReq->LastUpdated()->Order_->BeginUpdate(*this);
   /// - OmsRequestUpd 一般刪改查:
   ///   - iniReq = this->PreCheck_GetRequestInitiator();
   ///   - iniReq->LastUpdated()->Order_->Initiator_->PreCheck_IvRight();
   ///   - 返回: iniReq->LastUpdated()->Order_->BeginUpdate(*this);
   /// - 如果返回 nullptr, 表示已呼叫 runner.RequestAbandon();
   /// - 返回後 request 不應再有異動.
   virtual OmsOrderRaw* BeforeRunInCore(OmsRequestRunner& runner, OmsResource& res) = 0;
};

//--------------------------------------------------------------------------//

struct OmsRequestIniDat {
   IvacNo            IvacNo_;
   fon9::CharAry<10> SubacNo_;
   fon9::CharAry<10> SalesNo_;

   OmsRequestIniDat() {
      memset(this, 0, sizeof(*this));
   }
};

/// OmsRequestIni: 一般而言是新單要求.
/// 也有可能是遺失單的補單「刪單(不允許改單)、查詢」之類的操作, 此時「數量、價格」欄位應填「原始新單」的值。
class OmsRequestIni : public OmsRequestTrade, public OmsRequestIniDat {
   fon9_NON_COPY_NON_MOVE(OmsRequestIni);
   using base = OmsRequestTrade;

   friend class OmsOrder; // OmsOrder 建構時需要呼叫 SetInitiatorFlag() 的權限.
   void SetInitiatorFlag() {
      this->RxItemFlags_ |= OmsRequestFlag_Initiator;
   }
   static void MakeFieldsImpl(fon9::seed::Fields& flds);

   OmsIvRight CheckIvRight(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) const;

protected:
   template <class Derived>
   static void MakeFields(fon9::seed::Fields& flds) {
      static_assert(fon9_OffsetOf(Derived, IvacNo_) == fon9_OffsetOf(OmsRequestIni, IvacNo_),
                    "'OmsRequestIni' must be the first base class in derived.");
      MakeFieldsImpl(flds);
   }

   /// 傳回不相同的欄位名稱, 例如: "IvacNo", "SubacNo";
   /// 不檢查 SalesNo, 因為此欄不同也不影響委託內容.
   const char* IsIniFieldEqualImpl(const OmsRequestIni& req) const;

public:
   OmsRequestIni(OmsRequestFactory& creator, f9fmkt_RxKind reqKind = f9fmkt_RxKind_RequestNew)
      : base{creator, reqKind} {
   }
   OmsRequestIni(f9fmkt_RxKind reqKind = f9fmkt_RxKind_RequestNew)
      : base{reqKind} {
   }

   /// 檢查: 若 OrdNo_.begin() != '\0'; 則必須有 Policy()->IsAllowAnyOrdNo() 權限.
   bool ValidateInUser(OmsRequestRunner& reqRunner) override;

   /// \retval "RequestIni"   req 不是 OmsRequestIni;
   /// \retval "BrkId"        BrkId 不相同.
   /// \retval "IvacNo"       IvacNo 不相同.
   /// \retval "SubacNo"      SubacNo 不相同.
   virtual const char* IsIniFieldEqual(const OmsRequestBase& req) const;

   /// - 檢查 BrkId, OrdNo: 必須有填.
   /// - 如果沒填 Market, 則根據 scRes.Symb_ 判斷 Market.
   /// - 如果沒填 SessionId 則(由衍生者處理):
   ///   - 證券: 根據「數量 or 價格」判斷.
   ///   - 期權: 根據「期交所的 FlowGroup」判斷.
   ///
   /// \retval nullptr 表示失敗, 已呼叫 runner.RequestAbandon();
   /// \retval this    表示此筆為「新單要求」 or 「補單的刪改查」.
   /// \retval else    表示此筆為「刪改查要求」, 傳回要操作的「初始委託要求」.
   virtual const OmsRequestIni* PreCheck_OrdKey(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes);

   /// this 為「初始委託要求」, 檢查 runner.Request_.Policy() 的權限是否允許 runner.Request_;
   /// - 應在 inireq = runner.Request_->PreCheck_OrdKey() 之後執行: inireq->PreCheck_IvRight(runner...);
   /// - this 有可能等於 runner.Request_:「新單要求」 or 「補單的刪改查(必須要有額外的 OmsIvRight::AllowRequestIni 權限)」
   /// - 如果允許, 且不是因 admin 權限, 則會設定 scRes.Ivr_;
   bool PreCheck_IvRight(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) const;

   /// this 為「初始委託要求」, 檢查 runner.Request_.Policy() 的權限是否允許 runner.Request_;
   /// - 應在 inireq = runner.Request_->PreCheck_GetRequestInitiator() 之後執行:
   ///   inireq->LastUpdated()->Order_->Initiator_->PreCheck_IvRight(runner);
   bool PreCheck_IvRight(OmsRequestRunner& runner, OmsResource& res) const;

   OmsOrderRaw* BeforeRunInCore(OmsRequestRunner& runner, OmsResource& res) override;
};

class OmsRequestUpd : public OmsRequestTrade {
   fon9_NON_COPY_NON_MOVE(OmsRequestUpd);
   using base = OmsRequestTrade;

   OmsRxSNO IniSNO_{0};

   static void MakeFieldsImpl(fon9::seed::Fields& flds);
protected:
   template <class Derived>
   static void MakeFields(fon9::seed::Fields& flds) {
      static_assert(fon9_OffsetOf(Derived, IniSNO_) == fon9_OffsetOf(OmsRequestUpd, IniSNO_),
                    "'OmsRequestUpd' must be the first base class in derived.");
      MakeFieldsImpl(flds);
   }

public:
   using base::base;
   OmsRequestUpd() = default;

   OmsRxSNO IniSNO() const {
      return this->IniSNO_;
   }

   const OmsRequestBase* PreCheck_GetRequestInitiator(OmsRequestRunner& runner, OmsResource& res) {
      return base::PreCheck_GetRequestInitiator(runner, &this->IniSNO_, res);
   }

   OmsOrderRaw* BeforeRunInCore(OmsRequestRunner& runner, OmsResource& res) override;
};

} // namespaces
#endif//__f9omstw_OmsRequestTrade_hpp__
