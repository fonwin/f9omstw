// \file f9omstw/OmsRequestBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestBase_hpp__
#define __f9omstw_OmsRequestBase_hpp__
#include "f9omstw/OmsRequestId.hpp"
#include "f9omstw/OmsErrCode.h"
#include "fon9/seed/Tab.hpp"
#include "fon9/TimeStamp.hpp"

namespace f9omstw {

/// OmsRequestBase 定義 fon9::fmkt::TradingRxItem::RxItemFlags_;
enum OmsRequestFlag : uint8_t {
   OmsRequestFlag_Initiator = 0x01,

   /// 回報要求, 包含成交回報、券商回報、其他 f9oms 的回報...
   OmsRequestFlag_ReportIn = 0x02,

   /// 由建立 Report 的人決定: 如果 rpt 的 origReq 已存在, 是否要用 ExInfo 記錄 rpt?
   /// - 因為如果 origReq 已存在, 則「回報物件」在回報處理完畢後, 會被刪除, 不會留在 history 裡面.
   /// - 如果 runner.ExLog_ 已可完整記錄, 則不用此旗標.
   /// - 通常用於 Client 填入的回報補單?
   OmsRequestFlag_ReportNeedsLog = 0x08,

   /// 如果 RxSNO=0 (例如:有問題的Report), 是否需要發行給訂閱者?
   OmsRequestFlag_ForcePublish = 0x10,

   /// 無法進入委託流程: 無法建立 OmsOrder, 或找不到對應的 OmsOrder.
   OmsRequestFlag_Abandon = 0x80,
};
fon9_ENABLE_ENUM_BITWISE_OP(OmsRequestFlag);

/// 市場唯一編號:
/// - 需要再配合 fon9::fmkt::TradingRequest 裡面的 Market_; SessionId_;
/// - 才能組成完整的「市場委託唯一Key」:「BrkId-Market-SessionId-OrdNo」;
struct OmsOrdKey {
   OmsBrkId BrkId_;
   OmsOrdNo OrdNo_;

   OmsOrdKey() {
      memset(this, 0, sizeof(*this));
   }
};

/// 「新、刪、改、查、成交」的共同基底.
/// \code
///
///                           f9fmkt::TradingRxItem
///                            ↑                 ↑
/// TradingLineMgr             ↑                 ↑
///   ::SendRequest(f9fmkt::TradingRequest);     ↑
///                           ↑                  ↑
///                    OmsRequestBase           OmsOrderRaw
///                       ↑       ↑                     ↑
///           OmsRequestTrade   OmsReportFilled         ↑
///               ↑    ↑                  ↑             ↑
///    OmsRequestIni  OmsRequestUpd       ↑             ↑
///               ↑    ↑                  ↑             ↑
/// OmsTwsRequestIni  OmsTwsRequestChg    ↑            OmsTwsOrderRaw  (類推 Twf)
///               ↑                       ↑
///     OmsTwsReport                     OmsTwsFilled
///
/// \endcode
class OmsRequestBase : public fon9::fmkt::TradingRequest, public OmsRequestId, public OmsOrdKey {
   fon9_NON_COPY_NON_MOVE(OmsRequestBase);
   using base = fon9::fmkt::TradingRequest;

   /// 當 this 是回報物件, 則此處記錄回報的狀態.
   /// 如果 this 是下單物件, 則不必理會此處.
   f9fmkt_TradingRequestSt ReportSt_{};
   OmsErrCode              ErrCode_{OmsErrCode_NoError};

   friend class OmsBackend; // 取得修改 SetLastUpdated() 的權限.
   void SetLastUpdated(OmsOrderRaw& lastupd) const {
      this->LastUpdated_ = &lastupd;
   }

   union {
      /// 當 this->IsAbandoned(): 此時這裡記錄中斷要求的原因.
      /// 可能為 nullptr, 表示只提供 ErrCode_ 沒有額外訊息.
      std::string*         AbandonReason_;
      OmsOrderRaw mutable* LastUpdated_{nullptr};
   };
   fon9::TimeStamp      CrTime_;
   OmsRequestFactory*   Creator_;

