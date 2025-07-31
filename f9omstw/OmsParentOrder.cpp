// \file f9omstw/OmsParentOrder.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsParentOrder.hpp"
#include "f9omstw/OmsRequestFactory.hpp"

namespace f9omstw {

OmsParentOrder::~OmsParentOrder() {
}

void OmsParentOrder::RunChildOrderUpdated(const OmsOrderRaw& childOrdraw, OmsResource& res, const OmsRequestRunnerInCore* childRunner, void* udata,
                                          FnOnAfterChildOrderUpdated fnOnAfterChildOrderUpdated) {
   OmsParentQty  childLeavesQty;
   OmsParentQtyS childAdjQty; // 刪減成功數量 or 成交數量.
   ChildOrderSt  childSt = this->GetChildOrderQtys(childOrdraw, childLeavesQty, childAdjQty);
   if (childAdjQty > 0) // 不可能增量! 但有可能 [二次回報取消,例:OmsErrCode_Twf_DynPriBandRpt], 所以 childAdjQty 可能為 0;
      return;
   if (childAdjQty == 0 && childOrdraw.RequestSt_ < f9fmkt_TradingRequestSt_Done)
      // 子單尚未完成(例:Queuing,Sending...), 不更新母單;
      return;
   const auto* reqFrom = &childOrdraw.Request();
   // 這裡使用 childOrdraw.Request() 來更新母單,
   // - 會造成 childOrdraw.Request().LastUpdated() 變成母單的 OrderRaw;
   // - 修改 OmsRequestBase::SetLastUpdated() 避免這種情況.
   // ParentOrd + 成交回報(TwfFil or TwfFil2 or TwsFil);
   // ParentOrd + 手動改單(TwfChg or TwsChg) => (ParentNew);
   f9fmkt_TradingRequestSt reqSt{};
   const OmsRequestBase*   parentIni = &this->Head()->Request();
   if (reqFrom->RxKind() == f9fmkt_RxKind_Filled)
      reqSt = childOrdraw.RequestSt_;
   else {
      reqFrom = parentIni;
      if (!this->IsContinueTailUpdate())
         reqSt = reqFrom->LastUpdated()->RequestSt_;
   }
   if (childLeavesQty == 0) {
      this->EraseChild(childOrdraw.Order().Initiator());
   }
   // -----
   OmsRequestRunnerInCore parentRunner{res, *this->BeginUpdate(*reqFrom), childRunner};
   auto& parentOrdraw = parentRunner.OrderRawT<OmsParentOrderRaw>();
   parentOrdraw.DecChildParentLeavesQty(fon9::unsigned_cast(-childAdjQty), IsChildRunning(childSt));
   if (reqSt != f9fmkt_TradingRequestSt{})
      parentOrdraw.RequestSt_ = reqSt;
   if (childOrdraw.RequestSt_ == f9fmkt_TradingRequestSt_Filled) {
      this->OnChildFilled(parentOrdraw, *reqFrom);
      // ===== 成交後的 OrderSt =====
      assert(parentOrdraw.CumQty_ > 0);
      if (parentOrdraw.LeavesQty_ == 0) {
         assert(parentOrdraw.ChildLeavesQty_ == 0);
         parentOrdraw.UpdateOrderSt_ = f9fmkt_OrderSt_FullFilled;
      }
      else if (parentOrdraw.LeavesQty_ == parentOrdraw.ChildLeavesQty_) {
         // 可能有子單失敗(必定: parentOrdraw.LeavesQty_ == parentOrdraw.ChildLeavesQty_);
         parentOrdraw.UpdateOrderSt_ = f9fmkt_OrderSt_PartFilled;
      }
      else {
         // 還有子單未送出.
         assert(parentOrdraw.LeavesQty_ > parentOrdraw.ChildLeavesQty_);
         parentOrdraw.UpdateOrderSt_ = f9fmkt_OrderSt_ParentPartFilled;
      }
   }
   else {
      // 最後一筆子單的失敗, 才更新母單的錯誤代碼;
      // 避免: 如果還有其他子單存在, 可能會造成使用者誤判;
      if (parentOrdraw.LeavesQty_ == 0)
         parentOrdraw.ErrCode_ = childOrdraw.ErrCode_;
      // 手動刪改子單 or 未成交刪單(fa:ExchangeCanceled, OmsErrCode_SessionClosedCanceled, OmsErrCode_SessionClosedRejected, OmsErrCode_Twf_DynPriBandRej);
      fon9_WARN_DISABLE_SWITCH;
      switch(childOrdraw.ErrCode_) {
      case OmsErrCode_Twf_DynPriBandRej:
      case OmsErrCode_Twf_DynPriBandRpt:
         parentOrdraw.Message_ = childOrdraw.Message_;
         /* fall through */
      case OmsErrCode_SessionClosedCanceled:
      case OmsErrCode_SessionClosedRejected:
         parentOrdraw.LeavesQty_ = parentOrdraw.ChildLeavesQty_;
         this->SetParentStopRun();
         break;
      }
      fon9_WARN_POP;
      // ===== 子單刪減成功後的 OrderSt =====
      // 刪減後還有剩餘量 => 狀態不變(Sending, PartRejected, PartAccepted, PartFilled...)
      if (parentOrdraw.LeavesQty_ == 0) {
         // 全部刪除後 => 根據成交設定最後狀態.
         if (parentOrdraw.CumQty_ == 0) {
            // 已無剩餘量, 無成交 => 全都刪單.
            parentOrdraw.UpdateOrderSt_ = f9fmkt_OrderSt_ExchangeCanceled;
         }
         else {
            // 已無剩餘量, 有成交 => 成功的部分已全部成交.
            parentOrdraw.UpdateOrderSt_ = f9fmkt_OrderSt_FullFilled;
         }
      }
   }
   fnOnAfterChildOrderUpdated(*this, *parentIni, std::move(parentRunner), udata);
}
void OmsParentOrder::OnChildOrderUpdated(const OmsRequestRunnerInCore& childRunner) {
   // 手動刪改子單 or 成交回報.
   if (this->IsParentEnabled()) {
      const auto& childOrdraw = childRunner.OrderRaw();
      this->RunChildOrderUpdated(childOrdraw, childRunner.Resource_, &childRunner, const_cast<OmsOrderRaw*>(&childOrdraw),
                                 [](OmsParentOrder& parentOrder, const OmsRequestBase& parentIni, OmsRequestRunnerInCore&& parentRunner, void* udata) {
         parentOrder.OnAfterChildOrderUpdated(parentIni, std::move(parentRunner), *static_cast<const OmsOrderRaw*>(udata));
      });
   }
}
void OmsParentOrder::OnAfterChildOrderUpdated(const OmsRequestBase& parentIni, OmsRequestRunnerInCore&& parentRunner, const OmsOrderRaw& childOrdraw) {
   (void)parentIni; (void)parentRunner; (void)childOrdraw;
}

void OmsParentOrder::OnAfterBackendReload(OmsResource& res, void* backendLocker) {
   (void)res; (void)backendLocker;
   if (static_cast<const OmsParentOrderRaw*>(this->Tail())->LeavesQty_ <= 0) {
      assert(static_cast<const OmsParentOrderRaw*>(this->Tail())->ChildLeavesQty_ <= 0);
      this->WorkingChildRequests_.clear();
   }
}
void OmsParentOrder::OnOrderUpdated(const OmsRequestRunnerInCore& runner) {
   // 備援主機, 尚未接手母單, this->EraseChild(); 暫時先不要移除.
   // - 等母單完結後, 再清除 this->WorkingChildRequests_;
   //   - 所以需要設定 OmsOrderFlag::IsNeedsOnOrderUpdated;
   //   - 覆寫 void OnOrderUpdated(const OmsRequestRunnerInCore& runner) override;
   //   - 若 this->Tail()->LeavesQty <= 0 則清除全部 this->WorkingChildRequests_;
   if (runner.OrderRawT<OmsParentOrderRaw>().LeavesQty_ <= 0) {
      assert(runner.OrderRawT<OmsParentOrderRaw>().ChildLeavesQty_ <= 0);
      this->WorkingChildRequests_.clear();
   }
   base::OnOrderUpdated(runner);
}
bool OmsParentOrder::EraseChild(const OmsRequestIni* childReq) {
   assert(childReq->IsAbandoned() || !childReq->LastUpdated()->Order().IsWorkingOrder());
   assert(childReq->RxKind() == f9fmkt_RxKind_RequestNew);
   if (this->IsParentEnabled() || !this->Initiator()->IsReportIn()) {
      auto ifind = std::find(this->WorkingChildRequests_.begin(), this->WorkingChildRequests_.end(), childReq);
      if (ifind == this->WorkingChildRequests_.end())
         return false;
      this->WorkingChildRequests_.erase(ifind);
      return true;
   }
   return false;
}
OmsParentOrderRaw* OmsParentOrder::RecalcAllChildForRerun() {
   auto* tail = static_cast<const OmsParentOrderRaw*>(this->Tail());
   assert(tail && tail->LeavesQty_ > 0 && this->WorkingChildRequests_.size() > 0);
   OmsParentOrderRawDat recalc;
   recalc.ClearRawDat();
   recalc.RequireQty_ = tail->RequireQty_;
   recalc.LeavesQty_ = fon9::unsigned_cast(static_cast<OmsParentQtyS>(-1));
   auto icur = this->WorkingChildRequests_.begin();
   while (icur != this->WorkingChildRequests_.end()) {
      if (auto* child = (*icur)->LastUpdated()) {
         child = child->Order().Tail();
         this->RecalcParent(recalc, *child);
         if (child->IsWorking()) {
            ++icur;
            continue;
         }
      }
      else {
         // (*icur)->LastUpdated()==nullptr: abandon
      }
      icur = this->WorkingChildRequests_.erase(icur);
   }
   assert(recalc.RequireQty_ == tail->RequireQty_); // this->RecalcParent() 不應該變動 RequireQty_;
   if (fon9::signed_cast(recalc.LeavesQty_) < 0) {  // this->RecalcParent() 沒有計算 recalc.LeavesQty_, 則必須在這兒算;
      recalc.LeavesQty_ = (recalc.RequireQty_ - recalc.CumQty_);
   }
   assert(recalc.LeavesQty_ >= recalc.ChildLeavesQty_);
   assert(fon9::signed_cast(recalc.LeavesQty_) >= 0);
   if (fon9::signed_cast(recalc.LeavesQty_) < 0)
      recalc.LeavesQty_ = 0;
   if (recalc.LeavesQty_ < recalc.ChildLeavesQty_)
      recalc.LeavesQty_ = recalc.ChildLeavesQty_;

   if (recalc.ChildLeavesQty_ == tail->ChildLeavesQty_
       && recalc.LeavesQty_   == tail->LeavesQty_
       && recalc.CumQty_      == tail->CumQty_
       && recalc.CumAmt_      == tail->CumAmt_
       && recalc.Leg1CumQty_  == tail->Leg1CumQty_
       && recalc.Leg2CumQty_  == tail->Leg2CumQty_
       && recalc.Leg1CumAmt_  == tail->Leg1CumAmt_
       && recalc.Leg2CumAmt_  == tail->Leg2CumAmt_) {
      return nullptr;
   }
   if (auto* newParent = static_cast<OmsParentOrderRaw*>(this->BeginUpdate(*this->Initiator()))) {
      recalc.BeforeQty_ = tail->LeavesQty_;
      recalc.AfterQty_ = recalc.LeavesQty_;
      OmsParentOrderRawDat* afdat = newParent;
      *afdat = recalc;
      if (newParent->LeavesQty_ > 0) {
         // 還有剩餘量 => 母單結束, 等候子單成交.
         // newParent->RequestSt_ = 還有剩餘量 Request 最後狀態不用變;
         if (newParent->CumQty_ == 0) {
            // 還有剩餘量, 目前無成交.
            newParent->UpdateOrderSt_ = f9fmkt_OrderSt_ExchangeAccepted;
         }
         else {
            newParent->UpdateOrderSt_ = f9fmkt_OrderSt_PartFilled;
         }
      }
      else {
         // 重算後,沒有剩餘量 => Request視為[交易所取消].
         newParent->RequestSt_ = f9fmkt_TradingRequestSt_ExchangeCanceled;
         if (newParent->CumQty_ == 0) {
            // 已無剩餘量, 無成交 => 全都取消.
            newParent->UpdateOrderSt_ = f9fmkt_OrderSt_ExchangeCanceled;
         }
         else {
            // 已無剩餘量, 有成交 => 已全部成交.
            newParent->UpdateOrderSt_ = f9fmkt_OrderSt_FullFilled;
         }
      }
      return newParent;
   }
   return nullptr;
}
bool OmsParentOrder::RerunParent(OmsResource& resource) {
   // 母單已啟用 => 重複執行, 沒有額外需要做的事情.
   if (this->IsParentEnabled())
      return false;
   assert(dynamic_cast<const OmsParentOrderRaw*>(this->Tail()) != nullptr);
   if (static_cast<const OmsParentOrderRaw*>(this->Tail())->LeavesQty_ <= 0)
      return false;
   auto* newParent = this->RecalcAllChildForRerun();
   if(newParent) {                      // 重算後, 有異動.
      if (newParent->LeavesQty_ <= 0) { // 但異動後無剩餘量.
         // 無剩餘量, 不用重啟 Parent,
         // 但因為有異動(newParent), 所以需要更新(OmsRequestRunnerInCore);
         OmsRequestRunnerInCore runner{resource, *newParent, 0u};
         return false;
      }
   }
   // 不用考慮 tail->IsFrozeScLeaves_; 因為: 即使已經收盤, 但後續的子單仍需要更新母單, 所以這裡仍需啟用母單.
   this->SetParentRunning();
   bool retval = this->OnRerunParent(resource, newParent);
   // 若 RecalcAllChildForRerun() 有異動: newParent != nullptr;
   // 則不論 this->OnRerunParent() 一定要透過 newParent 寫入母單啟用結果.
   assert(newParent == nullptr || newParent->RxSNO() > 0);
   return retval;
}
bool OmsParentOrder::ParentRerunStep(OmsResource& resource, OmsParentOrderRaw* newParent, OmsRequestRunStep& rerunStep) {
   auto& iniReq = *this->Initiator();
   auto  lastReqSt = iniReq.LastUpdated()->RequestSt_;;
   if (newParent == nullptr) {
      newParent = static_cast<OmsParentOrderRaw*>(this->BeginUpdate(iniReq));
      assert(newParent != nullptr);
      if (newParent == nullptr)
         return false;
   }
   OmsRequestRunnerInCore parentRunner{resource, *newParent};
   this->AssignRerunMessage(newParent->Message_);
   newParent->RequestSt_ = lastReqSt;
   rerunStep.RunRequest(std::move(parentRunner));
   return true;
}
void OmsParentOrder::AssignRerunMessage(fon9::CharVector& message) {
   message.clear();
   fon9::RevPrintAppendTo(message, "Parent rerun@", fon9::LocalHostId_);
}
//--------------------------------------------------------------------------//
OmsParentOrder::OnEachWorkingChild::CheckResult OmsParentOrder::OnEachWorkingChild::CheckNeedsChildChange(const OmsRequestIni& childReqIni) {
   (void)childReqIni;
   return NeedThisChild;
}
void OmsParentOrder::OnEachWorkingChild::SetupChildRequest(OmsRequestRunnerInCore& parentRunner, OmsRequestRunner& childRunner, const OmsRequestIni& childReqIni) {
   auto* reqUpd = static_cast<OmsRequestUpd*>(childRunner.Request_.get());
   reqUpd->SetPolicy(static_cast<const OmsRequestTrade*>(&parentRunner.OrderRaw().Request())->Policy());
   reqUpd->SetIniFrom(childReqIni);
}
void OmsParentOrder::OnEachWorkingChild::OnAbandonBeforeRunChild(OmsRequestRunnerInCore& parentRunner, OmsRequestIni& childReqIni, size_t workingIndex) {
   auto& parentOrder = *static_cast<OmsParentOrder*>(&parentRunner.OrderRaw().Order());
   assert(childReqIni.RxSNO() == 0 && childReqIni.GetParentRequest() != nullptr);
   assert(workingIndex < parentOrder.WorkingChildRequests_.size() && parentOrder.WorkingChildRequests_[workingIndex] == &childReqIni);
   // 刪單: 先解除與 Parent 的關聯, 然後使用 Abandon(), 這樣在 RunInCore() 時, 就不會執行 childReqIni 了!
   if (childReqIni.RxSNO() == 0)
      if (auto* parentIni = childReqIni.GetParentRequest()) {
         parentRunner.OrderRawT<OmsParentOrderRaw>().DecChildParentLeavesQty(parentOrder.GetChildRequestIniQty(childReqIni),
                                                                             childReqIni.ReportSt() != f9fmkt_TradingRequestSt_WaitingRun);
         childReqIni.ClearParentRequest();
         childReqIni.Abandon(OmsErrCode_AbandonBeforeRun,
                             fon9::RevPrintTo<std::string>("Parent=", parentIni->ReqUID_),
                             nullptr, nullptr);
         parentOrder.WorkingChildRequests_.erase(parentOrder.WorkingChildRequests_.begin() + fon9::signed_cast(workingIndex));
      }
}
size_t OmsParentOrder::ChgEachWorkingChild(OmsRequestRunnerInCore&& parentRunner, OmsRequestFactory& chgfac, OnEachWorkingChild& maker) {
   const OmsRequestBase& reqParent = parentRunner.OrderRaw().Request();
   size_t reqCount = 0;
   size_t workingIndex = this->WorkingChildRequests_.size(); // 從後往前尋找需要刪改的子單, 送交易所刪改時, 比較容易成功.
   while (workingIndex > 0) {
      const OmsRequestIni* childReqIni = this->WorkingChildRequests_[--workingIndex];
      switch (maker.CheckNeedsChildChange(*childReqIni)) {
      default:
      case OnEachWorkingChild::NeedThisChild:
         break;
      case OnEachWorkingChild::SkipThisChild:
         continue;
      case OnEachWorkingChild::BreakEachChild:
         return reqCount;
      }
      if (fon9_UNLIKELY(childReqIni->RxSNO() == 0)) {
         maker.DirectChange(parentRunner, *const_cast<OmsRequestIni*>(childReqIni), workingIndex);
         continue;
      }
      OmsRequestRunner  childRunner;
      childRunner.Request_ = chgfac.MakeRequest();
      assert(dynamic_cast<OmsRequestUpd*>(childRunner.Request_.get()) != nullptr);
      if (fon9_UNLIKELY(childRunner.Request_.get() == nullptr)) {
         parentRunner.Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_UnknownRequestType,
                             "Bad child RequestChg factory");
         return reqCount;
      }
      maker.SetupChildRequest(parentRunner, childRunner, *childReqIni);
      // -----
      if (fon9_UNLIKELY(!reqParent.MoveChildRunnerToCore(parentRunner, std::move(childRunner))))
         return reqCount;
      ++reqCount;
   }
   return reqCount;
}
size_t OmsParentOrder::DelEachWorkingChild(OmsRequestRunnerInCore&& parentRunner, OmsRequestFactory& chgfac) {
   struct OnEachWorkingChildDel : public OnEachWorkingChild {
      using base = OnEachWorkingChild;
      void DirectChange(OmsRequestRunnerInCore& parentRunner, OmsRequestIni& childReqIni, size_t workingIndex) override {
         this->OnAbandonBeforeRunChild(parentRunner, childReqIni, workingIndex);
      }
      void SetupChildRequest(OmsRequestRunnerInCore& parentRunner, OmsRequestRunner& childRunner, const OmsRequestIni& childReqIni) override {
         static_cast<OmsRequestUpd*>(childRunner.Request_.get())->SetRxKind(f9fmkt_RxKind_RequestDelete);
         base::SetupChildRequest(parentRunner, childRunner, childReqIni);
      }
   };
   OnEachWorkingChildDel delMaker;
   return this->ChgEachWorkingChild(std::move(parentRunner), chgfac, delMaker);
}
//--------------------------------------------------------------------------//
void OmsParentOrderRaw::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsParentOrderRaw>(flds);
   flds.Add(fon9_MakeField2(OmsParentOrderRaw, RequireQty));
   flds.Add(fon9_MakeField2(OmsParentOrderRaw, BeforeQty));
   flds.Add(fon9_MakeField2(OmsParentOrderRaw, AfterQty));
   flds.Add(fon9_MakeField2(OmsParentOrderRaw, LeavesQty));
   flds.Add(fon9_MakeField2(OmsParentOrderRaw, ChildLeavesQty));
   flds.Add(fon9_MakeField2(OmsParentOrderRaw, CumQty));
   flds.Add(fon9_MakeField2(OmsParentOrderRaw, CumAmt));
   flds.Add(fon9_MakeField2(OmsParentOrderRaw, Leg1CumQty));
   flds.Add(fon9_MakeField2(OmsParentOrderRaw, Leg1CumAmt));
   flds.Add(fon9_MakeField2(OmsParentOrderRaw, Leg2CumQty));
   flds.Add(fon9_MakeField2(OmsParentOrderRaw, Leg2CumAmt));
}
void OmsParentOrderRaw::ContinuePrevUpdate(const OmsOrderRaw& prev) {
   base::ContinuePrevUpdate(prev);
   OmsParentOrderRawDat::ContinuePrevUpdate(*static_cast<const OmsParentOrderRaw*>(&prev));
}
void OmsParentOrderRaw::OnOrderReject() {
   assert(f9fmkt_OrderSt_IsFinishedRejected(this->UpdateOrderSt_));
   assert(this->ChildLeavesQty_ == 0); // 只有在尚未建立子單之前, 才有可能使用 OnOrderReject();
   this->AfterQty_ = this->LeavesQty_ = this->ChildLeavesQty_;
}
bool OmsParentOrderRaw::IsWorking() const {
   return this->LeavesQty_ > 0;
}
OmsFilledFlag OmsParentOrderRaw::HasFilled() const {
   return this->CumQty_ > 0 ? OmsFilledFlag::HasFilled : OmsFilledFlag::None;
}

} // namespaces
