// \file f9utw/UtwRequests.hpp
// \author fonwinz@gmail.com
#ifndef __f9utw_UtwRequests_hpp__
#define __f9utw_UtwRequests_hpp__
#include "f9omstws/OmsTwsRequest.hpp"
#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstwf/OmsTwfRequest1.hpp"
#include "f9omstwf/OmsTwfOrder1.hpp"
#include "f9omstw/OmsCxRequest.hpp"
#include "f9omstw/OmsResource.hpp"

namespace f9omstw {

// ======================================================================== //
struct UtwOmsCxIni : public OmsCxBaseIniFn {
   fon9_NON_COPY_NON_MOVE(UtwOmsCxIni);
   UtwOmsCxIni() = default;
   fon9::intrusive_ptr<UtwsSymb> CondSymb_;
   virtual fon9::StrView GetTxSymbId() const = 0;

   bool BeforeReqInCore_CheckCond(OmsRequestRunner& runner, OmsResource& res, const OmsRequestPolicy* pol) {
      if (this->CondName_.empty1st())
         return true;
      OmsErrCode ec;
      if (fon9_UNLIKELY(pol == nullptr || !pol->IsCondAllowed(ToStrView(this->CondName_)))) {
         ec = OmsErrCode_CondSc_Deny;
         goto __REQUEST_ABANDON;
      }
      ec = this->CreateCondFn();
      if (fon9_LIKELY(ec == OmsErrCode_Null)) {
         if (this->CondFn_ == nullptr) // 非條件單.
            return true;
         if (this->CondSymbId_.empty()) {
            // 沒提供條件判斷用的商品Id => 使用 [下單商品] 判斷條件;
            // => 在 this->RunCondStepBfSc() 設定 this->CondSymb_;
         }
         else {
            this->CondSymb_ = fon9::static_pointer_cast<UtwsSymb>(res.Symbs_->GetOmsSymb(ToStrView(this->CondSymbId_)));
            if (!this->CondSymb_) {
               ec = OmsErrCode_CondSc_SymbNotFound;
               goto __REQUEST_ABANDON;
            }
         }
         // ... 這裡可以加上其他檢查 ...
         return true;
      }
   __REQUEST_ABANDON:;
      runner.RequestAbandon(&res, ec);
      return false;
   }

