// \file f9omstw/OmsParentRequest.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsParentRequest.hpp"
#include "f9omstw/OmsParentOrder.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "f9omstw/OmsResource.hpp"
#include "f9omstw/OmsOrdNoMap.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "fon9/CmdArgs.hpp"

namespace f9omstw {

static void CopyErr_FromChild_ToParent(OmsOrderRaw& parent, const OmsRequestBase& childReq) {
   if (const auto* childOrdraw = childReq.LastUpdated()) {
      parent.ErrCode_ = childOrdraw->ErrCode_;
      parent.Message_ = childOrdraw->Message_;
      parent.RequestSt_ = childOrdraw->RequestSt_;
   }
   else {
      parent.ErrCode_ = childReq.ErrCode();
      parent.Message_.assign(childReq.AbandonReasonStrView());
      parent.RequestSt_ = f9fmkt_TradingRequestSt_InternalRejected;
   }
}
// 改單要求失敗, 若母單改單要求已結束, 則設定 Parent.RequestSt, 不會影響 Parent.OrderSt;
static void OnChildChgError(const bool isRunningDone, const OmsRequestBase& parentReq, const OmsRequestBase& childReq, OmsResource& res, const OmsRequestRunnerInCore* childRunner) {
   const auto* lastUpd = parentReq.LastUpdated();
   const auto  bfst = lastUpd->RequestSt_;
   if (isRunningDone) {
      // 最後一筆子單改單失敗, 即使曾經有成功過,
      // RequestSt = 最後 childReq 的狀態.
      if (bfst >= f9fmkt_TradingRequestSt_Done)
         return;
   }
   else {
      // 尚有未完成的子單改單, 但此次的子單改單失敗.
      if (bfst >= f9fmkt_TradingRequestSt_PartExchangeRejected)
         return;
   }
   OmsRequestRunnerInCore runner{res, *lastUpd->Order().BeginUpdate(parentReq), childRunner};
   CopyErr_FromChild_ToParent(runner.OrderRaw(), childReq);
   if (!isRunningDone)
      runner.OrderRaw().RequestSt_ = f9fmkt_TradingRequestSt_PartExchangeRejected;
}
//--------------------------------------------------------------------------//
bool OmsParentRequestIni::IsParentRequest() const {
   return true;
}
void OmsParentRequestIni::OnAfterBackendReload(OmsResource& res, void* backendLocker) const {
   auto* lastOrdraw = static_cast<const OmsParentOrderRaw*>(this->LastUpdated());
   if (lastOrdraw)
      static_cast<OmsParentOrder*>(&lastOrdraw->Order())->OnAfterBackendReload(res, backendLocker);
   if (this->IsReportIn())
      return;
   // => 多主機備援環境, 必須強制 [死亡主機] 當日禁止重啟.
   //    => 避免: 已由備援主機接手母單, 死機重啟後, 互搶母單的更新.
   // => 單機環境, 則應啟動底下機制.
   struct AutoGetCmdArg {
      bool IsNeedsEnableParentOnReload_{};
      AutoGetCmdArg() {
         // 使用 [執行參數: --EnableParentOnReload] 來決定 : 重啟時是否調整[本機母單]狀態?
         // 預設為 off; 與 --EnableParentOnReload=N 相同;
         // 打開此功能: --EnableParentOnReload
         auto v = fon9::GetCmdArg(0, nullptr, nullptr, "EnableParentOnReload");
         if (v.IsNull()) {
            // 沒有提供執行參數, 使用預設值.
         }
         else {
            // 有提供執行參數: 只有提供 "=N" 才關閉此功能, 否則開啟.
            this->IsNeedsEnableParentOnReload_ = (fon9::toupper(v.Get1st()) != 'N');
         }
      }
   };
   static AutoGetCmdArg  sAutoGetCmdArg;
   if (sAutoGetCmdArg.IsNeedsEnableParentOnReload_ && lastOrdraw) {
      auto& order = lastOrdraw->Order();
      // -----
      // SetParentEnabled(): 為了在 OmsParentOrder::OnChildOrderUpdated();
      // 當子單有異動時, 仍能正確更新母單的狀態。
      order.SetParentEnabled();
      // -----
      lastOrdraw = static_cast<const OmsParentOrderRaw*>(order.Tail());
      if (lastOrdraw->ChildLeavesQty_ < lastOrdraw->LeavesQty_) {
         // 重啟後, 所有[本機母單]進入完畢狀態.
         // static_cast<OmsBackend::Locker*>(backendLocker)->unlock(); 改用: runner.BackendLocker_ = backendLocker;
         if (OmsOrderRaw* ordupd = order.BeginUpdate(*this)) {
            OmsRequestRunnerInCore runner{res, *ordupd};
            runner.BackendLocker_ = backendLocker;
            auto* upd = static_cast<OmsParentOrderRaw*>(ordupd);
            upd->AfterQty_ = upd->LeavesQty_ = upd->ChildLeavesQty_;
            upd->Message_.assign("System restart");
            upd->ErrCode_ = OmsErrCode_Parent_OnlyDel;
            upd->RequestSt_ = f9fmkt_TradingRequestSt_Done;
            if (upd->LeavesQty_ > 0) {
               // 還有剩餘量 => 母單結束, 等候子單成交.
               if (upd->CumQty_ == 0) {
                  // 還有剩餘量, 目前無成交.
                  upd->UpdateOrderSt_ = f9fmkt_OrderSt_ExchangeAccepted;
               }
               else {
                  upd->UpdateOrderSt_ = f9fmkt_OrderSt_PartFilled;
               }
            }
            else {
               if (upd->CumQty_ == 0) {
                  // 已無剩餘量, 無成交 => 全都取消.
                  upd->RequestSt_ = f9fmkt_TradingRequestSt_QueuingCanceled;
                  upd->UpdateOrderSt_ = f9fmkt_OrderSt_NewQueuingCanceled;
               }
               else {
                  // 已無剩餘量, 有成交 => 已全部成交.
                  upd->UpdateOrderSt_ = f9fmkt_OrderSt_FullFilled;
               }
            }
         }
         // static_cast<OmsBackend::Locker*>(backendLocker)->lock();
      }
   }
   base::OnAfterBackendReload(res, backendLocker);
}
void OmsParentRequestIni::OnAfterBackendReloadChild(const OmsRequestBase& childReq) const {
   if (childReq.IsAbandoned() || !childReq.LastUpdated()->Order().IsWorkingOrder()) {
      DecRunningChildCount(this->RunningChildCount_);
      if (childReq.RxKind() == f9fmkt_RxKind_RequestNew) {
         assert(dynamic_cast<const OmsRequestIni*>(&childReq) != nullptr);
         assert(dynamic_cast<OmsParentOrder*>(&this->LastUpdated()->Order()) != nullptr);
         static_cast<OmsParentOrder*>(&this->LastUpdated()->Order())->EraseChild(static_cast<const OmsRequestIni*>(&childReq));
      }
   }
}
void OmsParentRequestIni::OnMoveChildRunnerToCoreError(OmsRequestRunnerInCore& parentRunner, OmsRequestRunner&& childRunner) const {
   DecRunningChildCount(this->RunningChildCount_);
   if (childRunner.Request_->RxKind() == f9fmkt_RxKind_RequestNew) {
      assert(dynamic_cast<OmsParentOrder*>(&parentRunner.OrderRaw().Order()) != nullptr);
      assert(dynamic_cast<OmsRequestIni*>(childRunner.Request_.get()) != nullptr);
      static_cast<OmsParentOrder*>(&parentRunner.OrderRaw().Order())->EraseChild(static_cast<OmsRequestIni*>(childRunner.Request_.get()));
   }
}
void OmsParentRequestIni::OnAddChildRequest(const OmsRequestBase& childReq, OmsRequestRunnerInCore* parentRunner) const {
   if (childReq.RxKind() == f9fmkt_RxKind_RequestNew) {
      const OmsOrderRaw* lastUpd = (parentRunner ? &parentRunner->OrderRaw() : this->LastUpdated());
      assert(dynamic_cast<const OmsRequestIni*>(&childReq) != nullptr);
      assert(lastUpd != nullptr && dynamic_cast<OmsParentOrder*>(&lastUpd->Order()) != nullptr);
      static_cast<OmsParentOrder*>(&lastUpd->Order())->AddChild(*static_cast<const OmsRequestIni*>(&childReq));
   }
   ++this->RunningChildCount_;
}
void OmsParentRequestIni::OnChildError(const OmsRequestBase& childReq, OmsResource& res, const OmsRequestRunnerInCore* childRunner) const {
   if (childReq.RxKind() != f9fmkt_RxKind_RequestNew) {
      OnChildChgError(DecRunningChildCount(this->RunningChildCount_), *this, childReq, res, childRunner);
      return;
   }

   DecRunningChildCount(this->RunningChildCount_);
   assert(dynamic_cast<const OmsRequestIni*>(&childReq) != nullptr);
   auto& childIni = *static_cast<const OmsRequestIni*>(&childReq);

   assert(dynamic_cast<OmsParentOrder*>(&this->LastUpdated()->Order()) != nullptr);
   auto& parentOrder = *static_cast<OmsParentOrder*>(&this->LastUpdated()->Order());
   parentOrder.EraseChild(&childIni);

   OmsRequestRunnerInCore runner{res, *parentOrder.BeginUpdate(*this), childRunner};
   auto  qtyRejected = this->GetChildRequestQty(childIni);
   auto& ordraw = runner.OrderRawT<OmsParentOrderRaw>();
   CopyErr_FromChild_ToParent(ordraw, childIni);
   assert(ordraw.ChildLeavesQty_ >= qtyRejected);
   if (fon9::signed_cast(ordraw.ChildLeavesQty_ -= qtyRejected) < 0)
      ordraw.ChildLeavesQty_ = 0;
   // ===== 子單失敗後的 OrderSt =====
   ordraw.AfterQty_ = ordraw.LeavesQty_ = ordraw.ChildLeavesQty_;
   parentOrder.SetParentStopRun();
   if (ordraw.LeavesQty_ > 0) {
      // 還有剩餘量 => 部分失敗.
      ordraw.RequestSt_ = f9fmkt_TradingRequestSt_PartExchangeRejected;
      if (ordraw.CumQty_ == 0) {
         // 還有剩餘量, 目前無成交 => 新單部分失敗.
         // 因為只要有子單新單失敗, 母單就失效,
         // 所以: 若之後有成交會使用 f9fmkt_OrderSt_PartFilled;
         ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_NewPartExchangeRejected;
      }
      else {
         ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_PartFilled;
      }
   }
   else {
      if (ordraw.CumQty_ == 0) {
         // 已無剩餘量, 無成交 => 全都失敗.
         ordraw.UpdateOrderSt_ = static_cast<f9fmkt_OrderSt>(ordraw.RequestSt_);
      }
      else {
         // 已無剩餘量, 有成交 => 部分失敗, 成功的部分已全部成交.
         ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_FullFilled;
      }
   }
}
void OmsParentRequestIni::OnChildAbandoned(const OmsRequestBase& childReq, OmsResource& res) const {
   this->OnChildError(childReq, res, nullptr);
}
void OmsParentRequestIni::OnChildRequestUpdated(const OmsRequestRunnerInCore& childRunner) const {
   auto& childOrdraw = childRunner.OrderRaw();
   if (childOrdraw.RequestSt_ < f9fmkt_TradingRequestSt_Done)
      return;
   assert(dynamic_cast<OmsParentOrder*>(&this->LastUpdated()->Order()) != nullptr);
   auto& parentOrder = *static_cast<OmsParentOrder*>(&this->LastUpdated()->Order());
   if (!parentOrder.IsParentEnabled()) {
      if (!childOrdraw.IsWorking()) {
         parentOrder.EraseChild(childOrdraw.Order().Initiator());
         DecRunningChildCount(this->RunningChildCount_);
      }
      return;
   }
   const auto& childReq = childOrdraw.Request();
   if (f9fmkt_TradingRequestSt_QueuingCanceled < childOrdraw.RequestSt_
       && childOrdraw.RequestSt_ < f9fmkt_TradingRequestSt_ExchangeNoLeavesQty) {
      this->OnChildError(childReq, childRunner.Resource_, &childRunner);
      return;
   }
   OmsParentQty   childLeavesQty;
   OmsParentQtyS  childAdjQty;
   const bool     isRunningDone = DecRunningChildCount(this->RunningChildCount_);
   this->GetNewChildOrderQtys(childOrdraw, childLeavesQty, childAdjQty);

   if (childLeavesQty == 0)
      parentOrder.EraseChild(childOrdraw.Order().Initiator());
   // 子單數量, 只有可能變少(例:排隊中減量), 不可能變多.
   assert(childAdjQty <= 0);
   if (childAdjQty > 0) {
      return;
   }

   const auto  lastReqSt = this->LastUpdated()->RequestSt_;
   const auto& parentLastOrdraw = *static_cast<const OmsParentOrderRaw*>(parentOrder.Tail());
   const bool  isParentDone = (isRunningDone && parentLastOrdraw.LeavesQty_ == parentLastOrdraw.ChildLeavesQty_);
   const bool  isFirstRunningDone = (isRunningDone && lastReqSt < f9fmkt_TradingRequestSt_PartExchangeAccepted);
   if (isParentDone && childAdjQty == 0) {
      // isParentDone:     母單已完畢, 且最後一筆子單成功;
      // childAdjQty == 0: Parent.LeavesQty 不用調整;
      if (lastReqSt == f9fmkt_TradingRequestSt_PartExchangeAccepted) {
         // 全部完成後 PartExchangeAccepted 必須變成 ExchangeAccepted;
      }
      else if (lastReqSt == f9fmkt_TradingRequestSt_PartExchangeRejected) {
         // 子單部分失敗, 最後的成功: 狀態不變.
         return;
      }
      else if (lastReqSt >= f9fmkt_TradingRequestSt_Done) {
         // 母單處在其他狀態, 最後的成功: 狀態不變.
         return;
      }
      // 新單的 RequestSt (未更新過) or (為 PartExchangeAccepted)
      // => 則應變成 ExchangeAccepted;
      // => 告知此母單已完畢.
   }
   if (childAdjQty != 0 || isParentDone || isFirstRunningDone) {
      OmsRequestRunnerInCore runner{childRunner, *parentOrder.BeginUpdate(*this)};
      auto& ordraw = runner.OrderRawT<OmsParentOrderRaw>();
      if (childAdjQty != 0) {
         ordraw.DecChildParentLeavesQty(fon9::unsigned_cast(-childAdjQty));
      }
      if (isParentDone) { // 母單已完畢(且子單已全部回報), 中途可能有: 成功、失敗、成交、刪改...
         ordraw.RequestSt_ = f9fmkt_TradingRequestSt_ExchangeAccepted;
         if (ordraw.CumQty_ > 0)
            ordraw.UpdateOrderSt_ = (ordraw.LeavesQty_ > 0 ? f9fmkt_OrderSt_PartFilled : f9fmkt_OrderSt_FullFilled);
         else { // if (ordraw.CumQty_ == 0)
            ordraw.UpdateOrderSt_ = (ordraw.LeavesQty_ > 0 ? f9fmkt_OrderSt_ExchangeAccepted : f9fmkt_OrderSt_ExchangeCanceled);
         }
      }
      else { // 母單尚未完成, 有子單成功: (有 childAdjQty) or (isFirstRunningDone)
         if (lastReqSt < f9fmkt_TradingRequestSt_PartExchangeAccepted)
            ordraw.RequestSt_ = f9fmkt_TradingRequestSt_PartExchangeAccepted;
         if (ordraw.UpdateOrderSt_ < f9fmkt_OrderSt_NewPartExchangeAccepted)
            ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_NewPartExchangeAccepted;
      }
   }
}

void OmsParentRequestIni::OnSynReport(const OmsRequestBase* ref, fon9::StrView message) {
   this->ReportRef_ = ref;
   base::OnSynReport(ref, message);
}
fon9::HostId OmsParentRequestIni::OrigHostId() const {
   return this->OrigHostId_;
}
void OmsParentRequestIni::SetOrigHostId(fon9::HostId origHostId) {
   this->OrigHostId_ = origHostId;
}

static inline bool IsReportRefMatch(const OmsParentRequestIni& rthis, const OmsRequestBase& ref) {
   return ref.RxSNO() != 0
      && ref.ReqUID_ == rthis.ReqUID_
      && ref.OrdNo_ == rthis.OrdNo_
      && rthis.GetNotEqualIniFieldName(ref) == nullptr;
}
void OmsParentRequestIni::RunReportInCore(OmsReportChecker&& checker) {
   // 母單回報(新單、刪改、成交)都會來到這裡:
   // - 只會從原始收單主機送來, 不會有其他主機的母單回報. 所以:
   //   - 必定會依序回報, 不會有亂序回報, 因此不用考慮 PendingReport;
   //   - 刪改回報、子單造成的異動: 直接用 this(Report) 更新 ordraw 的內容即可.
   // - 成交也是透過這裡回報.
   // - 子單刪改造成的異動, 仍會先由 OmsParentRequestIni::OnChildRequestUpdated() 處理.
   //   若子單已無剩餘量, 則會從 WorkingChild 移除.
   OmsRxSNO refSNO;
   if (OmsParseForceReportReqUID(*this, refSNO)) {
      assert(this->ReportRef_ != nullptr);
      if (this->ReportRef_ == nullptr || refSNO != this->ReportRef_->RxSNO()) {
         checker.ReportAbandon("ParentReport: Bad ReportRef.");
         return;
      }
      this->ReqUID_ = this->ReportRef_->ReqUID_;
   }
   fon9_WARN_DISABLE_SWITCH;
   switch (this->RxKind()) {
   case f9fmkt_RxKind_Filled:
      if (fon9_UNLIKELY(this->ReportRef_ == nullptr)) {
         checker.ReportAbandon("ParentReport: Unknown filled ref.");
         return;
      }
      if (fon9_UNLIKELY(this->ReportRef_->RxSNO() <= 0)) {
         // 對子單而言, 此筆成交可能為重複回報, 造成 this->ReportRef_ 沒有加入 OmsCore.
         // 所以: 從子單找看看有沒有這筆成交.
         assert(dynamic_cast<const OmsReportFilled*>(this->ReportRef_) != nullptr);
         auto* reqFilled = static_cast<const OmsReportFilled*>(this->ReportRef_);
         if (auto* childIni = checker.Resource_.GetRequest(reqFilled->IniSNO_)) {
            if (auto* childIniOrdraw = childIni->LastUpdated()) {
               if ((this->ReportRef_ = childIniOrdraw->Order().FindFilled(reqFilled->MatchKey_)) != nullptr)
                  goto __RPT_FILLED;
            }
         }
         checker.ReportAbandon("ParentReport: Unknown filled.");
         return;
      }
   __RPT_FILLED:;
      if (auto* childOrdraw = this->ReportRef_->LastUpdated()) {
         // 子單成交有正確的更新子單內容, 表示此筆成交沒有重複, 可讓母單繼續處理.
         if (auto* pParentOrder = childOrdraw->Order().GetParentOrder()) {
            assert(dynamic_cast<OmsParentOrder*>(pParentOrder) != nullptr);
            // 要檢查 此筆成交 是否為重複更新?
            auto* pHeadOrdraw = pParentOrder->Head();
            while (pHeadOrdraw) {
               if (&pHeadOrdraw->Request() == this->ReportRef_) {
                  checker.ReportAbandon("ParentReport: Duplicate filled.");
                  return;
               }
               pHeadOrdraw = pHeadOrdraw->Next();
            }
            // =====> (1) 先用 this->ReportRef_(RequestFilled) 處理成交回報(更新 Cum* 欄位),
            static_cast<OmsParentOrder*>(pParentOrder)->RunChildOrderUpdated(
               *childOrdraw, checker.Resource_, nullptr, &checker,
               [](OmsParentOrder& parentOrder, const OmsRequestBase& parentIni, OmsRequestRunnerInCore&& parentRunner, void* udata) {
               (void)parentOrder; (void)parentIni;
               OmsReportChecker&    uchecker = *static_cast<OmsReportChecker*>(udata);
               OmsParentRequestIni& rthis = *static_cast<OmsParentRequestIni*>(uchecker.Report_.get());
               parentRunner.ExLogForUpd_ = std::move(uchecker.ExLog_);
               fon9::RevPrint(parentRunner.ExLogForUpd_, '\n');
               rthis.RevPrint(parentRunner.ExLogForUpd_);
               // =====> (2) 再將 this 異動欄位填入 ParentOrderRaw,
               rthis.OnParentReportInCore_Filled(parentRunner.OrderRawT<OmsParentOrderRaw>());
            });
            // =====> (3) 然後才能完成此次成交回報的更新.
            break;
         }
      }
      checker.ReportAbandon("ParentReport: Unknown filled NoChild.");
      return;
   case f9fmkt_RxKind_RequestNew:
      if (this->ReportRef_) {
         // this->ReportRef_(母單新單要求) 後續的回報(例: 建立子單後的母單異動回報);
         if (fon9_LIKELY(IsReportRefMatch(*this, *this->ReportRef_))) {
            if (this->IsNeedsReport_ForNew(checker, *this->ReportRef_)) {
               OmsOrder&               order = this->ReportRef_->LastUpdated()->Order();
               OmsReportRunnerInCore   inCoreRunner{std::move(checker), *order.BeginUpdate(*this->ReportRef_)};
               this->OnParentReportInCore_NewUpdate(inCoreRunner);
               inCoreRunner.UpdateReport(*this);
            }
         }
         else {
            checker.ReportAbandon("ParentReport: Bad NewUpdate.");
         }
      }
      else {
         // 新建立的 [新單] 回報.
         if (OmsOrdNoMap* ordnoMap = checker.GetOrdNoMap()) {
            assert(!OmsIsReqUIDEmpty(*this));
            if (fon9_UNLIKELY(OmsIsReqUIDEmpty(*this)))
               checker.ReportAbandon("ParentReport: Bad new ReqUID.");
            else if (OmsOrder* order = ordnoMap->GetOrder(this->OrdNo_)) {
               (void)order;
               checker.ReportAbandon("ParentReport: duplicate OrdNo.");
            }
            else  {
               assert(this->Creator().OrderFactory_.get() != nullptr);
               if (OmsOrderFactory* ordfac = this->Creator().OrderFactory_.get()) {
                  OmsReportRunnerInCore   inCoreRunner{std::move(checker), *ordfac->MakeOrder(*this, nullptr)};
                  auto&                   ordraw = inCoreRunner.OrderRaw();
                  ordraw.OrdNo_ = this->OrdNo_;
                  this->OnParentReportInCore_NewReport(inCoreRunner);
                  inCoreRunner.Resource_.Backend_.FetchSNO(*this);
                  inCoreRunner.UpdateReport(*this);
                  ordnoMap->EmplaceOrder(ordraw);
               }
               else {
                  checker.ReportAbandon("ParentReport: No OrderFactory.");
               }
            }
         }
         else {
            // checker.GetOrdNoMap(); 失敗.
            // 已呼叫過 checker.ReportAbandon(), 所以直接結束.
         }
      }
      break;
   default: // 刪改回報.
      if (this->ReportRef_) {
         // this->ReportRef_(母單刪改要求) 的後續回報.
         if (fon9_LIKELY(IsReportRefMatch(*this, *this->ReportRef_))) {
            if (this->IsNeedsReport_ForChg(checker, *this->ReportRef_)) {
               OmsOrder&               order = this->ReportRef_->LastUpdated()->Order();
               OmsReportRunnerInCore   inCoreRunner{std::move(checker), *order.BeginUpdate(*this->ReportRef_)};
               this->OnParentReportInCore_ChgUpdate(inCoreRunner);
               inCoreRunner.UpdateReport(*this);
            }
         }
         else {
            checker.ReportAbandon("ParentReport: Bad ChgUpdate.");
         }
      }
      else {
         // 新建立的 [刪改] 回報.
         // 先用 OrdNo 找到 RequestIni.
         if (OmsOrdNoMap* ordnoMap = checker.GetOrdNoMap()) {
            assert(!OmsIsReqUIDEmpty(*this));
            if (fon9_UNLIKELY(OmsIsReqUIDEmpty(*this)))
               checker.ReportAbandon("ParentReport: Bad chg ReqUID.");
            else if (OmsOrder* order = ordnoMap->GetOrder(this->OrdNo_)) {
               // auto* ini = order->Initiator();
               // if (fon9_LIKELY(檢查 this 與 ini 的欄位是否相符))) {
               // [改單回報] 不一定回提供全部欄位, 所以用委託書號找到 原委託 即可.
                  OmsReportRunnerInCore   inCoreRunner{std::move(checker), *order->BeginUpdate(*this)};
                  this->OnParentReportInCore_ChgReport(inCoreRunner);
                  inCoreRunner.Resource_.Backend_.FetchSNO(*this);
                  inCoreRunner.UpdateReport(*this);
               // }
               // else {
               //    checker.ReportAbandon("ParentReport: Order not match for chg.");
               // }
            }
            else {
               checker.ReportAbandon("ParentReport: Order not found.");
            }
         }
         else {
            // checker.GetOrdNoMap(); 失敗.
            // 已呼叫過 checker.ReportAbandon(), 所以直接結束.
         }
      }
      break;
   }
   fon9_WARN_POP;
}
fon9::TimeStamp OmsParentRequestIni::GetSrcUTime() const {
   return fon9::TimeStamp::Null();
}
bool OmsParentRequestIni::IsNeedsReport(const OmsRequestBase& ref) const {
   if (ref.LastUpdated() == nullptr)
      return true;
   auto& refOrder = ref.LastUpdated()->Order();
   if (!refOrder.IsWorkingOrder())
      return false;
   assert(dynamic_cast<const OmsParentOrderRaw*>(refOrder.Tail()) != nullptr);
   return this->GetSrcUTime() > static_cast<const OmsParentOrderRaw*>(refOrder.Tail())->SrcUTime_;
}
bool OmsParentRequestIni::IsNeedsReport_ForNew(OmsReportChecker& checker, const OmsRequestBase& ref) {
   if (this->IsNeedsReport(ref))
      return true;
   checker.ReportAbandon("ParentReport: Duplicate or Obsolete report ini.");
   return false;
}
bool OmsParentRequestIni::IsNeedsReport_ForChg(OmsReportChecker& checker, const OmsRequestBase& ref) {
   if (this->IsNeedsReport(ref))
      return true;
   checker.ReportAbandon("ParentReport: Duplicate or Obsolete report chg.");
   return false;
}
void OmsParentRequestIni::OnParentReportInCore_Filled(OmsParentOrderRaw& ordraw) {
   (void)ordraw;
   assert(!"Derived must override OnParentReportInCore_Filled()");
}
void OmsParentRequestIni::OnParentReportInCore_NewReport(OmsReportRunnerInCore& runner) {
   (void)runner;
   assert(!"Derived must override OnParentReportInCore_NewReport()");
}
void OmsParentRequestIni::OnParentReportInCore_NewUpdate(OmsReportRunnerInCore& runner) {
   (void)runner;
   assert(!"Derived must override OnParentReportInCore_NewUpdate()");
}
void OmsParentRequestIni::OnParentReportInCore_ChgReport(OmsReportRunnerInCore& runner) {
   (void)runner;
   assert(!"Derived must override OnParentReportInCore_ChgReport()");
}
void OmsParentRequestIni::OnParentReportInCore_ChgUpdate(OmsReportRunnerInCore& runner) {
   (void)runner;
   assert(!"Derived must override OnParentReportInCore_ChgUpdate()");
}
//--------------------------------------------------------------------------//
bool OmsParentRequestChg::IsParentRequest() const {
   return true;
}
void OmsParentRequestChg::OnAfterBackendReloadChild(const OmsRequestBase& childReq) const {
   if (childReq.IsAbandoned() || childReq.LastUpdated()->RequestSt_ >= f9fmkt_TradingRequestSt_Done)
      DecRunningChildCount(this->RunningChildCount_);
}
void OmsParentRequestChg::OnMoveChildRunnerToCoreError(OmsRequestRunnerInCore& parentRunner, OmsRequestRunner&& childRunner) const {
   (void)parentRunner; (void)childRunner;
   DecRunningChildCount(this->RunningChildCount_);
}
void OmsParentRequestChg::OnAddChildRequest(const OmsRequestBase& childReq, OmsRequestRunnerInCore* parentRunner) const {
   (void)childReq; (void)parentRunner;
   ++this->RunningChildCount_;
}
void OmsParentRequestChg::OnChildError(const OmsRequestBase& childReq, OmsResource& res, const OmsRequestRunnerInCore* childRunner) const {
   OnChildChgError(DecRunningChildCount(this->RunningChildCount_), *this, childReq, res, childRunner);
}
void OmsParentRequestChg::OnChildAbandoned(const OmsRequestBase& childReq, OmsResource& res) const {
   this->OnChildError(childReq, res, nullptr);
}
void OmsParentRequestChg::OnChildRequestUpdated(const OmsRequestRunnerInCore& childRunner) const {
   auto& childOrdraw = childRunner.OrderRaw();
   if (childOrdraw.RequestSt_ < f9fmkt_TradingRequestSt_Done)
      return;
   const auto& childReq = childOrdraw.Request();
   if (f9fmkt_TradingRequestSt_IsFinishedRejected(childOrdraw.RequestSt_)) {
      this->OnChildError(childReq, childRunner.Resource_, &childRunner);
      return;
   }
   // 處理子單改單結果(調整ParentOrdraw): 若改單完成, 需設定最終的 RequestSt, OrderSt;
   const bool  isRunningDone = DecRunningChildCount(this->RunningChildCount_);

   assert(dynamic_cast<OmsParentOrder*>(&this->LastUpdated()->Order()) != nullptr);
   auto&          parentOrder = *static_cast<OmsParentOrder*>(&this->LastUpdated()->Order());
   OmsParentQty   childLeavesQty;
   OmsParentQtyS  childAdjQty;
   parentOrder.GetChildOrderQtys(childOrdraw, childLeavesQty, childAdjQty);
   assert(childAdjQty <= 0); // 子單改單, 數量只可能變少, 不可能增量.
   if (childLeavesQty <= 0)
      parentOrder.EraseChild(childOrdraw.Order().Initiator());

   const auto lastReqSt = this->LastUpdated()->RequestSt_;
   if (childAdjQty == 0) {
      if (isRunningDone) {
         if (lastReqSt >= f9fmkt_TradingRequestSt_Done)
            return;
      }
      else {
         if (lastReqSt >= f9fmkt_TradingRequestSt_PartExchangeAccepted)
            return;
      }
   }

   OmsRequestRunnerInCore runner{childRunner, *parentOrder.BeginUpdate(*this)};
   auto& ordraw = runner.OrderRawT<OmsParentOrderRaw>();
   if (childAdjQty != 0) {
      ordraw.DecChildParentLeavesQty(fon9::unsigned_cast(-childAdjQty));
   }
   if (isRunningDone) // 母單改單的子單已全部回報.
      ordraw.RequestSt_ = f9fmkt_TradingRequestSt_ExchangeAccepted;
   else {
      ordraw.RequestSt_ = (lastReqSt < f9fmkt_TradingRequestSt_Done
                           ? f9fmkt_TradingRequestSt_PartExchangeAccepted
                           : lastReqSt);
   }
   if (ordraw.LeavesQty_ == 0)
      ordraw.UpdateOrderSt_ = (ordraw.CumQty_ > 0 ? f9fmkt_OrderSt_FullFilled : f9fmkt_OrderSt_UserCanceled);
}

} // namespaces
