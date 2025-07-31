// \file f9omstw/OmsCxReqBase.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCxReqBase.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsCxReqBase::MakeFields(fon9::seed::Fields& flds, int ofsadj) {
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxReqBase, CxArgs));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxReqBase, CondAfterSc));
}
//--------------------------------------------------------------------------//
OmsCxReqBase::~OmsCxReqBase() {
   delete this->CxExpr_;
}
bool OmsCxReqBase::ToWaitCond_BfSc(OmsRequestRunnerInCore&& runner, OmsCxBaseRaw& cxRaw, OmsRequestRunStep& condNextStep) const {
   auto& ordraw = runner.OrderRaw();
   const f9fmkt_TradingRequestSt reqst = ordraw.RequestSt_;
   if (fon9_LIKELY(!this->CxExpr_)) {
      if (fon9_LIKELY(reqst != f9fmkt_TradingRequestSt_WaitingCond))
         return false;
      // Rerun: 重建 CxExpr_; 重新進入 WaitingCond 狀態;
      if (const_cast<OmsCxReqBase*>(this)->RecreateCxExpr(runner)) {
         // 重建 CxExpr 成功, 執行條件檢查流程, 必要時進入條件等候狀態;
         ordraw.ErrCode_ = OmsErrCode_ReWaitingCond;
         goto __TO_WAIT_COND;
      }
      // 重建 CxExpr_ 失敗, 返回 true 中斷下單流程;
      return true;
   }
   assert(this->CxUnitCount_ > 0 && this->FirstCxUnit_ != nullptr);
   if (fon9_LIKELY(reqst < f9fmkt_TradingRequestSt_WaitToCheck)) {
      if (fon9_LIKELY(this->CxUnitCount_ <= 1)) {
         cxRaw = this->FirstCxUnit_->Cond_;
      }
   __TO_WAIT_COND:;
      if (this->CondAfterSc_ == fon9::EnabledYN::Yes) // 先風控,之後才開始等候條件.
         return false;
      // 先等候條件, 條件成立後才風控.
      // 風控前, 加入 CondSymb 開始等候條件.
      if (this->RegCondToSymb(runner, kPriority_BfSc, condNextStep)) {
         this->OnWaitCondBfSc(runner);
         this->OrderUpdateWaitCondSt(std::move(runner));
         return true;
      }
   }
   // 條件已成立 or 此次執行(因reqst)不用等條件 => 則直接進行下一步驟.
   return false;
}
bool OmsCxReqBase::ToWaitCond_AfSc(OmsRequestRunnerInCore&& runner, OmsRequestRunStep& condNextStep) const {
   if (this->CxExpr_ && this->CondAfterSc_ == fon9::EnabledYN::Yes) {
      if (runner.OrderRaw().RequestSt_ < f9fmkt_TradingRequestSt_WaitToGoOut) {
         // 風控後, 加入 CondSymb 開始等候條件.
         // 風控後, 尚開始未等候的條件單 => 此時開始等候條件.
         if (this->RegCondToSymb(runner, kPriority_AfSc, condNextStep)) {
            this->OnWaitCondAfSc(runner);
            this->OrderUpdateWaitCondSt(std::move(runner));
            return true;
         }
      }
   }
   return false;
}
bool OmsCxReqBase::ToWaitCond_ReqChg(OmsCxBaseChgDat* pthisCond, OmsRequestRunnerInCore&& runner, OmsRequestRunStep& condNextStep) const {
   auto& ordraw = runner.OrderRaw();
   const f9fmkt_TradingRequestSt reqst = ordraw.RequestSt_;
   if (fon9_LIKELY(!this->CxExpr_)) {
      if (fon9_LIKELY(reqst != f9fmkt_TradingRequestSt_WaitingCond)) {
         // 一般改單(非條件改單), 返回 false, 繼續處理一般改單流程;
         return false;
      }
      // Rerun: 重建 CxExpr_; 重新進入 WaitingCond 狀態;
      if (const_cast<OmsCxReqBase*>(this)->RecreateCxExpr(runner)) {
         // 重建 CxExpr 成功, 執行條件檢查流程, 必要時進入條件等候狀態;
         ordraw.ErrCode_ = OmsErrCode_ReWaitingCond;
         goto __TO_WAIT_COND;
      }
      // 重建 CxExpr_ 失敗, 返回 true 中斷下單流程;
      return true;
   }
   if (fon9_LIKELY(reqst < f9fmkt_TradingRequestSt_WaitToCheck)) {
   __TO_WAIT_COND:;
      if (fon9_LIKELY(this->CxUnitCount_ <= 1)) {
         if (ordraw.Request().RxKind() == f9fmkt_RxKind_RequestChgCond) {
            // 因為 rthis 的 CondPri, CondQty 需要填入 CxUnit 的內容,
            // 所以不允許 [rthis:條件改單] 改 ini 的條件;
            ordraw.ErrCode_ = OmsErrCode_Bad_RxKind;
            ordraw.RequestSt_ = f9fmkt_TradingRequestSt_InternalRejected;
            return true;
         }
         if (pthisCond)
            *pthisCond = this->FirstCxUnit_->Cond_;
      }
      // 先等候條件, 條件成立後才風控.
      // 風控前, 加入 CondSymb 開始等候條件.
      const auto priorityAdj = (f9fmkt_OrderSt_IsAfterOrEqual(ordraw.UpdateOrderSt_, f9fmkt_OrderSt_NewAlreadyGoOut)
                                ? kPriority_ReqChg
                                : kPriority_ChgNWC);
      if (this->RegCondToSymb(runner, priorityAdj, condNextStep)) {
         ordraw.RequestSt_ = f9fmkt_TradingRequestSt_WaitingCond;
         return true;
      }
   }
   // 條件已成立 or 此次執行(因reqst)不用等條件 => 則直接進行下一步驟.
   return false;
}
// ======================================================================== //
OmsCxCheckResult OmsCxReqBase::OnOmsCx_CalcExpr(OmsCxUnit& cxUnit, OnOmsCxMdEvFnT onOmsCxEvFn, OmsCxMdEvArgs& args) const {
   if (fon9_LIKELY(this->CxUnitCount() == 1)) {
      assert(this->FirstCxUnit_ == &cxUnit);
      if (cxUnit.OnOmsCx_CallEvFn(onOmsCxEvFn, args)) {
         this->CxExpr_->SetCxReqSt(CxReqSt_Fired);
         return OmsCxCheckResult::FireNow;
      }
      return OmsCxCheckResult::ContinueWaiting;
   }
   CxExprLocker expr(*this);
   if (expr->GetCxReqSt() >= CxReqSt_Fired)
      return OmsCxCheckResult::AlreadyFired;
   /// 行情異動檢查(同商品串列).
   /// (1)如果有任一個 CxUnit 從 0 => 1, 則 expr 要重算;
   /// (2)如果 SameSymb list 全部變成 TrueLocked 狀態, 則從 symb 移除異動通知.
   bool hasChangeToTrue = false;
   bool hasNotTrueLocked = false;
   for (OmsCxUnit* node = &cxUnit; node != nullptr; node = node->CondSameSymbNext()) {
      const OmsCxUnitSt bfSt = node->CurrentSt();
      args.CxRaw_ = &node->Cond_;
      node->OnOmsCx_CallEvFn(onOmsCxEvFn, args);
      const OmsCxUnitSt afSt = node->CurrentSt();
      if (bfSt <= OmsCxUnitSt::FalseAndWaiting && OmsCxUnitSt::TrueAndWaiting <= afSt) {
         hasChangeToTrue = true;
      }
      if (afSt != OmsCxUnitSt::TrueLocked) {
         hasNotTrueLocked = true;
      }
   }

   if (hasChangeToTrue) {
      if (expr->GetCxReqSt() != CxReqSt_Waiting) {
         assert(expr->GetCxReqSt() == CxReqSt_Initialing);
         return OmsCxCheckResult::ContinueWaiting;
      }
      // (0 => 1) 則 expr 可能會成立, 所以要重算.
      if (expr->IsTrue()) {
         this->CxExpr_->SetCxReqSt(CxReqSt_Fired);
         return OmsCxCheckResult::FireNow;
      }
   }
   return hasNotTrueLocked ? OmsCxCheckResult::ContinueWaiting : OmsCxCheckResult::TrueLocked;
}
// ======================================================================== //

} // namespaces
