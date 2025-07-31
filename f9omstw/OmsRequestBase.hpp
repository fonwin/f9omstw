// \file f9omstw/OmsRequestBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestBase_hpp__
#define __f9omstw_OmsRequestBase_hpp__
#include "f9omstw/OmsRequestId.hpp"
#include "f9omstw/OmsErrCode.h"
#include "fon9/seed/Tab.hpp"
#include "fon9/TimeStamp.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { class RevBufferList; }

namespace f9omstw {

/// OmsRequestBase 定義 fon9::fmkt::TradingRxItem::RxItemFlags_;
enum OmsRequestFlag : uint8_t {
   OmsRequestFlag_Initiator = 0x01,

   /// 回報要求, 包含成交回報、券商回報、其他 f9oms 的回報...
   OmsRequestFlag_ReportIn = 0x02,
   /// 從遠端同步(RcSyn模組)而來的回報, TwfRpt、TwsRpt、TwsFil、TwfFil… 都會設定此旗標
   OmsRequestFlag_IsSynReport = 0x04,

   /// 由建立 Report 的人決定: 如果 rpt 的 origReq 已存在, 是否要用 ExInfo 記錄 rpt?
   /// - 因為如果 origReq 已存在, 則「回報物件」在回報處理完畢後, 會被刪除, 不會留在 history 裡面.
   /// - 如果 runner.ExLog_ 已可完整記錄, 則不用此旗標.
   /// - 通常用於 Client 填入的回報補單?
   OmsRequestFlag_ReportNeedsLog = 0x08,

   /// 如果 RxSNO=0 (例如:有問題的Report), 是否需要發行給訂閱者?
   OmsRequestFlag_ForcePublish = 0x10,

   /// - 強制設定為 [內部要求], 可能包含 [下單要求的Report] 或 [成交回報 OmsReportFilled].
   ///   例: [備援主機] 接手 [死亡主機] 的交易線路, 從此線路收到的回報, 應視為 [內部要求].
   /// - 因為此旗標不會儲存, 所以除了設定此旗標外, 還必須設定會儲存的欄位:
   ///   - [下單要求的Report] 的 SesName: fon9_kCSTR_OmsForceInternal; 由回報接收處理程序自行填入此欄位;
   ///   - OmsReportFilled 的 ReqUID: "I:原本規則"; 
   ///     在 OmsReportFilled::RunReportInCore_MakeReqUID() 根據此旗標在 ReqUID 前方加入 "I:";
   ///     此旗標僅是為了協助處理上述欄位的填寫.
   OmsRequestFlag_ForceInternal = 0x20,

