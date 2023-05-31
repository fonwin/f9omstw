// \file f9omstw/OmsRequestTrade.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestTrade_hpp__
#define __f9omstw_OmsRequestTrade_hpp__
#include "f9omstw/OmsRequestBase.hpp"
#include "f9omstw/OmsRequestPolicy.hpp"
#include "f9omstw/IvacNo.hpp"

namespace f9omstw {

/// - 用於 OmsRequestTrade.SesName_;
///   如果內容為 fon9_kCSTR_OmsForceInternal 則表示此筆為[強制內部單]回報.
/// - 成交回報則使用 OmsReportFilled.ReqUID_ = "I:原有規則";
///   因成交回報的 ReqUID 會在 OmsReportFilled::RunReportInCore_MakeReqUID() 重編,
///   所以即使收到遠端回報 "I:xxx" 的成交回報, 因為沒有設定 ForceInternal, 
///   所以重編成交的 ReqUID 時, 會移除 "I:";
#define fon9_kCSTR_OmsForceInternal "I:FromEx"

struct OmsRequestFrom {
   /// 收單處理程序名稱, 例如: Rc, FixIn...
   /// 如果內容為 fon9_kCSTR_OmsForceInternal 則表示此筆為[強制內部單]回報.
   fon9::CharAry<8>  SesName_;
   /// 是否為: 強制設定為本機回報, 例: [本機從交易所] [交易線路] 收到的回報.
   bool IsForceInternalRpt() const {
      static_assert(sizeof(this->SesName_) == sizeof(fon9_kCSTR_OmsForceInternal) - 1, "");
      return memcmp(this->SesName_.Chars_, fon9_kCSTR_OmsForceInternal, sizeof(fon9_kCSTR_OmsForceInternal) - 1) == 0;
   }

   fon9::CharAry<12> UserId_;
   fon9::CharAry<16> FromIp_;
   /// 來源別.
   fon9::CharAry<3>  Src_;

   /// 預計要使用的下單線路群組.
   /// 可能根據: IvacNo, User, Src, RxKind 設定, 必須在進入下單流程前填妥.
   /// 在設計券商系統時, 根據需求決定.
   LgOut LgOut_;

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
      fon9_GCC_WARN_DISABLE("-Winvalid-offsetof");
      static_assert(offsetof(Derived, SesName_) == offsetof(OmsRequestTrade, SesName_),
                    "'OmsRequestTrade' must be the first base class in derived.");
      fon9_GCC_WARN_POP;

      MakeFieldsImpl(flds);
   }

public:
   OmsRequestTrade(OmsRequestFactory& creator, f9fmkt_RxKind reqKind = f9fmkt_RxKind_Unknown)
      : base{creator, reqKind} {
   }
   OmsRequestTrade(f9fmkt_RxKind reqKind = f9fmkt_RxKind_Unknown)
      : base{reqKind} {
   }
   ~OmsRequestTrade();

   /// 收單程序, 可在建立 req 之後, 才設定是哪種下單要求.
   void SetRxKind(f9fmkt_RxKind reqKind) {
      this->RxKind_ = reqKind;
   }

   /// 請參閱「下單流程」文件.
   /// 在建立好 req 之後的 req 驗證程序, 此時還在 user thread, 尚未進入 OmsCore.
   /// 只有「下單要求」會呼叫此處, 「回報」不會呼叫此處.
   /// 預設: 傳回 true;
   virtual bool ValidateInUser(OmsRequestRunner&);

   void SetPolicy(OmsRequestPolicySP policy) {
      assert(this->Policy_.get() == nullptr);
      this->Policy_ = std::move(policy);
   }
   void ForceResetPolicy(OmsRequestPolicySP policy) {
      this->Policy_ = std::move(policy);
   }
   const OmsRequestPolicy* Policy() const {
      return this->Policy_.get();
   }

   /// 執行下單步驟的前置作業:
   /// - assert(runner.Request_.get() == this);
   /// - res.FetchRequestId(*this);
   /// - OmsRequestIni
   ///   - iniReq = this->BeforeReq_CheckOrdKey();
   ///   - iniReq->BeforeReq_CheckIvRight();
   ///   - 新單,   返回: this->Creator_->OrderFactory_->MakeOrder();
   ///   - 刪改查, 返回: iniReq->LastUpdated()->Order_->BeginUpdate(*this);
   /// - OmsRequestUpd 一般刪改查:
   ///   - iniReq = this->BeforeReq_GetInitiator();
   ///   - iniReq->BeforeReq_CheckIvRight();
   ///   - 返回: iniReq->LastUpdated()->Order().BeginUpdate(*this);
   /// - 如果返回 nullptr, 表示已呼叫 runner.RequestAbandon();
   /// - 返回後 request 不應再有異動.
   virtual OmsOrderRaw* BeforeReqInCore(OmsRequestRunner& runner, OmsResource& res) = 0;

