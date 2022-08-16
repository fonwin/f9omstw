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
   this->GetChildOrderQtys(childOrdraw, childLeavesQty, childAdjQty);
   if (childAdjQty >= 0) // 不可能增量!
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
      reqSt = reqFrom->LastUpdated()->RequestSt_;
   }
   if (childLeavesQty == 0) {
      this->EraseChild(childOrdraw.Order().Initiator());
   }
   // -----
   OmsRequestRunnerInCore parentRunner{res, *this->BeginUpdate(*reqFrom), childRunner};
   auto& parentOrdraw = parentRunner.OrderRawT<OmsParentOrderRaw>();
   parentOrdraw.DecChildParentLeavesQty(fon9::unsigned_cast(-childAdjQty));
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
      // 手動刪改子單 or 未成交刪單(fa:ExchangeCanceled, OmsErrCode_SessionClosed);
      parentOrdraw.ErrCode_ = childOrdraw.ErrCode_;
      if (parentOrdraw.ErrCode_ == OmsErrCode_SessionClosed) {
         parentOrdraw.LeavesQty_ = parentOrdraw.ChildLeavesQty_;
         this->ClearParentWorking();
      }
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
   // TODO: 如果[原母單主機]已死, childRunner 是來自[非原母單主機]的刪改,
   //       接下來不會有母單回報, 此時應該要處理 childRunner 的異動.
   //       但是要如何判斷呢?
   //       解析 childRunner.OrderRaw().Request().ReqUID_; 與 this->Initiator()->ReqUID_;
   //       取得 HostId 來判斷?
   if (!childRunner.OrderRaw().Request().IsSynReport()) {
      this->RunChildOrderUpdated(childRunner.OrderRaw(), childRunner.Resource_, &childRunner, nullptr,
         [](OmsParentOrder& parentOrder, const OmsRequestBase& parentIni, OmsRequestRunnerInCore&& parentRunner, void* udata) {
            (void)udata;
            parentOrder.OnAfterChildOrderUpdated(parentIni, std::move(parentRunner));
      });
   }
}
void OmsParentOrder::OnAfterChildOrderUpdated(const OmsRequestBase& parentIni, OmsRequestRunnerInCore&& parentRunner) {
   (void)parentIni, (void)parentRunner;
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
         parentRunner.OrderRawT<OmsParentOrderRaw>().DecChildParentLeavesQty(parentOrder.GetChildRequestIniQty(childReqIni));
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
   flds.Add(fon9_MakeField2(OmsParentOrderRaw, ChildLeavesQty));
   flds.Add(fon9_MakeField2(OmsParentOrderRaw, LeavesQty));
   flds.Add(fon9_MakeField2(OmsParentOrderRaw, BeforeQty));
   flds.Add(fon9_MakeField2(OmsParentOrderRaw, AfterQty));
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

} // namespaces