   static void MakeFieldsImpl(fon9::seed::Fields& flds);

   /// 透過 this->OrdKey(BrkId+Market+SessionId+OrdNo)取出此筆要求要操作的委託書(OmsOrder).
   OmsOrder* SearchOrderByOrdKey(OmsResource& res) const;
   OmsOrder* SearchOrderByKey(OmsRxSNO srcSNO, OmsResource& res);

protected:
   template <class Derived>
   static void MakeFields(fon9::seed::Fields& flds) {
      fon9_GCC_WARN_DISABLE("-Winvalid-offsetof");
      static_assert(offsetof(Derived, RxKind_) == offsetof(OmsRequestBase, RxKind_),
                    "'OmsRequestBase' must be the first base class in derived.");
      fon9_GCC_WARN_POP;

      MakeFieldsImpl(flds);
   }
   /// 在 flds 增加 ReportSt 及 ErrCode 欄位, 這裡沒有呼叫 MakeFields().
   static void AddFieldsForReport(fon9::seed::Fields& flds);
   /// 在 flds 增加 ErrCode 欄位, 這裡沒有呼叫 MakeFields().
   static void AddFieldsErrCode(fon9::seed::Fields& flds);

   void InitializeForReportIn() {
      this->RxItemFlags_ |= OmsRequestFlag_ReportIn;
   }
   void MakeReportReqUID(fon9::DayTime exgTime, uint32_t beforeQty);
   void MakeReportReqUID(fon9::TimeStamp exgTime, uint32_t beforeQty) {
      MakeReportReqUID(fon9::GetDayTime(exgTime), beforeQty);
   }

   /// 執行「刪改查」下單步驟前, 取出此筆要求要操作的原始新單要求.
   /// - 如果有提供 pIniSNO 且 *pIniSNO != 0, 則 this.OrdKey 如果有填則必須正確, 如果沒填則自動填入正確值.
   ///   此時會先取得 IniSNO 的 request, 然後再透過該 request 取得 order.Initiator();
   /// - 如果沒提供 pIniSNO 或 *pIniSNO == 0, 則使用 this->GetOrderByOrdKey() 尋找.
   /// - 如果找不到, 則在返回前會呼叫 runner.RequestAbandon(OmsErrCode_OrderNotFound);
   const OmsRequestIni* BeforeReq_GetInitiator(OmsRequestRunner& runner, OmsRxSNO* pIniSNO, OmsResource& res);

public:
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
      this->Creator_ = &creator;
      this->CrTime_ = now;
   }
   /// 解構時 if (this->IsAbandoned()) 則刪除 AbandonReason_.
   ~OmsRequestBase();

   /// 這裡只是提供欄位的型別及名稱, 不應使用此欄位存取 RxSNO_;
   /// fon9_MakeField2_const(OmsRequestBase, RxSNO);
   static fon9::seed::FieldSP MakeField_RxSNO();

   /// 傳回 this;
   const OmsRequestBase* CastToRequest() const override;
   /// 使用 RevPrintFields() 輸出.
   void RevPrint(fon9::RevBuffer& rbuf) const override;

   OmsRequestFactory& Creator() const {
      assert(this->Creator_ != nullptr);
      return *this->Creator_;
   }

   fon9::TimeStamp CrTime() const {
      return this->CrTime_;
   }
   OmsRequestFlag RequestFlags() const {
      return static_cast<OmsRequestFlag>(this->RxItemFlags_);
   }
   /// 傳回 true 則表示此筆下單要求失敗, 沒有進入委託系統.
   /// 進入委託系統後的失敗會用 Reject 機制, 透過 OmsOrderRaw 處理.
   bool IsAbandoned() const {
      return (this->RxItemFlags_ & OmsRequestFlag_Abandon) == OmsRequestFlag_Abandon;
   }
   bool IsReportIn() const {
      return (this->RxItemFlags_ & OmsRequestFlag_ReportIn) == OmsRequestFlag_ReportIn;
   }
   /// 設定 OmsRequestFlag_ReportNeedsLog 旗標.
   void SetReportNeedsLog() {
      assert(this->IsReportIn());
      this->RxItemFlags_ |= OmsRequestFlag_ReportNeedsLog;
   }
   /// 設定 OmsRequestFlag_ForcePublish 旗標.
   void SetForcePublish() {
      this->RxItemFlags_ |= OmsRequestFlag_ForcePublish;
   }