   /// 請求遠端協助送單時, 將 OrderRaw 無法表達的欄位, 透過 Message 提供給遠端.
   /// 一般而言, 只有改單才需要, 例:
   /// - 本地(請求端) TwfChg, 改量填入: "|Qty=..."; 改價填入: "|Pri=...|PriType=...|TimeInForce=...";
   /// - 遠端(支援端) 將上述內容填入 TwfRpt,
   ///   配合送單程序 TwfTradingLineTmp::SendRequest() 對於 TwfRpt 的理解, 填入期交所的欄位.
   /// - 新單, 則直接使用 OrderRaw 的最後內容即可.
   /// - 預設: do nothing.
   virtual bool PutAskToRemoteMessage(OmsRequestRunnerInCore& runner, fon9::RevBuffer& rbuf) const;
   /// 解析 PutAskToRemoteMessage() 打包的訊息, 一般只有在 TwfRpt, TwsRpt... 才需要覆寫此函式.
   /// TwfChg.PutAskToRemoteMessage() => TwfRpt.ParseAskerMessage();
   virtual bool ParseAskerMessage(fon9::StrView message);
};

//--------------------------------------------------------------------------//

struct OmsRequestIniDat {
   IvacNo      IvacNo_;
   OmsSubacNo  SubacNo_;
   OmsSalesNo  SalesNo_;
   /// 新單有效時間, 超過時間則不送出.
   /// 從新單建立開始計算: >0:超過多少 ms 視為過期; =0:不檢查是否過期;
   uint16_t    VaTimeMS_;

   OmsRequestIniDat() {
      memset(this, 0, sizeof(*this));
   }
};

/// 當需要提供全部欄位時(例:新單要求、回報匯入), 使用此基底.
/// 也可用於「提供全部欄位」的「刪改查」, 但此時委託必須有 Initiator();
class OmsRequestIni : public OmsRequestTrade, public OmsRequestIniDat {
   fon9_NON_COPY_NON_MOVE(OmsRequestIni);
   using base = OmsRequestTrade;

   friend class OmsOrder; // OmsOrder 建構時需要呼叫 SetInitiatorFlag() 的權限.
   void SetInitiatorFlag() {
      this->RxItemFlags_ |= OmsRequestFlag_Initiator;
   }
   static void MakeFieldsImpl(fon9::seed::Fields& flds);

   /// 使用 runner.Request_.Policy()  檢查 runner.Request_ 是否有「this->IvacNo_、this->SubacNo_」的交易權限.
   OmsIvRight CheckIvRight(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) const;

   /// - 檢查 runner.Request_.Policy() 的權限是否允許 runner.Request_;
   /// - 應在 inireq = runner.Request_->BeforeReq_CheckOrdKey() 之後執行:
   ///   inireq->BeforeReq_CheckIvRight(runner...);
   /// - 若委託已存在(this 為刪改查要求), 則欄位(Ivr,Side,Symbol,...)必須正確.
   /// - 如果允許, 且不是因 admin 權限, 則會設定 scRes.Ivr_;
   bool BeforeReq_CheckIvRight(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) const;

protected:
   template <class Derived>
   static void MakeFields(fon9::seed::Fields& flds) {
      fon9_GCC_WARN_DISABLE("-Winvalid-offsetof");
      static_assert(offsetof(Derived, IvacNo_) == offsetof(OmsRequestIni, IvacNo_),
                    "'OmsRequestTrade' must be the first base class in derived.");
      fon9_GCC_WARN_POP;

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
   /// \retval nullptr 表示失敗, 返回前已呼叫 runner.RequestAbandon();
   /// \retval this    表示此筆為「新單要求」.
   /// \retval else    表示此筆為「刪改查要求」, 傳回要操作的「初始委託要求」.
   virtual const OmsRequestIni* BeforeReq_CheckOrdKey(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes);

   OmsOrderRaw* BeforeReqInCore(OmsRequestRunner& runner, OmsResource& res) override;

   /// - 通常是從 OmsRequestUpd::BeforeReqInCore(), 在找到 inireq 之後呼叫.
   /// - 檢查 runner.Request_.Policy() 的權限是否允許 runner.Request_;
   /// - 應在 iniReq = runner.Request_->BeforeReq_GetInitiator() 之後執行:
   ///   iniReq->BeforeReq_CheckIvRight(runner);
   bool BeforeReq_CheckIvRight(OmsRequestRunner& runner, OmsResource& res) const;
};

/// 複製 OmsRequestIni 基本資料, 及 Policy: dst.SetPolicy(src.Policy());
void CopyRequestIni(OmsRequestIni& dst, const OmsRequestIni& src);

static inline bool IsOverVaTimeMS(const fon9::fmkt::TradingRequest& req, fon9::TimeStamp now) {
   if (fon9_LIKELY(req.RxKind() == f9fmkt_RxKind_RequestNew)) {
      assert(dynamic_cast<const OmsRequestIni*>(&req) != nullptr);
      const OmsRequestIni* ini = static_cast<const OmsRequestIni*>(&req);
      if (fon9_UNLIKELY(ini->VaTimeMS_ > 0))
         return(now >= (ini->CrTime() + fon9::TimeInterval_Millisecond(ini->VaTimeMS_)));
   }
   return false;
}

class OmsRequestUpd : public OmsRequestTrade {
   fon9_NON_COPY_NON_MOVE(OmsRequestUpd);
   using base = OmsRequestTrade;

