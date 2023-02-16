// \file f9omstw/OmsRequestBase.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestBase.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsRequestFactory.hpp"

namespace f9omstw {

void OmsRequestBase::MakeFieldsImpl(fon9::seed::Fields& flds) {
   flds.Add(fon9_MakeField(OmsRequestBase, RxKind_, "Kind"));
   flds.Add(fon9_MakeField2(OmsRequestBase, Market));
   flds.Add(fon9_MakeField2(OmsRequestBase, SessionId));
   flds.Add(fon9_MakeField2(OmsRequestBase, BrkId));
   flds.Add(fon9_MakeField2(OmsRequestBase, OrdNo));
   flds.Add(fon9_MakeField2(OmsRequestBase, ReqUID));
   flds.Add(fon9_MakeField2(OmsRequestBase, CrTime));
}
void OmsRequestBase::AddFieldsErrCode(fon9::seed::Fields& flds) {
   flds.Add(fon9_MakeField(OmsRequestBase, ErrCode_, "ErrCode"));
}
void OmsRequestBase::AddFieldsForReport(fon9::seed::Fields& flds) {
   using namespace fon9;
   using namespace fon9::seed;
   flds.Add(FieldSP{new FieldIntHx<underlying_type_t<f9fmkt_TradingRequestSt>>(Named{"ReportSt"}, fon9_OffsetOfRawPointer(OmsRequestBase, ReportSt_))});
   flds.Add(fon9_MakeField(OmsRequestBase, ErrCode_, "ErrCode"));
}
fon9::seed::FieldSP OmsRequestBase::MakeField_RxSNO() {
   return fon9_MakeField2_const(OmsRequestBase, RxSNO);
}
//--------------------------------------------------------------------------//
OmsRequestBase::~OmsRequestBase() {
   if (this->RxItemFlags_ & OmsRequestFlag_Abandon)
      delete this->AbandonReason_;
   //-----------------------------
   // 改放到 ~OmsOrderRaw() 處理;
   // else if (auto ordraw = this->LastUpdated()) {
   //    // 發現 order 的最後一個 ordraw 時, 刪除 order;
   //    // 但不能直接使用 order.Tail(); 因為有可能 order 已經死亡;
   //    // 所以使用 ordraw->Next() == nullptr 來判斷「最後一個 ordraw」;
   //    if (ordraw->Next() == nullptr)
   //       ordraw->Order().FreeThis();
   // }
   //-----------------------------
}
const OmsRequestBase* OmsRequestBase::CastToRequest() const {
   return this;
}
//--------------------------------------------------------------------------//
void OmsRequestBase::LogAbandon(OmsErrCode errCode, std::string reason, OmsResource& res, fon9::RevBufferList&& exLog) {
   assert((this->RxItemFlags_ & (OmsRequestFlag_Abandon | OmsRequestFlag_Initiator)) == OmsRequestFlag{});
   this->RxItemFlags_ |= OmsRequestFlag_Abandon;
   this->ErrCode_ = errCode;
   if (!reason.empty())
      this->AbandonReason_ = new std::string(std::move(reason));
   res.Backend_.LogAppend(*this, std::move(exLog));
   this->OnRequestAbandoned(&res);
}
void OmsRequestBase::Abandon(OmsErrCode errCode, OmsResource* res, fon9::RevBufferList* exLog) {
   assert((this->RxItemFlags_ & (OmsRequestFlag_Abandon | OmsRequestFlag_Initiator)) == OmsRequestFlag{});
   this->RxItemFlags_ |= OmsRequestFlag_Abandon;
   this->ErrCode_ = errCode;
   if (res)
      res->Backend_.Append(*this, std::move(*exLog));
   this->OnRequestAbandoned(res);
}
void OmsRequestBase::OnRequestAbandoned(OmsResource* res) {
   if (res) {
      if (auto* reqParent = this->GetParentRequest())
         reqParent->OnChildAbandoned(*this, *res);
   }
}
void OmsRequestBase::OnChildAbandoned(const OmsRequestBase& reqChild, OmsResource& res) const {
   (void)reqChild; (void)res;
}
//--------------------------------------------------------------------------//
void OmsRequestBase::SetParentRequest(const OmsRequestBase& parentRequest, OmsRequestRunnerInCore* parentRunner) {
   (void)parentRequest; (void)parentRunner;
   assert(!"Not support OmsRequestBase::SetParentRequest()");
}
void OmsRequestBase::SetParentReport(const OmsRequestBase* parentReport) {
   (void)parentReport;
}
const OmsRequestBase* OmsRequestBase::GetParentRequest() const {
   return nullptr;
}
OmsRxSNO OmsRequestBase::GetParentRequestSNO() const {
   return 0;
}
bool OmsRequestBase::IsParentRequest() const {
   return false;
}
void OmsRequestBase::ClearParentRequest() {
}
void OmsRequestBase::OnAddChildRequest(const OmsRequestBase& childReq, OmsRequestRunnerInCore* parentRunner) const {
   (void)childReq; (void)parentRunner;
}
void OmsRequestBase::OnAfterReloadFields(OmsResource& res) {
   if (auto parentSNO = this->GetParentRequestSNO())
      if (auto* parentRequest = res.GetRequest(parentSNO))
         this->SetParentRequest(*parentRequest, nullptr);
}
void OmsRequestBase::OnAfterBackendReload(OmsResource& res, void* backendLocker) const {
   if (auto* parentRequest = this->GetParentRequest())
      parentRequest->OnAfterBackendReloadChild(*this);
   if (this->IsReportIn()) // 如果是回報, 應該由原主機更新狀態, 不能因本機重啟而改變.
      return;
   auto* ordraw = this->LastUpdated();
   if (ordraw == nullptr) // Abandoned request.
      return;
   if (fon9_UNLIKELY(ordraw->RequestSt_ == f9fmkt_TradingRequestSt_Queuing
                  || ordraw->RequestSt_ == f9fmkt_TradingRequestSt_WaitingCond)) {
      auto& order = ordraw->Order();
      if (order.LastOrderSt() != f9fmkt_OrderSt_NewQueuingCanceled) {
         // 若 req 最後狀態為 Queueing, 則應改成 f9fmkt_TradingRequestSt_QueuingCanceled.
         // - 發生原因: 可能因為程式沒有正常結束: crash? kill -9?
         // - 不能強迫設定 const_cast<OmsOrderRaw*>(ordraw)->RequestSt_ = f9fmkt_TradingRequestSt_QueuingCanceled;
         //   應使用正常更新方式處理, 否則訂閱端(如果有保留最後 RxSNO), 可能會不知道該筆要求變成 QueuingCanceled.
         static_cast<OmsBackend::Locker*>(backendLocker)->unlock();
         if (OmsOrderRaw* ordupd = order.BeginUpdate(*this)) {
            OmsRequestRunnerInCore runner{res, *ordupd, 0u};
            runner.Reject(f9fmkt_TradingRequestSt_QueuingCanceled, OmsErrCode_NoReadyLine, "Lg=System restart");
         }
         static_cast<OmsBackend::Locker*>(backendLocker)->lock();
      }
   }
}
void OmsRequestBase::OnAfterBackendReloadChild(const OmsRequestBase& childReq) const {
   (void)childReq;
}
void OmsRequestBase::OnOrderUpdated(const OmsRequestRunnerInCore& runner) const {
   if (auto* reqParent = this->GetParentRequest())
      reqParent->OnChildRequestUpdated(runner);
   else if (OmsOrder* ordParent = runner.OrderRaw().Order().GetParentOrder())
      ordParent->OnChildOrderUpdated(runner);
}
void OmsRequestBase::OnChildRequestUpdated(const OmsRequestRunnerInCore& childRunner) const {
   (void)childRunner;
}
bool OmsRequestBase::MoveChildRunnerToCore(OmsRequestRunnerInCore& parentRunner, OmsRequestRunner&& childRunner) const {
   // parentRunner 有可能是成交回報(或手動刪單...)後的更新, 所以這個 assert() 不一定成立.
   // assert(this == &parentRunner.OrderRaw().Request());
   // ----- 建立 Parent/Child 的關聯.
   childRunner.Request_->SetParentRequest(*this, &parentRunner);
   // -----
   if (fon9_LIKELY(parentRunner.Resource_.Core_.MoveToCore(std::move(childRunner))))
      return true;
   this->OnMoveChildRunnerToCoreError(parentRunner, std::move(childRunner));
   assert(childRunner.Request_->IsAbandoned());
   parentRunner.Reject(f9fmkt_TradingRequestSt_InternalRejected, childRunner.Request_->ErrCode(),
                       childRunner.Request_->AbandonReasonStrView());
   return false;
}
void OmsRequestBase::OnMoveChildRunnerToCoreError(OmsRequestRunnerInCore& parentRunner, OmsRequestRunner&& childRunner) const {
   (void)parentRunner; (void)childRunner;
}
//--------------------------------------------------------------------------//
OmsOrder* OmsRequestBase::SearchOrderByOrdKey(OmsResource& res) const {
   if (const OmsBrk* brk = res.Brks_->GetBrkRec(ToStrView(this->BrkId_))) {
      if (const OmsOrdNoMap* ordNoMap = brk->GetOrdNoMap(*this))
         return ordNoMap->GetOrder(this->OrdNo_);
   }
   return nullptr;
}
OmsOrder* OmsRequestBase::SearchOrderByKey(OmsRxSNO srcSNO, OmsResource& res) {
   if (srcSNO == 0)
      return this->SearchOrderByOrdKey(res);

   const OmsRequestBase* srcReq = res.GetRequest(srcSNO);
   if (srcReq == nullptr)
      return nullptr;
   const OmsOrderRaw* lastUpdated = srcReq->LastUpdated();
   if (lastUpdated == nullptr)
      return nullptr;

   if (this->BrkId_.empty())
      this->BrkId_ = srcReq->BrkId_;
   else if (this->BrkId_ != srcReq->BrkId_)
      return nullptr;

   lastUpdated = lastUpdated->Order().Tail();
   if (OmsIsOrdNoEmpty(this->OrdNo_))
      this->OrdNo_ = lastUpdated->OrdNo_;
   else if (this->OrdNo_ != lastUpdated->OrdNo_)
      return nullptr;

   if (this->Market_ == f9fmkt_TradingMarket_Unknown)
      this->Market_ = srcReq->Market_;
   else if (this->Market_ != srcReq->Market_)
      return nullptr;

   if (this->SessionId_ == f9fmkt_TradingSessionId_Unknown)
      this->SessionId_ = srcReq->SessionId_;
   else if (this->SessionId_ != srcReq->SessionId_)
      return nullptr;

   return &lastUpdated->Order();
}
const OmsRequestIni* OmsRequestBase::BeforeReq_GetInitiator(OmsRequestRunner& runner, OmsRxSNO* pIniSNO, OmsResource& res) {
   assert(this->LastUpdated_ == nullptr && this->AbandonReason_ == nullptr);
   if (OmsOrder* order = this->SearchOrderByKey(pIniSNO ? *pIniSNO : 0, res)) {
      if (const OmsRequestIni* iniReq = order->Initiator()) {
         if (pIniSNO && *pIniSNO == 0)
            *pIniSNO = iniReq->RxSNO();
         return iniReq;
      }
      runner.RequestAbandon(&res, OmsErrCode_OrderInitiatorNotFound);
      return nullptr;
   }
   runner.RequestAbandon(&res, OmsErrCode_OrderNotFound);
   return nullptr;
}
//--------------------------------------------------------------------------//
void OmsRequestBase::MakeReportReqUID(fon9::DayTime exgTime, uint32_t beforeQty) {
   if (!OmsIsReqUIDEmpty(*this))
      return;
   this->ReqUID_.Chars_[0] = this->Market();
   this->ReqUID_.Chars_[1] = this->SessionId();
   size_t      szBrkId = this->BrkId_.size();
   const char* pBrkIdEnd = this->BrkId_.begin() + szBrkId;
   this->ReqUID_.Chars_[2] = szBrkId >= 2 ? *(pBrkIdEnd - 2) : '-';
   this->ReqUID_.Chars_[3] = szBrkId >= 1 ? *(pBrkIdEnd - 1) : '-';
   memcpy(this->ReqUID_.Chars_ + 4, this->OrdNo_.Chars_, sizeof(this->OrdNo_));
   if (this->RxKind() == f9fmkt_RxKind_RequestNew)
      return;
   // Kind[1] + Time[9(HHMMSSuuu)] + Bf[6(499000)]
   // => ReqUID 相同 => 視為重複
   // => (1) 機率極低 (2) 反正也難以分辨, 哪個才是交易所的最後結果.
   // => 同一個瞬間(相同時間)的多次改價.
   // => 同一個瞬間的刪改失敗(Bf=0).
   this->ReqUID_.Chars_[4 + sizeof(this->OrdNo_)] = this->RxKind();
   char   buf[sizeof(OmsRequestId) * 2];
   char*  pout = buf + sizeof(OmsRequestId);
   memset(pout, 0, sizeof(OmsRequestId));
   pout = fon9::ToStrRev(pout, beforeQty);
   if (exgTime.IsNull())
      *--pout = ':';
   else {
      auto tms = fon9::unsigned_cast(exgTime.ShiftUnit<3>());
      auto sec = (tms / 1000);
      tms = (tms % 1000)
         + ((sec % 60)
            + ((sec / 60) % 60) * 100
            + ((sec / 60 / 60) % 24) * 10000) * 1000;
      pout = fon9::Pic9ToStrRev<9>(pout, tms);
   }
   memcpy(this->ReqUID_.Chars_ + 4 + sizeof(this->OrdNo_) + 1, pout,
          sizeof(this->ReqUID_) - (4 + sizeof(this->OrdNo_) + 1));
}
void OmsRequestBase::ProcessPendingReport(const OmsRequestRunnerInCore& prevRunner) const {
   (void)prevRunner;
   assert(!"Derived must override ProcessPendingReport()");
}
void OmsRequestBase::OnSynReport(const OmsRequestBase* ref, fon9::StrView message) {
   (void)message;
   if (ref) {
      if (OmsIsOrdNoEmpty(this->OrdNo_)) {
         *static_cast<OmsOrdKey*>(this) = *ref;
         if (ref->LastUpdated()) {
            // ref 有可能尚未編制 OrdNo, 在後續的回報才編入 OrdNo,
            // 所以, 若有需要, 需從 ref 的最後異動取得 OrdNo.
            this->OrdNo_ = ref->LastUpdated()->OrdNo_;
         }
      }
      else { // this 已有 OrdNo, 從 ref 複製複製而來的 OrdKey 就需要排除 OrdNo;
         this->AssignOrdKeyExcludeOrdNo(*ref);
      }
      this->Market_ = ref->Market();
      this->SessionId_ = ref->SessionId();
      if (this->RxKind_ == f9fmkt_RxKind_Unknown)
         this->RxKind_ = ref->RxKind();
      if (this->RxKind() == ref->RxKind() && ref->RxSNO() > 0) {
         // this = 針對 ref 的回報.
         OmsForceAssignReportReqUID(*this, ref->RxSNO());
      }
      else if (OmsIsReqUIDEmpty(*this)) {
         *static_cast<OmsRequestId*>(this) = *ref;
      }
   }
}
void OmsRequestBase::SetOrigHostId(fon9::HostId origHostId) {
   (void)origHostId;
}
fon9::HostId OmsRequestBase::OrigHostId() const {
   return 0;
}
//--------------------------------------------------------------------------//
void OmsRequestBase::RunReportInCore(OmsReportChecker&& checker) {
   this->RunReportInCore_Start(std::move(checker));
}
void OmsRequestBase::RunReportInCore_Start(OmsReportChecker&& checker) {
   assert(checker.Report_.get() == this);
   if (const OmsRequestBase* origReq = checker.SearchOrigRequestId()) {
      this->RunReportInCore_FromOrig(std::move(checker), *origReq);
      return;
   }
   // checker.SearchOrigRequestId(); 失敗,
   // 若已呼叫過 checker.ReportAbandon(), 則直接結束.
   if (checker.CheckerSt_ == OmsReportCheckerSt::Abandoned)
      return;
   // ReqUID 找不到原下單要求, 使用 OrdKey 來找.
   if (OmsIsOrdNoEmpty(this->OrdNo_)) {
      this->RunReportInCore_OrdNoEmpty(std::move(checker));
      return;
   }
   OmsOrdNoMap* ordnoMap = checker.GetOrdNoMap();
   if (ordnoMap == nullptr) // checker.GetOrdNoMap(); 失敗.
      return;               // 已呼叫過 checker.ReportAbandon(), 所以直接結束.

   if (OmsIsReqUIDEmpty(*this))
      this->RunReportInCore_MakeReqUID();

   if (OmsOrder* order = ordnoMap->GetOrder(this->OrdNo_))
      this->RunReportInCore_Order(std::move(checker), *order);
   else
      this->RunReportInCore_OrderNotFound(std::move(checker), *ordnoMap);
}
bool OmsRequestBase::RunReportInCore_FromOrig_Precheck(OmsReportChecker& checker, const OmsRequestBase& origReq) {
   if (fon9_UNLIKELY(this->RxKind() == f9fmkt_RxKind_Unknown))
      this->RxKind_ = origReq.RxKind();
   else if (fon9_UNLIKELY(this->RxKind_ != origReq.RxKind())) {
      if (this->RxKind_ == f9fmkt_RxKind_RequestDelete) {
         if (origReq.RxKind() == f9fmkt_RxKind_RequestChgQty) {
            // origReq 是改量, 但回報是刪單: 有可能發生,
            // 因為改量要求如果會讓剩餘量為0; 則下單時可能會用「刪單訊息」.
            goto __RX_KIND_OK;
         }
      }
      assert(this->RxKind_ == origReq.RxKind()); // _DEBUG 時直接死在這裡. RELEASE build 則僅記錄在 omslog.
      checker.ReportAbandon("Report: RxKind not match orig request.");
      return false;
   __RX_KIND_OK:;
   }
   if (fon9_LIKELY(checker.CheckerSt_ == OmsReportCheckerSt::NotReceived))
      return true;
   // 不確定是否有收過此回報: 檢查是否重複回報.
   auto* lastUpdated = origReq.LastUpdated();
   if (fon9_UNLIKELY(this->ReportSt() <= lastUpdated->RequestSt_)) {
      checker.ReportAbandon("Report: Duplicate or Obsolete report.");
      return false;
   }
   // Order 在此筆回報之前尚未編制 OrdNo, 此次回報提供了 Order 的 [委託書號].
   // 這種情況會在 OmsReportRunnerInCore::UpdateReportImpl() 將 OrdNo 填入 ordraw;
   if (OmsIsOrdNoEmpty(lastUpdated->Order().Tail()->OrdNo_)) {
      if (!OmsIsOrdNoEmpty(checker.Report_->OrdNo_)) {
         if (OmsOrdNoMap* ordnoMap = checker.GetOrdNoMap()) {
            if (ordnoMap->EmplaceOrder(checker.Report_->OrdNo_, &lastUpdated->Order()))
               return true;
         }
         checker.ReportAbandon("Report: Bad rpt OrdNo.");
         return false;
      }
   }
   // origReq 已存在的回報, 不用考慮 BeforeQty, AfterQty 重複? 有沒有底下的可能?
   // => f9oms.A => TwsRpt.1:Sending         => f9oms.Local (TwsRpt.1:Sending)
   // => f9oms.A => TwsRpt.1:Bf=10,Af=9
   //            => BrokerHost(C:Bf=10,Af=9) => f9oms.Local (TwsRpt.C:Bf=10,Af=9)
   //                                           因無法識別此回報為 TwsRpt.1 所以建立新的 TwsRpt.C
   //            => TwsRpt.1:Bf=10,Af=9      => f9oms.Local (TwsRpt.1:Bf=10,Af=9)
   //                                           此時收到的為重複回報!
   // => 為了避免發生此情況, 這裡要再一次強調(已於 ReportIn.md 說明):
   // * 如果券商主機的回報格式, 可以辨別下單要求來源是否為 f9oms, 則可將其過濾, 只處理「非 f9oms」的回報.
   // * 如果無法辨別, 則 f9oms 之間「不可以」互傳回報.
   return true;
}
void OmsRequestBase::RunReportInCore_FromOrig(OmsReportChecker&& checker, const OmsRequestBase& origReq) {
   (void)checker; (void)origReq;
   assert(!"Derived must override RunReportInCore_FromOrig()");
}
void OmsRequestBase::RunReportInCore_MakeReqUID() {
   assert(!"Derived must override RunReportInCore_MakeReqUID()");
}
void OmsRequestBase::RunReportInCore_InitiatorNew(OmsReportRunnerInCore&& inCoreRunner) {
   (void)inCoreRunner;
   assert(!"Derived must override RunReportInCore_InitiatorNew()");
}
void OmsRequestBase::RunReportInCore_DCQ(OmsReportRunnerInCore&& inCoreRunner) {
   (void)inCoreRunner;
   assert(!"Derived must override RunReportInCore_DCQ()");
}
void OmsRequestBase::RunReportInCore_OrdNoEmpty(OmsReportChecker&& checker) {
   // if (this->RxKind() == f9fmkt_RxKind_RequestNew) {
   //    // TODO: 沒有委託書號的回報, 如何取得 origReq? 如何判斷重複?
   //    //    - 建立 ReqUID map?
   //    // or - f9oms 首次異動就必須提供 OrdNo (即使是 Queuing), 那 Abandon request 呢(不匯入?)?
   //    //      沒有提供 OrdNo 就拋棄此次回報?
         checker.ReportAbandon("Report: OrdNo is empty.");
   //    return;
   // }
   // checker.ReportAbandon("Report: OrdNo is empty, but kind isnot 'N'");
}
bool OmsRequestBase::RunReportInCore_IsBfAfMatch(const OmsOrderRaw& ordu) {
   (void)ordu;
   assert(!"Derived must override RunReportInCore_IsBfAfMatch()");
   return false;
}
bool OmsRequestBase::RunReportInCore_IsExgTimeMatch(const OmsOrderRaw& ordu) {
   (void)ordu;
   assert(!"Derived must override RunReportInCore_IsExgTimeMatch()");
   return false;
}
void OmsRequestBase::RunReportInCore_Order(OmsReportChecker&& checker, OmsOrder& order) {
   assert(order.Head() != nullptr);
   if (this->RxKind() == f9fmkt_RxKind_RequestNew) {
      if (auto origReq = order.Initiator())
         this->RunReportInCore_FromOrig(std::move(checker), *origReq);
      else
         this->RunReportInCore_InitiatorNew(OmsReportRunnerInCore{std::move(checker), *order.BeginUpdate(*this)});
      return;
   }
   // 檢查 this 是否為重複回報(曾經有過相同的 BeforeQty, AfterQty),
   const OmsOrderRaw* ordu = order.Head();
   do {
      const OmsRequestBase& requ = ordu->Request();
      if ((requ.RxKind() == this->RxKind() || this->RxKind() == f9fmkt_RxKind_Unknown)
          && this->RunReportInCore_IsBfAfMatch(*ordu)) {
         if (requ.RxKind() == f9fmkt_RxKind_RequestDelete
             || requ.RxKind() == f9fmkt_RxKind_RequestChgQty
             || this->RunReportInCore_IsExgTimeMatch(*ordu)) {
            this->RunReportInCore_FromOrig(std::move(checker), requ);
            return;
         }
      }
   } while ((ordu = ordu->Next()) != nullptr);
   // order 裡面沒找到 origReq: this = 刪改查回報.
   if (this->RxKind_ == f9fmkt_RxKind_Unknown)
      checker.ReportAbandon("ReportOrder: Unknown report kind.");
   else
      this->RunReportInCore_DCQ(OmsReportRunnerInCore{std::move(checker), *order.BeginUpdate(*this)});
}
void OmsRequestBase::RunReportInCore_OrderNotFound(OmsReportChecker&& checker, OmsOrdNoMap& ordnoMap) {
   if (this->RxKind_ == f9fmkt_RxKind_Unknown) {
      checker.ReportAbandon("ReportOrderNotFound: Unknown report kind.");
      return;
   }
   assert(this->Creator().OrderFactory_.get() != nullptr);
   if (OmsOrderFactory* ordfac = this->Creator().OrderFactory_.get()) {
      OmsReportRunnerInCore inCoreRunner{std::move(checker), *ordfac->MakeOrder(*this, nullptr)};
      this->RunReportInCore_NewOrder(std::move(inCoreRunner));
      ordnoMap.EmplaceOrder(inCoreRunner.OrderRaw());
   }
   else
      checker.ReportAbandon("Report: No OrderFactory.");
}
void OmsRequestBase::RunReportInCore_NewOrder(OmsReportRunnerInCore&& runner) {
   if (this->RxKind() == f9fmkt_RxKind_RequestNew)
      this->RunReportInCore_InitiatorNew(std::move(runner));
   else // 委託不存在(建立新委託後)的刪改查回報.
      this->RunReportInCore_DCQ(std::move(runner));
}

} // namespaces