   /// 尚未尚未進行處理的 Request,
   /// 由建立 Request 的地方自行 設定/清除 此旗標;
   /// 一旦 Request 進入 OmsCore, 就應該清除此旗標;
   OmsRequestFlag_NotYetToCore = 0x40,

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
      fon9::ForceZeroNonTrivial(this);
   }
   void AssignOrdKeyExcludeOrdNo(const OmsOrdKey& rhs) {
      this->BrkId_ = rhs.BrkId_;
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
   OmsErrCode              ErrCode_{};

   friend class OmsBackend; // 取得修改 SetLastUpdated() 的權限.
   void SetLastUpdated(OmsOrderRaw& lastupd) const;
   union {
      /// 當 this->IsAbandoned(): 此時這裡記錄中斷要求的原因.
      /// 可能為 nullptr, 表示只提供 ErrCode_ 沒有額外訊息.
      std::string*                              AbandonReason_;
      const fon9::fmkt::TradingRxItem mutable*  LastUpdatedOrReportRef_{nullptr};
   };
   fon9::TimeStamp      CrTime_;
   OmsRequestFactory*   Creator_;

   static void MakeFieldsImpl(fon9::seed::Fields& flds);

   /// - 若 srcSNO==0, 則使用 this->SearchOrderByOrdKey(); 尋找原始委託.
   /// - 若 srcSNO!=0, 則用 srcSNO 尋找原始委託, 若有找到, 則:
   ///   - 若 BrkId_, OrdNo_, Market_, SessionId_ 其中有沒填的, 則填入 src 的內容.
   ///   - 若 BrkId_, OrdNo_, Market_, SessionId_ 與 src 不同, 則返回 nullptr;
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

   void ResetCrTime(fon9::TimeStamp crTime) {
      this->CrTime_ = crTime;
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
   void InitializeForReportIn() {
      this->RxItemFlags_ |= OmsRequestFlag_ReportIn;
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

   void SetForceInternal() {
      this->RxItemFlags_ |= OmsRequestFlag_ForceInternal;
   }
   bool HasForceInternalFlag() const {
      return(this->RxItemFlags_ & OmsRequestFlag_ForceInternal) == OmsRequestFlag_ForceInternal;
   }

   void SetNotYetToCore() {
      this->RxItemFlags_ |= OmsRequestFlag_NotYetToCore;
   }
   void ClearNotYetToCore() {
      this->RxItemFlags_ = static_cast<OmsRequestFlag>(this->RxItemFlags_ & ~OmsRequestFlag_NotYetToCore);
   }
   bool IsNotYetToCore() const {
      if (this->IsAbandoned())
         return false;
      if (this->LastUpdatedOrReportRef_ && this->LastUpdatedOrReportRef_->RxKind() == f9fmkt_RxKind_Order)
         return false;
      return (this->RxItemFlags_ & OmsRequestFlag_NotYetToCore) == OmsRequestFlag_NotYetToCore;
   }

   /// 透過 this->OrdKey(BrkId+Market+SessionId+OrdNo)取出此筆要求要操作的委託書(OmsOrder).
   /// - 一般而言應使用 this->LastUpdated()->Order() 取得 OmsOrder.
   /// - 只有在 this->LastUpdated() == nullptr 時, 才應該呼叫此處.
   OmsOrder* SearchOrderByOrdKey(OmsResource& res) const;

   /// 最後一次委託異動完畢後的內容.
   /// 若 OmsRequestRunnerInCore 正在處理, 則此處傳回的是「上一次異動」的內容,
   /// 若 this 為首次執行 OmsRequestRunnerInCore, 則會傳回 nullptr.
   const OmsOrderRaw* LastUpdated() const;
   const OmsRequestBase* ReportRef() const;
   OmsOrder* Order() const {
      return GetRequestOrder(this);
   }
   friend OmsOrder* GetRequestOrder(const OmsRequestBase* pthis);

   OmsErrCode ErrCode() const {
      return this->ErrCode_;
   }
   void SetErrCode(OmsErrCode ec) {
      this->ErrCode_ = ec;
   }
   /// 不論現在的 ErrCode 內容, 強制設定 this->ErrCode_ = this->GetOkErrCode();
   void ResetOkErrCode() {
      this->ErrCode_ = this->GetOkErrCode();
   }
   /// 取得若交易所下單成功, 應使用的訊息代碼,
   /// 例: 新單成功(6=NewOrderOK), 改量成功(8=ChgQtyOK)...
   /// 預設: 根據 RxKind() 決定 ErrCode_ 的值; 若為不認識的 RxKind(), 則返回 this->ErrCode_;
   virtual OmsErrCode GetOkErrCode() const;

   /// 如果 this->IsAbandoned() 則傳回失敗時提供的訊息.
   /// - 可能為 nullptr, 表示只提供 ErrCode(), 沒提供額外訊息.
   const std::string* AbandonReason() const {
      return this->IsAbandoned() ? this->AbandonReason_ : nullptr;
   }
   fon9::StrView AbandonReasonStrView() const {
      if (auto* abandonReason = this->AbandonReason())
         return fon9::StrView{abandonReason};
      return fon9::StrView{nullptr};
   }
   /// - 若有提供 res: res->Backend_.Append(*this, std::move(*exLog));
   /// - 返回前呼叫 this->OnRequestAbandon(res);
   void Abandon(OmsErrCode errCode, OmsResource* res, fon9::RevBufferList* exLog);
   void Abandon(OmsErrCode errCode, std::string reason, OmsResource* res, fon9::RevBufferList* exLog) {
      assert(this->AbandonReason_ == nullptr); // Abandon() 不可重覆呼叫.
      if (!reason.empty())
         this->AbandonReason_ = new std::string(std::move(reason));
      this->Abandon(errCode, res, exLog);
   }
   /// - 失敗的下單要求.
   /// - 沒有設定 RxSNO, 且不用設定 RxSNO.
   /// - res.Backend_.LogAppend(*this, std::move(exLog));
   /// - 返回前呼叫 this->OnRequestAbandon(&res);
   void LogAbandon(OmsErrCode errCode, std::string reason, OmsResource& res, fon9::RevBufferList&& exLog);

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
   virtual void ProcessPendingReport(const OmsRequestRunnerInCore& prevRunner) const;

   /// 從另一 oms 同步而來的回報, 呼叫時機: 已填入回報來源提供的欄位之後.
   /// - 在此應從 ref 複製所需欄位.
   /// - 設定額外來源沒有提供的欄位, 例如:
   ///   TwsRpt.QtyStyle_; TwsRpt.Message_; ...
   /// - 預設, 從 ref 複製:
   ///   - OmsOrdKey; Market; SessionId;
   ///   - OmsRequestId: 如果 OmsIsReqUIDEmpty(*this);
   ///   - RxKind: 如果 this->RxKind_ == f9fmkt_RxKind_Unknown;
   virtual void OnSynReport(const OmsRequestBase* ref, fon9::StrView message);
   void SetSynReport() {
      this->RxItemFlags_ |= OmsRequestFlag_IsSynReport;
   }
   bool IsSynReport() const {
      return (this->RxItemFlags_ & OmsRequestFlag_IsSynReport) == OmsRequestFlag_IsSynReport;
   }

   /// 設定/取得 [原始收單主機Id].
   /// - 在 RcSyn 收到回報時填入.
   /// - 在 SeedCommand [啟用 ParentOrder] 時會用到;
   /// - 不儲存, 重啟後歸0: 因為系統重啟前, Parent/Child 的狀態都無法正確更新, 所以重啟後不可以啟用 ParentOrder.
   /// - 由 OmsParentRequestIni 實作, 預設 do nothing.
   virtual void SetOrigHostId(fon9::HostId origHostId);
   /// - 由 OmsParentRequestIni 實作, 預設返回 0.
   virtual fon9::HostId OrigHostId() const;

   // ----- 底下為 [子母單機制] 的支援 {
   /// 當 this 為子單回報(有 ParentRequestSNO 欄位).
   /// - 則收到此筆子單的人(例:RcSyn), 有必要找到此回報原本的母單, 然後透過此處設定.
   /// - 不可直接呼叫 SetParentRequest(), 因為其必須在 Reload or CoreThread(母單建立子單時) 之中執行,
   ///   如果在回報接收程序裡直接呼叫 SetParentRequest() 無法保證 thread safe.
   /// - 預設: do nothing, 實際可參考 OmsChildRequestT<>::SetParentReport() 的做法及說明.
   virtual void SetParentReport(const OmsRequestBase* parentReport);
   /// assert(!"Not support");
   virtual void SetParentRequest(const OmsRequestBase& parentRequest, OmsRequestRunnerInCore* parentRunner);
   /// 預設返回 nullptr;
   virtual const OmsRequestBase* GetParentRequest() const;
   /// 預設返回 0;
   virtual OmsRxSNO GetParentRequestSNO() const;
   /// 預設返回 false;
   virtual bool IsParentRequest() const;
   /// 在 childReq.SetParentRequest() 時, 主動通知.
   /// 預設 do nothing.
   virtual void OnAddChildRequest(const OmsRequestBase& childReq, OmsRequestRunnerInCore* parentRunner) const;
   /// - 當收到遠端的 Request, 且有提供 ParentRequestSNO(遠端的), 但找不到 [對應本機的 ParentRequest],
   ///   則必須清除 ParentRequestSNO, 避免重啟時找錯 Parent;
   /// - 當 Request 尚未進入 RunInCore(), 就要求刪除子單, 此時應先解除與 Parent 的關聯, 然後呼叫 Abandon();
   /// - 預設: do nothing;
   virtual void ClearParentRequest();
   /// 當 Backend.Reload 會呼叫此處, 此時僅將欄位重新載入完畢, 尚未處理 Abandon.
   /// - 預設: 若 this->GetParentRequestSNO() 有效,
   ///   則在此呼叫 this->SetParentRequest(); 重建與 Parent 的關聯.
   virtual void OnAfterReloadFields(OmsResource& res);
   /// 當 Backend 已將全部資料載入完畢, 全部的 req 會依序呼叫此處.
   /// 預設:
   /// - 通知 parentRequest->OnAfterBackendReloadChild(*this);
   /// - 調整 RequestSt_ == Queuing or WaitingCond; => QueuingCanceled;
   ///    調整前需要先 backendLocker.unlock(); 調整後需要重新 backendLocker.lock();
   virtual void OnAfterBackendReload(OmsResource& res, void* backendLocker) const;
   /// 當 Backend 已將全部資料載入完畢, 則全部的 childReq, 會依序呼叫此處通知 parentReq.
   /// 預設 do nothing.
   virtual void OnAfterBackendReloadChild(const OmsRequestBase& childReq) const;
   /// 當委託有設定 OmsOrderFlag::IsNeedsOnOrderUpdated 旗標,
   /// 則在 OmsOrder::OnOrderUpdated() 時, 會通知 Request;
   /// - 預設, 若有提供 runner:
   ///   - 且有 reqParent = this->GetParentRequest();
   ///     則呼叫: reqParent->OnChildRequestUpdated(*runner);
   ///   - 若沒有 reqParent, 但有 ParentOrder, 則呼叫 ParentOrder.OnChildOrderUpdated();
   ///     - 這種情況為 [手動刪改子單] or [成交回報]
   virtual void OnOrderUpdated(const OmsRequestRunnerInCore& runner) const;
   /// 子單建立完畢後, 透過這裡執行.
   /// \retval false: MoveToCore() 失敗, 返回前會先呼叫 this->OnMoveChildRunnerToCoreError();
   bool MoveChildRunnerToCore(OmsRequestRunnerInCore& parentRunner, OmsRequestRunner&& childRunner) const;
protected:
   /// - 在設定 this->Abandon(errCode); 完成, 返回前, 通知衍生者.
   /// - res==nullptr 表示 this 為系統重載時, 重建的失敗要求.
   /// - 若 res 有效, 且有 reqParent = this->GetParentRequest();
   ///   則呼叫: reqParent->OnChildAbandoned(*this, *res);
   virtual void OnRequestAbandoned(OmsResource* res);
   /// 預設: do nothing;
   virtual void OnChildAbandoned(const OmsRequestBase& reqChild, OmsResource& res) const;
   /// 來自 this 衍生出的子單的回報.
   /// 預設: do nothing;
   virtual void OnChildRequestUpdated(const OmsRequestRunnerInCore& childRunner) const;
   /// 預設: do nothing;
   virtual void OnMoveChildRunnerToCoreError(OmsRequestRunnerInCore& parentRunner, OmsRequestRunner&& childRunner) const;
   // ----- 以上為 [子母單機制] 的支援 };

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
   virtual bool RunReportInCore_IsBfAfMatch(const OmsOrderRaw& ordu) const;
   virtual bool RunReportInCore_IsExgTimeMatch(const OmsOrderRaw& ordu) const;
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

/// 重啟時, 若下單要求為 WiatingCond, 且委託尚有剩餘量;
/// true  = 下單要求狀態不變(且保留下單數量), 但設定 ErrCode = OmsErrCode_SystemRestart_StopWaiting;
/// false = 拒絕該下單要求(預設);
extern bool IsReloadKeepWaitingRequest;

} // namespaces
#endif//__f9omstw_OmsRequestBase_hpp__