   OmsRxSNO IniSNO_{0};

   static void MakeFieldsImpl(fon9::seed::Fields& flds);
protected:
   template <class Derived>
   static void MakeFields(fon9::seed::Fields& flds) {
      fon9_GCC_WARN_DISABLE("-Winvalid-offsetof");
      static_assert(offsetof(Derived, IniSNO_) == offsetof(OmsRequestUpd, IniSNO_),
                    "'OmsRequestUpd' must be the first base class in derived.");
      fon9_GCC_WARN_POP;

      MakeFieldsImpl(flds);
   }

   /// this->BeforeReqInCore(); 呼叫此處取得 order;
   /// 衍生者也可呼叫此處取得 order, 然後檢查是否可繼續下單步驟, 或 runner.RequestAbandon();
   OmsOrder* BeforeReqInCore_GetOrder(OmsRequestRunner& runner, OmsResource& res);
   template <class OrderT, class OmsRequestRunner>
   OrderT* BeforeReqInCore_GetOrderT(OmsRequestRunner& runner, OmsResource& res) {
      if (auto* order = this->BeforeReqInCore_GetOrder(runner, res)) {
         if (fon9_LIKELY(dynamic_cast<OrderT*>(order) != nullptr))
            return static_cast<OrderT*>(order);
         runner.RequestAbandon(&res, OmsErrCode_RequestNotSupportThisOrder);
      }
      return nullptr;
   }

public:
   using base::base;
   OmsRequestUpd() = default;

   /// IniSNO() 可以視為 RefSNO,
   /// 對應的 req 不一定是 OmsRequestIni; 也可以是 OmsRequestUpd; 只要是 OmsRequestTrade 皆可;
   OmsRxSNO IniSNO() const {
      return this->IniSNO_;
   }
   void SetIniFrom(const OmsRequestTrade& ini) {
      assert(this->IniSNO_ == 0);
      this->IniSNO_ = ini.RxSNO();
      this->BrkId_ = ini.BrkId_;
   }
   /// 如果先前有呼叫過 SetIniFrom(), 則透過 IniSNO_ 取得 req->order->Initiator();
   /// 否則使用 OrdKey 取得 order->Initiator(); 並設定 IniSNO_;
   const OmsRequestIni* BeforeReq_GetInitiator(OmsRequestRunner& runner, OmsResource& res) {
      return base::BeforeReq_GetInitiator(runner, &this->IniSNO_, res);
   }

   OmsOrderRaw* BeforeReqInCore(OmsRequestRunner& runner, OmsResource& res) override;
   template <class OrderT>
   OmsOrderRaw* BeforeReqInCoreT(OmsRequestRunner& runner, OmsResource& res) {
      if (auto* order = this->BeforeReqInCore_GetOrderT<OrderT>(runner, res))
         return order->BeginUpdate(*this);
      return nullptr;
   }

   template <class RequestUpdT, class OmsRequestRunnerT>
   static bool RequestUpd_AutoRxKind(RequestUpdT& req, OmsRequestRunnerT& runner) {
      if (fon9_LIKELY(req.RxKind_ != f9fmkt_RxKind_Unknown))
         return true;
      if (req.PriType_ == f9fmkt_PriType{} && req.Pri_.IsNull())
         req.RxKind_ = (req.Qty_ == 0 ? f9fmkt_RxKind_RequestDelete : f9fmkt_RxKind_RequestChgQty);
      else if (req.Qty_ == 0)
         req.RxKind_ = f9fmkt_RxKind_RequestChgPri;
      else { // 不能同時「改價 & 改量」.
         runner.RequestAbandon(nullptr, OmsErrCode_Bad_RxKind);
         return false;
      }
      return true;
   }
   /// \retval true  OrderSt < f9fmkt_OrderSt_NewSending
   bool PreOpQueuingRequest(fon9::fmkt::TradingLineManager& from) const override;
};

} // namespaces
#endif//__f9omstw_OmsRequestTrade_hpp__
