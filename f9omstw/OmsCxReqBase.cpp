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