   static constexpr int kPriority_BfSc = 1;
   static constexpr int kPriority_AfSc = 0; // 已風控的條件單優先檢查.
   /// 風控前,已進入等候狀態.
   /// - 因為尚未進行風控, 所以應設定 ordraw.BeforeQty_ = ordraw.AfterQty_ = ordraw.LeavesQty_ = reqQty;
   /// - 等條件成立後,在風控步驟,設定數量:
   ///   - BeforeQty = 0;
   ///   - AfterQty = LeavesQty;
   virtual void OnWaitCondBfSc(OmsRequestRunnerInCore&& runner) const = 0;
   void RunCondStepBfSc(OmsRequestRunnerInCore&& runner, OmsCxBaseOrdRaw& cxRaw, OmsRequestRunStep& nextStep) const {
      if (this->CondFn_) {
         if (!this->CondSymb_) { // 使用 [下單商品] 判斷條件;
            OmsOrder& order = runner.OrderRaw().Order();
            const_cast<UtwOmsCxIni*>(this)->CondSymb_ = static_cast<UtwsSymb*>(order.GetSymb(runner.Resource_, this->GetTxSymbId()));
            if (!this->CondSymb_) {
               runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_CondSc_SymbNotFound, nullptr);
               return;
            }
         }
         if (this->CondAfterSc_ == fon9::EnabledYN::Yes) {
            // 先風控,然後才開始等候條件.
         }
         else { // 先等候條件, 條件成立後才風控.
            runner.OrderRaw().RequestSt_ = f9fmkt_TradingRequestSt_WaitingCond;
            OmsSymb::MdLocker mdLocker{*this->CondSymb_};
            if (this->CondSymb_->AddCondReq(mdLocker, kPriority_BfSc, *this, cxRaw, nextStep, runner.ExLogForUpd_)) {
               this->OnWaitCondBfSc(std::move(runner));
               return;
            }
            // 條件已成立,則直接進行下一步驟.
         }
      }
      nextStep.RunRequest(std::move(runner));
   }
   /// 風控後,已進入等候狀態.
   virtual void OnWaitCondAfSc(OmsRequestRunnerInCore&& runner) const = 0;
   void RunCondStepAfSc(OmsRequestRunnerInCore&& runner, OmsCxBaseOrdRaw& cxRaw, OmsRequestRunStep& nextStep) const {
      if (this->CondFn_) {
         if (runner.OrderRaw().RequestSt_ == f9fmkt_TradingRequestSt_WaitingCond) {
            // 風控前,已等候過條件,所以: 此時必定是[已觸發]狀態,不用再進入等候狀態;
         }
         else {
            // 風控後,尚開始未等候的條件單 = 此時開始等候條件.
            OmsSymb::MdLocker mdLocker{*this->CondSymb_};
            if (this->CondSymb_->AddCondReq(mdLocker, kPriority_AfSc, *this, cxRaw, nextStep, runner.ExLogForUpd_)) {
               this->OnWaitCondAfSc(std::move(runner));
               return;
            }
         }
      }
      nextStep.RunRequest(std::move(runner));
   }
};
//--------------------------------------------------------------------------//
template <class TxReq, class TOrdRaw>
class UtwCondRequestIni: public OmsCxCombineReqIni<TxReq, UtwOmsCxIni, TOrdRaw> {
   fon9_NON_COPY_NON_MOVE(UtwCondRequestIni);
   using base = OmsCxCombineReqIni<TxReq, UtwOmsCxIni, TOrdRaw>;
public:
   using base::base;
   UtwCondRequestIni() = default;
   fon9::StrView GetTxSymbId() const override {
      return ToStrView(this->Symbol_);
   }
   OmsOrderRaw* BeforeReqInCore(OmsRequestRunner& runner, OmsResource& res) override {
      return (this->BeforeReqInCore_CheckCond(runner, res, this->Policy())
              ? base::BeforeReqInCore(runner, res)
              : nullptr);
   }
   void OnWaitCondBfSc(OmsRequestRunnerInCore&& runner) const override {
      auto& ordraw = *static_cast<TOrdRaw*>(&runner.OrderRaw());
      ordraw.BeforeQty_ = ordraw.AfterQty_ = 0;
      ordraw.LeavesQty_ = this->Qty_;
      ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_NewWaitingCond;
      runner.Update(f9fmkt_TradingRequestSt_WaitingCond);
   }
   void OnWaitCondAfSc(OmsRequestRunnerInCore&& runner) const override {
      runner.OrderRaw().UpdateOrderSt_ = f9fmkt_OrderSt_NewWaitingCond;
      runner.Update(f9fmkt_TradingRequestSt_WaitingCond);
   }
};
//--------------------------------------------------------------------------//
template <class ReqIniT, class OrdRawT>
struct UtwCondStepBfSc : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(UtwCondStepBfSc);
   using base = OmsRequestRunStep;
   using base::base;
   void RunRequest(OmsRequestRunnerInCore&& runner) override {
      static_cast<const ReqIniT*>(&runner.OrderRaw().Request())
         ->RunCondStepBfSc(std::move(runner), *static_cast<OrdRawT*>(&runner.OrderRaw()), *this->NextStep_);
   }
};
template <class ReqIniT, class OrdRawT>
struct UtwCondStepAfSc : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(UtwCondStepAfSc);
   using base = OmsRequestRunStep;
   using base::base;
   void RunRequest(OmsRequestRunnerInCore&& runner) override {
      static_cast<const ReqIniT*>(&runner.OrderRaw().Request())
         ->RunCondStepAfSc(std::move(runner), *static_cast<OrdRawT*>(&runner.OrderRaw()), *this->NextStep_);
   }
};
// ======================================================================== //
template <class ReqChgT, class OrdRawT, class ReqIniT>
struct UtwCondStepChg : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(UtwCondStepChg);
   using base = OmsRequestRunStep;
   using base::base;
   void RunRequest(OmsRequestRunnerInCore&& runner) override {
      OrdRawT& ordraw = *static_cast<OrdRawT*>(&runner.OrderRaw());
      auto&    reqChg = *static_cast<const ReqChgT*>(&ordraw.Request());
      if (ordraw.Order().LastOrderSt() == f9fmkt_OrderSt_NewWaitingCond) {
         switch (reqChg.RxKind()) {
         case f9fmkt_RxKind_RequestChgQty:
            if (reqChg.Qty_ != 0) {
            __CHG_QTY:;
               if (reqChg.Qty_ > 0) {
                  ordraw.LeavesQty_ = fon9::unsigned_cast(reqChg.Qty_);
               __LeavesQty_Changed:;
                  if (ordraw.AfterQty_ > 0) {
                     ordraw.BeforeQty_ = ordraw.AfterQty_;
                     ordraw.AfterQty_ = ordraw.LeavesQty_;
                  }
                  runner.Update(f9fmkt_TradingRequestSt_Done);
                  return;
               }
               const auto decQty = fon9::unsigned_cast(-reqChg.Qty_);
               if (ordraw.LeavesQty_ > decQty) {
                  ordraw.LeavesQty_ = static_cast<decltype(ordraw.LeavesQty_)>(ordraw.LeavesQty_ - decQty);
                  goto __LeavesQty_Changed;
               }
            }
            /* fall through */ // 改量後剩餘數量為 0, 視為刪單;
         case f9fmkt_RxKind_RequestDelete:
            ordraw.LeavesQty_ = ordraw.AfterQty_ = 0;
            ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_NewQueuingCanceled;
            runner.Update(f9fmkt_TradingRequestSt_Done);
            // 刪單後, 應該要通知 CondSymb,
            // 但在觸發條件時會檢查條件單是否有效,
            // 所以在此就不必通知 CondSymb 了!
            return;
         case f9fmkt_RxKind_RequestChgPri:
            ordraw.LastPriType_ = reqChg.PriType_;
            ordraw.LastPri_     = reqChg.Pri_;
            runner.Update(f9fmkt_TradingRequestSt_Done);
            return;
         case f9fmkt_RxKind_RequestChgCond:
            assert(dynamic_cast<const ReqIniT*>(ordraw.Order().Initiator()) != nullptr);
            if (auto* reqIni = static_cast<const ReqIniT*>(ordraw.Order().Initiator())) {
               assert(reqIni->CondFn_.get() != nullptr);
               OmsErrCode res = reqIni->CondFn_->OnOmsCx_AssignReqChgToOrd(reqChg, ordraw);
               if (res != OmsErrCode_Null)
                  runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, res, nullptr);
               else {
                  if (reqChg.PriType_ != f9fmkt_PriType_Unknown || !reqChg.Pri_.IsNull()) {
                     // 同時改價;
                     ordraw.LastPriType_ = reqChg.PriType_;
                     ordraw.LastPri_ = reqChg.Pri_;
                  }
                  if (reqChg.Qty_ != 0) { // 同時改量;
                     goto __CHG_QTY;
                  }
                  runner.Update(f9fmkt_TradingRequestSt_Done);
               }
               return;
            }
            break;
         case f9fmkt_RxKind_RequestQuery:
            // 目前條件單不支援: 查詢; 自行開發者,可自定查詢功能;
            runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_RequestNotSupportThisOrder, nullptr);
            return;
         case f9fmkt_RxKind_Unknown:
         case f9fmkt_RxKind_RequestNew:
         case f9fmkt_RxKind_RequestRerun:
         case f9fmkt_RxKind_Filled:
         case f9fmkt_RxKind_Order:
         case f9fmkt_RxKind_Event:
         default:
            runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Bad_RxKind, nullptr);
            return;
         }
      }
      else if (reqChg.RxKind() == f9fmkt_RxKind_RequestChgCond) {
         runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Triggered_Cannot_ChgCond, nullptr);
         return;
      }
      this->ToNextStep(std::move(runner));
   }
};
// ======================================================================== //
using UtwsOrderRaw     = OmsCxCombineOrdRaw<OmsTwsOrderRaw, OmsCxBaseOrdRaw>;
using UtwsRequestIni   = UtwCondRequestIni<OmsTwsRequestIni, UtwsOrderRaw>;
using UtwsCondStepBfSc = UtwCondStepBfSc<UtwsRequestIni, UtwsOrderRaw>;
using UtwsCondStepAfSc = UtwCondStepAfSc<UtwsRequestIni, UtwsOrderRaw>;

using UtwsRequestChg  = OmsCxCombine<OmsTwsRequestChg, OmsCxBaseChgDat>;
using UtwsCondStepChg = UtwCondStepChg<UtwsRequestChg, UtwsOrderRaw, UtwsRequestIni>;
//--------------------------------------------------------------------------//
using UtwfOrderRaw1    = OmsCxCombineOrdRaw<OmsTwfOrderRaw1, OmsCxBaseOrdRaw>;
using UtwfRequestIni1  = UtwCondRequestIni<OmsTwfRequestIni1, UtwfOrderRaw1>;
using UtwfCondStepBfSc = UtwCondStepBfSc<UtwfRequestIni1, UtwfOrderRaw1>;
using UtwfCondStepAfSc = UtwCondStepAfSc<UtwfRequestIni1, UtwfOrderRaw1>;

using UtwfRequestChg1  = OmsCxCombine<OmsTwfRequestChg1, OmsCxBaseChgDat>;
using UtwfCondStepChg1 = UtwCondStepChg<UtwfRequestChg1, UtwfOrderRaw1, UtwfRequestIni1>;
// ======================================================================== //

} // namespaces
#endif//__f9utw_UtwRequests_hpp__