   bool IsInitiator() const {
      return (this->RxItemFlags_ & OmsRequestFlag_Initiator) == OmsRequestFlag_Initiator;
   }

   /// 最後一次委託異動完畢後的內容.
   /// 若 OmsRequestRunnerInCore 正在處理, 則此處傳回的是「上一次異動」的內容,
   /// 若 this 為首次執行 OmsRequestRunnerInCore, 則會傳回 nullptr.
   const OmsOrderRaw* LastUpdated() const {
      return this->IsAbandoned() ? nullptr : this->LastUpdated_;
   }
   OmsErrCode ErrCode() const {
      return this->ErrCode_;
   }
   void SetErrCode(OmsErrCode ec) {
      this->ErrCode_ = ec;
   }
   /// 如果 this->IsAbandoned() 則傳回失敗時提供的訊息.
   /// - 可能為 nullptr, 表示只提供 ErrCode(), 沒提供額外訊息.
   const std::string* AbandonReason() const {
      return this->IsAbandoned() ? this->AbandonReason_ : nullptr;
   }
   void Abandon(OmsErrCode errCode) {
      assert((this->RxItemFlags_ & (OmsRequestFlag_Abandon | OmsRequestFlag_Initiator)) == OmsRequestFlag{});
      this->RxItemFlags_ |= OmsRequestFlag_Abandon;
      this->ErrCode_ = errCode;
   }
   void Abandon(OmsErrCode errCode, std::string reason) {
      this->Abandon(errCode);
      if (!reason.empty())
         this->AbandonReason_ = new std::string(std::move(reason));
   }

   void SetReportSt(f9fmkt_TradingRequestSt rptSt) {
      this->ReportSt_ = rptSt;
   }
   f9fmkt_TradingRequestSt ReportSt() const {
      return this->ReportSt_;
   }
   /// 排除重複回報之後, 透過這裡處理回報.
   /// - 預設呼叫 this->RunReportInCore_Start(std::move(checker));
   virtual void RunReportInCore(OmsReportChecker&& checker);

   /// 當委託有異動時, 在 OmsOrder::ProcessPendingReport(); 裡面:
   /// 若 this->LastUpdated()->OrderSt_ == f9fmkt_OrderSt_ReportPending;
   /// 則會透過這裡通知繼續處理回報.
   /// 預設 assert(!"Derived must override ProcessPendingReport()");
   virtual void ProcessPendingReport(OmsResource& res) const;

protected:
   void RunReportInCore_Start(OmsReportChecker&& checker);

   /// RunReportInCore_Start() 的過程:
   /// 有找到「原始下單要求」, 直接處理回報.
   /// 預設: do nothing: assert(!"應由衍生者完成.");
   virtual void RunReportInCore_FromOrig(OmsReportChecker&& checker, const OmsRequestBase& origReq);
   /// - 檢查 RxKind 是否與 origReq 一致.
   /// - 檢查是否重複回報: this->ReportSt() <= origReq.LastUpdated()->RequestSt_;
   /// - 返回 false: 表示已呼叫過 checker.ReportAbandon();
   /// - 返回 true: 表示檢查通過, 回報應正常處理.
   bool RunReportInCore_FromOrig_Precheck(OmsReportChecker& checker, const OmsRequestBase& origReq);

