// \file f9omstw/OmsCxReqBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCxReqBase_hpp__
#define __f9omstw_OmsCxReqBase_hpp__
#include "f9omstw/OmsCxExpr.hpp"
#include "f9omstw/OmsCxCombine.hpp"
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {

enum class OmsCxCheckResult {
   ContinueWaiting,
   AlreadyFired,
   TrueLocked,
   FireNow,
};
// OmsCxReqBase 內部使用, 輔助建構 CxExpr;
struct OmsCxCreatorInternal;
//--------------------------------------------------------------------//
class OmsCxReqBase {
   fon9_NON_COPY_NON_MOVE(OmsCxReqBase);

   OmsCxUnit*        FirstCxUnit_{};
   /// Y=先風控,然後等條件,條件成立後執行風控的下一步驟(送單);
   fon9::EnabledYN   CondAfterSc_{};
   bool              IsAllowChgCond_{false};
   uint16_t          CxUnitCount_{0};
   uint16_t          CxSymbCount_{0};
   char              Padding_______[2];

   enum CxReqSt : uint8_t {
      CxReqSt_Initialing = 0,
      CxReqSt_Waiting = 1,
      CxReqSt_Fired = 2,
   };
   struct CxExpress : public OmsCxExpr {
      fon9_NON_COPY_NON_MOVE(CxExpress);
      using base = OmsCxExpr;
      std::mutex  Mutex_;
      CxExpress(OmsCxExpr&& from) : base(std::move(from)) {
         this->SetCxReqSt(CxReqSt_Initialing);
      }
      void SetCxReqSt(CxReqSt v) { this->CxReqSt_ = static_cast<uint8_t>(v); }
      CxReqSt GetCxReqSt() const { return static_cast<CxReqSt>(this->CxReqSt_); }
   };
   CxExpress* CxExpr_{nullptr};

   class CxExprLocker {
      fon9_NON_COPYABLE(CxExprLocker);
      CxExpress*  Expr_;
   public:
      ~CxExprLocker() {
         if (this->Expr_)
            this->Expr_->Mutex_.unlock();
      }
      CxExprLocker(const OmsCxReqBase& owner) : Expr_{owner.CxExpr_} {
         if (this->Expr_)
            this->Expr_->Mutex_.lock();
      }
      CxExprLocker(CxExprLocker && from) : Expr_(from.Expr_) {
         from.Expr_ = nullptr;
      }
      CxExprLocker& operator=(CxExprLocker&& from) = delete;

      CxExpress* operator->() const {
         return this->Expr_;
      }
      CxExpress& operator*() const {
         return *this->Expr_;
      }
   };

protected:
   /// 由 Symb 資料異動時呼叫, 檢查條件是否成立.
   /// CxSymb 會根據返回值決定是否繼續等候, 或移除等候, 或觸發執行.
   OmsCxCheckResult OnOmsCx_CalcExpr(OmsCxUnit& cxUnit, OnOmsCxMdEvFnT onOmsCxEvFn, OmsCxMdEvArgs& args) const;

public:
   /// 使用字串來提供條件, 例: "2330.LP>=1000"
   fon9::CharVector  CxArgs_;

   OmsCxReqBase() = default;
   virtual ~OmsCxReqBase();

   template <class Derivid>
   static void CxMakeFields(fon9::seed::Fields& flds, const Derivid* = nullptr) {
      MakeFields(flds, fon9_OffsetOfBase(Derivid, OmsCxReqBase));
   }
   static void MakeFields(fon9::seed::Fields& flds, int ofsadj);

   bool IsCondAfterSc() const { return this->CondAfterSc_ == fon9::EnabledYN::Yes; }
   bool IsAllowChgCond() const { return this->IsAllowChgCond_; }
   uint16_t CxUnitCount() const { return this->CxUnitCount_; }
   // --------------------------------------------------------------------- //
   /// txReq 必定為 this 的衍生;
   /// 當條件沒有設定 SymbId 時, 使用 [下單要求] 的商品 txSymb;
   bool BeforeReqInCore_CreateCxExpr(OmsRequestRunner& runner, OmsResource& res, OmsSymbSP txSymb);
   bool RecreateCxExpr(OmsRequestRunnerInCore& runner);
   bool InternalCreateCxExpr(OmsCxCreatorInternal& args);
   // --------------------------------------------------------------------- //
   /// 實作方式請參考 OnOmsCxMdEv_T();
   virtual OmsCxCheckResult OnOmsCxMdEv(const OmsCxReqToSymb& creq, OnOmsCxMdEvFnT onOmsCxEvFn, OmsCxMdEvArgs& args) const = 0;
   /// TCxReq 須支援:
   ///   // 若 order 及條件單仍有效, 則取出判斷用的參數(CondPri_, CondQty_);
   ///   const OmsCxBaseRaw* GetWaitingCxRaw() const;
   template <class CoreUsrDef, class TCxReq>
   static OmsCxCheckResult OnOmsCxMdEv_T(const TCxReq& rthis, const OmsCxReqToSymb& creq, OnOmsCxMdEvFnT onOmsCxEvFn, OmsCxMdEvArgs& args, const CoreUsrDef*) {
      // 若 order 仍然有效, 則取出條件要判斷的內容參數;
      if ((args.CxRaw_ = rthis.GetWaitingCxRaw()) == nullptr)
         return OmsCxCheckResult::AlreadyFired;
      // ----- 檢查條件是否成立?
      OmsCxCheckResult retval = rthis.OnOmsCx_CalcExpr(*creq.CxUnit_, onOmsCxEvFn, args);
      if (fon9_UNLIKELY(retval == OmsCxCheckResult::FireNow)) {
         // ----- 將 req 移到 [等候執行] 列表, 等候 OmsCore 的執行.
         assert(dynamic_cast<CoreUsrDef*>(args.OmsCore_->GetUsrDef()) != nullptr);
         static_cast<CoreUsrDef*>(args.OmsCore_->GetUsrDef())->AddCondFiredReq(*args.OmsCore_, rthis, *creq.NextStep_, ToStrView(args.LogRBuf_));
      }
      return retval;
   }
   // --------------------------------------------------------------------- //
   /// 由 Policy 取得使用者的條件單優先權.
   /// 考慮下單價格是否為市價;
   template <class Policy>
   static uint32_t GetCondPriority_T(Policy& policy, f9fmkt_PriType priType) {
      return(priType == f9fmkt_PriType_Market ? policy.CondPriorityM() : policy.CondPriorityL());
   }

   static constexpr uint64_t kPriority_AfSc   = (0x110ULL << 32); // 已風控的條件單優先檢查.
   static constexpr uint64_t kPriority_BfSc   = (0x120ULL << 32); // 未風控的條件單排在後面.
   static constexpr uint64_t kPriority_ReqChg = (0x130ULL << 32);
   // 條件刪改 [等候條件的新單], 優先權需在新單之前, 這樣可以在 [新單、改單] 同時成立時, 先執行改單;
   static constexpr uint64_t kPriority_ChgNWC = (0x100ULL << 32);

   static void OrderUpdateWaitCondSt(OmsRequestRunnerInCore&& runner) {
      runner.OrderRaw().UpdateOrderSt_ = f9fmkt_OrderSt_NewWaitingCond;
      runner.Update(f9fmkt_TradingRequestSt_WaitingCond);
   }
   // --------------------------------------------------------------------- //
   virtual bool RegCondToSymb(OmsRequestRunnerInCore& runner, uint64_t priorityAdj, OmsRequestRunStep& nextStep) const = 0;
   /// CxSymb 必須支援:
   /// - void RegCondReq(const MdLocker& mdLocker, CondReq& src);
   /// \retval true  註冊成功,等候條件成立.
   /// \retval false 沒有註冊,條件已成立.
   template <class CxSymb>
   bool RegCondToSymb_T(OmsRequestRunnerInCore& runner, uint64_t priority, OmsRequestRunStep& nextStep, const CxSymb*) const {
      assert(this->FirstCxUnit_ && this->CxExpr_);
      if (this->CxExpr_->GetCxReqSt() == CxReqSt_Waiting) {
         runner.OrderRaw().ErrCode_ = OmsErrCode_ReWaitingCond;
         return true;
      }
      typename CxSymb::CondReq creq;
      creq.CxRequest_ = this;
      creq.Priority_ = priority;
      creq.NextStep_ = &nextStep;

      OmsCxUnit*        first = this->FirstCxUnit_;
      CxSymb*           symb = static_cast<CxSymb*>(first->CondSymb_.get());
      OmsSymb::MdLocker mdLocker{*symb};
      bool              isCondTrue;
      if (fon9_LIKELY(this->CxUnitCount_ == 1)) {
         assert(first->CondSameSymbNext() == nullptr && first->CondOtherSymbNext() == nullptr);
         isCondTrue = first->CheckNeedsFireNow(*symb, first->Cond_);
      __COND_SINGLE_SYMB:;
         if (isCondTrue) { // 條件成立.
            this->CxExpr_->SetCxReqSt(CxReqSt_Fired);
            symb->PrintMd(runner.ExLogForUpd_, first->RegEvMask());
            return false; // 直接執行, 不需要 Symb MdEv;
         }
         this->CxExpr_->SetCxReqSt(CxReqSt_Waiting);
         creq.CxUnit_ = first;
         symb->RegCondReq(mdLocker, creq);
         return true;
      }
      // 組合條件, 註冊 Symb MdEv.
      // symb->RegCondReq() 之後, 該 Symb MdEv 會呼叫 OnOmsCxMdEv() 檢查.
      // 有可能還在這個 fn 尚未結束, 就觸發 OnOmsCxMdEv() 事件.
      for (;;) {
         bool hasFalseAndWaiting = false;
         bool hasTrueAndWaiting = false;
         bool hasTrueLocked = false;
         for (OmsCxUnit* node = first; node != nullptr; node = node->CondSameSymbNext()) {
            node->CheckNeedsFireNow(*symb, node->Cond_);
            switch (node->CurrentSt()) {
            default:
            case OmsCxUnitSt::Initialing:        break;
            case OmsCxUnitSt::FalseAndWaiting:   hasFalseAndWaiting = true; break;
            case OmsCxUnitSt::TrueAndWaiting:    hasTrueAndWaiting = true;  break;
            case OmsCxUnitSt::TrueLocked:        hasTrueLocked = true;      break;
            }
         }
         if (fon9_LIKELY(this->CxSymbCount_ == 1)) {
            assert(this->FirstCxUnit_ == first && first->CondOtherSymbNext() == nullptr);
            if (hasTrueAndWaiting || hasTrueLocked) {
               // 單一商品, 若 CxUnit 有任一成立, 則判斷 expr 是否成立,
               // 若 expr 成立, 則直接執行, 不用註冊 Symb MdEv.
               // 因為是單一商品, 且尚未註冊, 所以 this->CxExpr_ 不用 lock, 可直接使用;
               isCondTrue = this->CxExpr_->IsTrue();
               goto __COND_SINGLE_SYMB;
            }
            assert(hasFalseAndWaiting); // 必定有 hasFalseAndWaiting, 所以接下來就會註冊 Symb MdEv;
         }
         if (hasFalseAndWaiting || hasTrueAndWaiting) {
            creq.CxUnit_ = first;
            symb->RegCondReq(mdLocker, creq);
         }
         if ((first = first->CondOtherSymbNext()) == nullptr)
            break;
         symb = static_cast<CxSymb*>(first->CondSymb_.get());
         mdLocker.Lock(*symb);
      }
      mdLocker.Unlock();
      // 所有相關商品皆已註冊 MdEv
      CxExprLocker expr{*this};
      if (expr->IsTrue()) {
         // 理論上應取消 Symb MdEv, 但實際運作時, 就等 Symb MdEv: OnOmsCxMdEv() 返回後移除.
         expr->SetCxReqSt(CxReqSt_Fired);
         return false;
      }
      expr->SetCxReqSt(CxReqSt_Waiting);
      return true;
   }
   /// 通知: 風控前,已進入等候狀態.
   /// - 請參考 RequestNew_SetBfSc_T<>();
   /// - 等條件成立後,在風控步驟,設定數量/價格...
   ///   - BeforeQty = 0;
   ///   - AfterQty = LeavesQty;
   virtual void OnWaitCondBfSc(OmsRequestRunnerInCore& runner) const = 0;

   /// 取得 Req 的 [剩餘量]
   template <typename TQty, class TxReq> // TxReq = Report: 回報 Rerun/Force 使用的是 LeavesQty
   static auto AssignReqLeavesQty(TQty& qty, const TxReq* req) -> decltype(req->LeavesQty_, bool()) {
      qty = req->LeavesQty_;
      return true;
   }
   static bool AssignReqLeavesQty(...) {
      return false;
   }
   /// 在尚未執行風控前, 進入等候狀態, 此時設定相關的 Qty:
   /// ordraw.BeforeQty_ = ordraw.AfterQty_ = 0;
   /// ordraw.LeavesQty_ = reqLeavesQty;
   template <class TOrdRaw, class TReq>
   static void RequestNew_SetBfSc_T(TOrdRaw& ordraw, const TReq& req) {
      ordraw.BeforeQty_ = ordraw.AfterQty_ = 0;
      // 風控前等條件: 進入條件等候前(可能之前是 WaitingRun)
      // 可能有設定過 LeavesQty, 此時不可重複設定 LeavesQty,
      // 因為: 可能有改過數量;
      if (ordraw.LeavesQty_ == 0) {
         if (!AssignReqLeavesQty(ordraw.LeavesQty_, &req))
            ordraw.LeavesQty_ = req.Qty_;
      }
      if (ordraw.LastPriType_ == f9fmkt_PriType_Unknown)
         ordraw.LastPriType_ = req.PriType_;
      if (ordraw.LastPri_.IsNull())
         ordraw.LastPri_ = req.Pri_;
   }
   template <class TCxReq>
   static bool ToWaitCond_BfSc(const TCxReq& rthis, OmsRequestRunnerInCore&& runner, OmsCxBaseRaw& cxRaw, OmsRequestRunStep& condNextStep) {
      runner.OrderRaw().Order().GetSymb(runner.Resource_, rthis.Symbol_);
      return rthis.GetCxReq()->ToWaitCond_BfSc(std::move(runner), cxRaw, condNextStep);
   }
   bool ToWaitCond_BfSc(OmsRequestRunnerInCore&& runner, OmsCxBaseRaw& cxRaw, OmsRequestRunStep& condNextStep) const;
   // --------------------------------------------------------------------- //
   virtual void OnWaitCondAfSc(OmsRequestRunnerInCore& runner) const = 0;
   bool ToWaitCond_AfSc(OmsRequestRunnerInCore&& runner, OmsRequestRunStep& condNextStep) const;
   // --------------------------------------------------------------------- //
   /// 條件改單.
   template <class TCxReq, class ReqIniForSymbT>
   static bool ToWaitCond_ReqChg(const TCxReq& rthis, OmsRequestRunnerInCore&& runner, OmsRequestRunStep& condNextStep, const ReqIniForSymbT& reqIniForSymb) {
      runner.OrderRaw().Order().GetSymb(runner.Resource_, reqIniForSymb.Symbol_);
      return rthis.GetCxReq()->ToWaitCond_ReqChg(const_cast<TCxReq*>(&rthis), std::move(runner), condNextStep);
   }
   bool ToWaitCond_ReqChg(OmsCxBaseChgDat* pthisCond, OmsRequestRunnerInCore&& runner, OmsRequestRunStep& condNextStep) const;
   // --------------------------------------------------------------------- //
   OmsErrCode OnOmsCx_AssignReqChgToOrd(const OmsCxBaseChgDat& reqChg, OmsCxBaseRaw& cxRaw) const {
      assert(this->CxUnitCount_ == 1 && this->FirstCxUnit_ != nullptr);
      if (this->CxUnitCount_ == 1) {
         return this->FirstCxUnit_->OnOmsCx_AssignReqChgToOrd(reqChg, cxRaw);
      }
      // 多條件單不允許改條件.
      return OmsErrCode_RequestNotSupportThisOrder;
   }
   /// 改條件內容後, 檢查條件是否成立? 若成立則立即觸發執行.
   /// CxSymb 必須支援:
   /// - OmsRequestRunStep* DelCondReq (const MdLocker& mdLocker, const OmsCxUnit& cxUnit); // 可參考 OmsCxSymb;
   /// - void               PrintMd    (fon9::RevBuffer& rbuf, OmsCxSrcEvMask evMask);
   template <class CoreUsrDef, class CxSymb>
   void OnAfterChgCond_Checker_T(const OmsRequestTrade& cxreq, OmsRequestRunnerInCore&& runner, const OmsCxBaseRaw& cxRaw, const CoreUsrDef*, const CxSymb*) const {
      assert(this->CxUnitCount_ == 1 && this->CxExpr_ && this->FirstCxUnit_);
      OmsCxUnit*  first = this->FirstCxUnit_;
      CxSymb*     symb = static_cast<CxSymb*>(first->CondSymb_.get());
      OmsSymb::MdLocker mdLocker{*symb};
      if (!first->CheckNeedsFireNow(*symb, cxRaw)) { // 條件不成立, 不用觸發執行.
         runner.Update(f9fmkt_TradingRequestSt_Done);
         return;
      }
      // 條件成立, 觸發執行.
      if (OmsRequestRunStep* nextStep = symb->DelCondReq(mdLocker, *first)) {
         fon9::RevBufferFixedSize<256> rbuf;
         this->CxExpr_->SetCxReqSt(CxReqSt_Fired);
         symb->PrintMd(rbuf, first->RegEvMask());
         mdLocker.Unlock();
         static_cast<CoreUsrDef*>(runner.Resource_.UsrDef_.get())->AddCondFiredReq(runner.Resource_.Core_, cxreq, *nextStep, ToStrView(rbuf));
         runner.Update(f9fmkt_TradingRequestSt_Done);
      }
      else {
         // 可能在 Symb MD thread 已觸發, 所以 symb->DelCondReq() 不一定成功.
         // runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_RequestStepNotFound, nullptr);
      }
   }
};
// ======================================================================== //
/// 檢查 條件單要求(txReq) 是否仍需要等候條件.
/// 新單尚未送出前, 可能會 [更改條件內容], 此時新單的 txReq.LastUpdated() 不是更改後的內容.
/// 所以委託的最後一次異動 order.Tail(), 才是最後的[新單條件內容];
/// 但這種判斷方式:
/// - 不支援 [新單還在等候條件] 時的 [條件改單], 因為 order.Tail() 可能會是 [條件改單] 的內容;
/// - 或不允許 [條件改單] 變動 ordraw, 也就是 [條件改單的條件內容] 不允許變動;
///
/// \retval !=nullptr  仍在等候條件成立後執行下單(送出).
/// \retval ==nullptr  條件已取消, 或 Order 已失效(剩餘量<=0).
static inline const OmsOrderRaw* GetWaitingOrdRaw(const OmsRequestTrade& txReq) {
   if (const OmsOrderRaw* ordraw = txReq.LastUpdated()) {
      auto& order = ordraw->Order();
      if (order.IsWorkingOrder()) {
         if (txReq.RxKind() == f9fmkt_RxKind_RequestNew) {
            ordraw = order.Tail();
            if (ordraw->UpdateOrderSt_ == f9fmkt_OrderSt_NewWaitingCond)
               return ordraw;
         }
         else if (ordraw->RequestSt_ == f9fmkt_TradingRequestSt_WaitingCond)
            return ordraw;
      }
   }
   return nullptr;
}
// ======================================================================== //
template <class TxReqBase, class TOrdRaw, class TCoreUsrDef, class TSymb>
class OmsCxCombineReqIni : public OmsCxCombine<TxReqBase, OmsCxReqBase> {
   fon9_NON_COPY_NON_MOVE(OmsCxCombineReqIni);
   using base = OmsCxCombine<TxReqBase, OmsCxReqBase>;

public:
   using base::base;
   OmsCxCombineReqIni() = default;
   // --------------------------------------------------------------------- //
   const OmsCxReqBase* GetCxReq() const override { return this; }
   // --------------------------------------------------------------------- //
   /// 若 order 及條件單仍有效, 則取出判斷用的參數(CondPri_, CondQty_);
   const TOrdRaw* GetWaitingCxRaw() const {
      return static_cast<const TOrdRaw*>(f9omstw::GetWaitingOrdRaw(*this));
   }
   OmsCxCheckResult OnOmsCxMdEv(const OmsCxReqToSymb& creq, OnOmsCxMdEvFnT onOmsCxEvFn, OmsCxMdEvArgs& args) const override {
      return this->OnOmsCxMdEv_T(*this, creq, onOmsCxEvFn, args, static_cast<TCoreUsrDef*>(nullptr));
   }
   // --------------------------------------------------------------------- //
   OmsOrderRaw* BeforeReqInCore(OmsRequestRunner& runner, OmsResource& res) override {
      return (this->BeforeReqInCore_CreateCxExpr(runner, res, res.Symbs_->GetOmsSymb(ToStrView(this->Symbol_)))
              ? base::BeforeReqInCore(runner, res)
              : nullptr);
   }
   uint32_t GetCondPriority() const {
      return this->GetCondPriority_T(*this->Policy(), this->PriType_);
   }
   bool RegCondToSymb(OmsRequestRunnerInCore& runner, uint64_t priorityAdj, OmsRequestRunStep& nextStep) const override {
      return this->RegCondToSymb_T(runner, priorityAdj + this->GetCondPriority(), nextStep, static_cast<TSymb*>(nullptr));
   }
   // --------------------------------------------------------------------- //
   void OnWaitCondBfSc(OmsRequestRunnerInCore& runner) const override {
      this->RequestNew_SetBfSc_T(*static_cast<TOrdRaw*>(&runner.OrderRaw()), *this);
   }
   void OnWaitCondAfSc(OmsRequestRunnerInCore& runner) const override {
      (void)runner;
   }
};

} // namespaces
#endif//__f9omstw_OmsCxReqBase_hpp__
