// \file f9omstw/OmsReportFilled.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsReportFilled.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "f9omstw/OmsReportFactory.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsReportFilled::MakeFieldsImpl(fon9::seed::Fields& flds) {
   flds.Add(fon9_MakeField2(OmsReportFilled, IniSNO));
   flds.Add(fon9_MakeField2(OmsReportFilled, MatchKey));
   base::MakeFields<OmsReportFilled>(flds);
}
const OmsReportFilled* OmsReportFilled::Insert(const OmsReportFilled** ppHead,
                                               const OmsReportFilled** ppLast,
                                               const OmsReportFilled* curr) {
   assert(curr->Next_ == nullptr);
   if (const OmsReportFilled* chk = *ppLast) {
      assert(*ppHead != nullptr);
      if (fon9_LIKELY(chk->MatchKey_ < curr->MatchKey_)) { // 加到串列尾.
         *ppLast = curr;
         chk->InsertAfter(curr);
      }
      else if (curr->MatchKey_ < (chk = *ppHead)->MatchKey_) { // 加到串列頭.
         *ppHead = curr;
         curr->Next_ = chk;
      }
      else { // 加到串列中間.
         for (;;) {
            if (chk->MatchKey_ == curr->MatchKey_)
               return chk;
            assert(chk->Next_ != nullptr);
            if (chk->MatchKey_ < curr->MatchKey_ && curr->MatchKey_ < chk->Next_->MatchKey_) {
               chk->InsertAfter(curr);
               break;
            }
            chk = chk->Next_;
         }
      }
   }
   else {
      assert(*ppHead == nullptr);
      *ppHead = *ppLast = curr;
   }
   return nullptr;
}
const OmsReportFilled* OmsReportFilled::Find(const OmsReportFilled* pHead, const OmsReportFilled* pLast, MatchKey matchKey) {
   if (pLast == nullptr || pLast->MatchKey_ < matchKey)
      return nullptr;
   assert(pHead != nullptr);
   do {
      if (matchKey < pHead->MatchKey_)
         return nullptr;
      if (matchKey == pHead->MatchKey_)
         return pHead;
      pHead = pHead->Next_;
   } while (pHead);
   return nullptr;
}
//--------------------------------------------------------------------------//
void OmsReportFilled::RunReportInCore_MakeReqUID() {
   fon9::NumOutBuf nbuf;
   char*           pout = nbuf.end();
   memset(pout -= sizeof(this->ReqUID_), 0, sizeof(this->ReqUID_));
   pout = fon9::ToStrRev(pout, this->MatchKey_);
   pout = this->RevFilledReqUID(pout);
   *--pout = this->SessionId();
   *--pout = this->Market();
   if (IsEnumContains(this->RequestFlags(), OmsRequestFlag_ForceInternal)) {
      *--pout = ':';
      *--pout = 'I';
   }
   memcpy(this->ReqUID_.Chars_, pout, sizeof(this->ReqUID_));
}
void OmsReportFilled::RunReportInCore(OmsReportChecker&& checker) {
   this->RunReportInCore_Filled(std::move(checker));
}
void OmsReportFilled::RunReportInCore_Filled(OmsReportChecker&& checker) {
   OmsOrder* order;
   if (!OmsIsReqUIDEmpty(*this)) {
      // 嘗試使用 ReqUID 取得 origReq.
      if (const OmsRequestBase* origReq = checker.SearchOrigRequestId())
         if (auto ordraw = origReq->LastUpdated()) {
            this->RunReportInCore_MakeReqUID();
            this->RunReportInCore_FilledOrder(std::move(checker), ordraw->Order());
            return;
         }
   }
   OmsOrdNoMap* ordnoMap = checker.GetOrdNoMap();
   if (ordnoMap == nullptr)
      return;
   this->RunReportInCore_MakeReqUID();
   if (fon9_UNLIKELY(OmsIsOrdNoEmpty(this->OrdNo_))) {
      if (auto* ini = checker.Resource_.Backend_.GetItem(this->IniSNO_)) {
         assert(ini->RxKind() == f9fmkt_RxKind_RequestNew && dynamic_cast<const OmsRequestBase*>(ini) != nullptr);
         if (ini->RxKind() == f9fmkt_RxKind_RequestNew) {
            if (auto* ordraw = static_cast<const OmsRequestBase*>(ini)->LastUpdated()) {
               this->OrdNo_ = ordraw->OrdNo_;
               this->RunReportInCore_FilledOrder(std::move(checker), ordraw->Order());
               return;
            }
         }
      }
      checker.ReportAbandon("OmsFilled: OrdNo cannot empty.");
      return;
   }
   if (fon9_LIKELY((order = ordnoMap->GetOrder(this->OrdNo_)) != nullptr)) {
      this->RunReportInCore_FilledOrder(std::move(checker), *order);
      return;
   }
   // 成交找不到 order: 等候新單回報.
   OmsOrderRaw* ordraw = this->RunReportInCore_FilledMakeOrder(checker);
   if (ordraw == nullptr) {
      if (checker.CheckerSt_ != OmsReportCheckerSt::Abandoned)
         checker.ReportAbandon("OmsFilled: No OrderFactory or MakeOrder err.");
      return;
   }
   OmsReportRunnerInCore inCoreRunner{std::move(checker), *ordraw};
   inCoreRunner.Resource_.Backend_.FetchSNO(*this);
   ordraw->Order().InsertFilled(this);
   ordraw->OrdNo_ = this->OrdNo_;
   inCoreRunner.UpdateFilled(f9fmkt_OrderSt_ReportPending, *this);
   ordnoMap->EmplaceOrder(*ordraw);
}
void OmsReportFilled::RunReportInCore_FilledOrder(OmsReportChecker&& checker, OmsOrder& order) {
   const OmsRequestIni* iniReq = order.Initiator();
   if (iniReq) {
      if (!this->RunReportInCore_FilledIsFieldsMatch(*iniReq)) {
         checker.ReportAbandon("OmsFilled: Field not match.");
         return;
      }
   }
   // 加入 order 的成交串列.
   if (order.InsertFilled(this) != nullptr) {
      checker.ReportAbandon("OmsFilled: Duplicate MatchKey.");
      return;
   }
   if (iniReq && f9fmkt_OrderSt_IsBefore(order.LastOrderSt(), f9fmkt_OrderSt_NewDone))
      this->RunReportInCore_FilledBeforeNewDone(checker.Resource_, order);
   // 更新 order.
   OmsReportRunnerInCore inCoreRunner{std::move(checker), *order.BeginUpdate(*this)};
   if (inCoreRunner.OrderRaw().ErrCode_ == OmsErrCode_FilledBeforeNewDone)
      inCoreRunner.OrderRaw().ErrCode_ = OmsErrCode_Null;
   inCoreRunner.Resource_.Backend_.FetchSNO(*this);
   if (iniReq)
      this->IniSNO_ = iniReq->RxSNO();

   if (fon9_LIKELY(f9fmkt_OrderSt_IsAfterOrEqual(order.LastOrderSt(), f9fmkt_OrderSt_NewDone)
                   && !this->RunReportInCore_FilledIsNeedsReportPending(inCoreRunner.OrderRaw())))
      this->RunReportInCore_FilledUpdateCum(std::move(inCoreRunner));
   else
      inCoreRunner.UpdateFilled(f9fmkt_OrderSt_ReportPending, *this);
}
OmsOrderRaw* OmsReportFilled::RunReportInCore_FilledMakeOrder(OmsReportChecker& checker) {
   (void)checker;
   assert(this->Creator().OrderFactory_.get() != nullptr);
   if (OmsOrderFactory* ordfac = this->Creator().OrderFactory_.get())
      return ordfac->MakeOrder(*this, nullptr);
   return nullptr;
}
void OmsReportFilled::ProcessPendingReport(const OmsRequestRunnerInCore& prevRunner) const {
   assert(this->LastUpdated()->UpdateOrderSt_ == f9fmkt_OrderSt_ReportPending);
   OmsOrder& order = this->LastUpdated()->Order();
   if (f9fmkt_OrderSt_IsBefore(order.LastOrderSt(), f9fmkt_OrderSt_NewDone)
       || this->RunReportInCore_FilledIsNeedsReportPending(*order.Tail()))
      return;
   const OmsRequestIni*    ini = order.Initiator();
   OmsReportRunnerInCore   inCoreRunner{prevRunner, *order.BeginUpdate(*this)};
   if (ini && this->RunReportInCore_FilledIsFieldsMatch(*ini))
      this->RunReportInCore_FilledUpdateCum(std::move(inCoreRunner));
   else {
      inCoreRunner.OrderRaw().ErrCode_ = OmsErrCode_FieldNotMatch;
   }
}
bool OmsReportFilled::RunReportInCore_FilledIsNeedsReportPending(const OmsOrderRaw& lastOrdUpd) const {
   (void)lastOrdUpd;
   return false;
}
void OmsReportFilled::RunReportInCore_FilledBeforeNewDone(OmsResource& resource, OmsOrder& order) {
   (void)resource; (void)order;
}
void OmsReportFilled::OnSynReport(const OmsRequestBase* ref, fon9::StrView message) {
   (void)message;
   if (ref) {
      *static_cast<OmsOrdKey*>(this) = *ref;
      this->Market_    = ref->Market();
      this->SessionId_ = ref->SessionId();
      this->IniSNO_    = ref->RxSNO();
   }
}
//--------------------------------------------------------------------------//
void OmsBadAfterWriteLog(OmsReportRunnerInCore& inCoreRunner, int afterQty) {
   if (inCoreRunner.ExLogForUpd_.cfront()) {
      fon9::RevPrint(inCoreRunner.ExLogForUpd_, ">" fon9_kCSTR_CELLSPL);
   }
   fon9::RevPrint(inCoreRunner.ExLogForUpd_, "Bad.After=", afterQty, '\n');
}

} // namespaces