   /// RunReportInCore_Start() 的過程:
   /// 預設: checker.ReportAbandon("Report: OrdNo is empty.");
   virtual void RunReportInCore_OrdNoEmpty(OmsReportChecker&& checker);
   /// RunReportInCore_Start() 的過程:
   /// 預設 do nothing: assert(!"應由衍生者完成.");
   /// 實作建議: this->MakeReportReqUID(this->ExgTime_, this->BeforeQty_);
   virtual void RunReportInCore_MakeReqUID();

   /// Order 沒有 Initiator 時的新單回報.
   /// - RunReportInCore_NewOrder() 的新單回報;
   /// - RunReportInCore_Order() 的新單回報:
   ///   - order.Initiator() 有存在 透過 RunReportInCore_FromOrig() 處理.
   ///   - order.Initiator() 不存在 透過 RunReportInCore_InitiatorNew() 處理.
   /// - 預設 do nothing: assert(!"應由衍生者完成.");
   virtual void RunReportInCore_InitiatorNew(OmsReportRunnerInCore&& inCoreRunner);
   /// 一般的刪改查回報.
   /// - 已過濾了重複回報.
   /// - 預設 do nothing: assert(!"應由衍生者完成.");
   virtual void RunReportInCore_DCQ(OmsReportRunnerInCore&& inCoreRunner);

   /// 在 RunReportInCore_Order() 的刪改查回報:
   /// - 檢查 ordu 的 Before*, After* 是否與 this 相同.
   /// - 如果相同, 則 this 可能為重複回報, 接下來 RunReportInCore_Order() 還會檢查:
   ///   - RunReportInCore_IsExgTimeMatch() or RxKind==ChgQty or RxKind==Delete;
   ///   - 如果都相同, 則為重複回報, 會透過 this->RunReportInCore_FromOrig() 做後續處理.
   //      - 一般而言此時會在 this->RunReportInCore_FromOrig_Precheck();
   //        - 透過 checker.ReportAbandon() 記錄 log.
   //        - Order 不會有任何異動.
   /// - 預設傳回 false;
   virtual bool RunReportInCore_IsBfAfMatch(const OmsOrderRaw& ordu);
   virtual bool RunReportInCore_IsExgTimeMatch(const OmsOrderRaw& ordu);
   /// RunReportInCore_Start() 的過程:
   /// - 回報找不到「原始下單要求」, 但有找到「原始委託」.
   /// - this = 新單回報
   ///   - order.Initiator() 有存在 透過 RunReportInCore_FromOrig() 處理.
   ///   - order.Initiator() 不存在 透過 RunReportInCore_InitiatorNew() 處理.
   /// - this = 刪改查回報
   ///   - 檢查是否重複
   ///     - 使用 RunReportInCore_IsBfAfMatch()、RunReportInCore_IsExgTimeMatch()
   ///     - 及 ExgTime, RxKind...
   ///   - 如果有重複: this->RunReportInCore_FromOrig();
   ///   - 如果沒重複: this->RunReportInCore_DCQ();
   virtual void RunReportInCore_Order(OmsReportChecker&& checker, OmsOrder& order);

   /// RunReportInCore_Start() 的過程:
   /// 回報找不到「原始下單要求」, 也找不到「原始委託」, 預設:
   /// - 用 this->Creator().OrderFactory_ 建立新委託.
   /// - 用 RunReportInCore_NewOrder(runner) 處理.
   virtual void RunReportInCore_OrderNotFound(OmsReportChecker&& checker, OmsOrdNoMap& ordnoMap);

   /// 在 RunReportInCore_OrderNotFound() 時,
   /// 透過 factory 建立了新單要求, 透過這裡處理後續回報.
   /// - this = 新單回報: this->RunReportInCore_InitiatorNew(std::move(runner));
   /// - this = 刪改查回報: this->RunReportInCore_DCQ(std::move(runner), *this);
   virtual void RunReportInCore_NewOrder(OmsReportRunnerInCore&& runner);
};

} // namespaces
#endif//__f9omstw_OmsRequestBase_hpp__
