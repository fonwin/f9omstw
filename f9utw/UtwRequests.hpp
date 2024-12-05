// \file f9utw/UtwRequests.hpp
// \author fonwinz@gmail.com
#ifndef __f9utw_UtwRequests_hpp__
#define __f9utw_UtwRequests_hpp__
#include "f9utw/UtwOmsCoreUsrDef.hpp"
#include "f9omstw/OmsCxReqBase.hpp"
#include "f9omstw/OmsCxCombine.hpp"
#include "f9omstws/OmsTwsRequest.hpp"
#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstwf/OmsTwfRequest1.hpp"
#include "f9omstwf/OmsTwfOrder1.hpp"

namespace f9omstw {

//--------------------------------------------------------------------------//
template <class TxReqT, class OrdRawT>
struct OmsCondStepBfSc : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(OmsCondStepBfSc);
   using base = OmsRequestRunStep;
   using base::base;
   void RunRequest(OmsRequestRunnerInCore&& runner) override {
      const auto& txReq = *static_cast<const TxReqT*>(&runner.OrderRaw().Request());
      txReq.RunCondStepBfSc_T(txReq, std::move(runner), *static_cast<OrdRawT*>(&runner.OrderRaw()), *this->NextStep_);
   }
};
template <class TxReqT, class OrdRawT>
struct OmsCondStepAfSc : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(OmsCondStepAfSc);
   using base = OmsRequestRunStep;
   using base::base;
   void RunRequest(OmsRequestRunnerInCore&& runner) override {
      const auto& txReq = *static_cast<const TxReqT*>(&runner.OrderRaw().Request());
      txReq.RunCondStepAfSc_T(txReq, std::move(runner), *static_cast<OrdRawT*>(&runner.OrderRaw()), *this->NextStep_);
   }
};
// ======================================================================== //
template <class ReqChgT, class OrdRawT, class ReqIniT>
struct OmsCondStepChg : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(OmsCondStepChg);
   using base = OmsRequestRunStep;
   using base::base;
   /// \retval false 刪單;
   /// \retval true  改量;
   bool ChgQty(const ReqChgT& reqChg, OrdRawT& ordraw) {
      assert(reqChg.Qty_ != 0);
      if (reqChg.Qty_ < 0) {
         const auto decQty = fon9::unsigned_cast(-reqChg.Qty_);
         if (ordraw.LeavesQty_ <= decQty)
            return false;
         ordraw.LeavesQty_ = static_cast<decltype(ordraw.LeavesQty_)>(ordraw.LeavesQty_ - decQty);
      }
      else {
         ordraw.LeavesQty_ = fon9::unsigned_cast(reqChg.Qty_);
      }
      if (ordraw.AfterQty_ > 0) {
         ordraw.BeforeQty_ = ordraw.AfterQty_;
         ordraw.AfterQty_ = ordraw.LeavesQty_;
      }
      return true;
   }
   void RunRequest(OmsRequestRunnerInCore&& runner) override {
      OrdRawT& ordraw = *static_cast<OrdRawT*>(&runner.OrderRaw());
      auto&    reqChg = *static_cast<const ReqChgT*>(&ordraw.Request());
      if (fon9_LIKELY(ordraw.Order().LastOrderSt() != f9fmkt_OrderSt_NewWaitingCond)) {
         if (fon9_LIKELY(reqChg.RxKind() != f9fmkt_RxKind_RequestChgCond))
            this->ToNextStep(std::move(runner));
         else {
            runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Triggered_Cannot_ChgCond, nullptr);
         }
         return;
      }
      switch (reqChg.RxKind()) {
      case f9fmkt_RxKind_RequestChgQty:
         if (reqChg.Qty_ != 0) {
            if (this->ChgQty(reqChg, ordraw))
               break;
         }
         /* fall through */ // 改量後剩餘數量為 0, 視為刪單;
      case f9fmkt_RxKind_RequestDelete:
      __DELETE_COND_REQUEST:;
         ordraw.LeavesQty_ = ordraw.AfterQty_ = 0;
         ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_NewQueuingCanceled;
         // 刪單後, 應該要通知 CondSymb,
         // 但在觸發條件時會檢查條件單是否有效,
         // 所以在此就不必通知 CondSymb 了!
         break;
      case f9fmkt_RxKind_RequestChgPri:
         ordraw.LastPriType_ = reqChg.PriType_;
         ordraw.LastPri_ = reqChg.Pri_;
         break;
      case f9fmkt_RxKind_RequestChgCond:
         assert(dynamic_cast<const ReqIniT*>(ordraw.Order().Initiator()) != nullptr);
         if (auto* reqIni = static_cast<const ReqIniT*>(ordraw.Order().Initiator())) {
            if (reqIni->IsAllowChgCond()) {
               OmsErrCode res = reqIni->OnOmsCx_AssignReqChgToOrd(reqChg, ordraw);
               if (res != OmsErrCode_Null) {
                  runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, res, nullptr);
                  return;
               }
               if (reqChg.PriType_ != f9fmkt_PriType_Unknown || !reqChg.Pri_.IsNull()) {
                  // 同時改價;
                  ordraw.LastPriType_ = reqChg.PriType_;
                  ordraw.LastPri_ = reqChg.Pri_;
               }
               if (reqChg.Qty_ != 0) { // 同時改量;
                  if (!this->ChgQty(reqChg, ordraw))
                     goto __DELETE_COND_REQUEST;
               }
               reqIni->OnAfterChgCond_Checker_T(*reqIni, std::move(runner), ordraw, static_cast<UtwOmsCoreUsrDef*>(nullptr), static_cast<UtwsSymb*>(nullptr));
               return;
            }
         }
         runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_RequestNotSupportThisOrder, nullptr);
         return;
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
      runner.Update(f9fmkt_TradingRequestSt_Done);
   }
};
// ======================================================================== //
using UtwsOrderRaw     = OmsCxCombineOrdRaw<OmsTwsOrderRaw, OmsCxBaseRaw>;
using UtwsRequestIni   = OmsCxCombineReqIni<OmsTwsRequestIni, UtwsOrderRaw, UtwOmsCoreUsrDef, UtwsSymb>;
using UtwsCondStepBfSc = OmsCondStepBfSc<UtwsRequestIni, UtwsOrderRaw>;
using UtwsCondStepAfSc = OmsCondStepAfSc<UtwsRequestIni, UtwsOrderRaw>;

using UtwsRequestChg   = OmsCxCombine<OmsTwsRequestChg, OmsCxBaseChgDat>;
using UtwsCondStepChg  = OmsCondStepChg<UtwsRequestChg, UtwsOrderRaw, UtwsRequestIni>;
//--------------------------------------------------------------------------//
using UtwfOrderRaw1    = OmsCxCombineOrdRaw<OmsTwfOrderRaw1, OmsCxBaseRaw>;
using UtwfRequestIni1  = OmsCxCombineReqIni<OmsTwfRequestIni1, UtwfOrderRaw1, UtwOmsCoreUsrDef, UtwsSymb>;
using UtwfCondStepBfSc = OmsCondStepBfSc<UtwfRequestIni1, UtwfOrderRaw1>;
using UtwfCondStepAfSc = OmsCondStepAfSc<UtwfRequestIni1, UtwfOrderRaw1>;

using UtwfRequestChg1  = OmsCxCombine<OmsTwfRequestChg1, OmsCxBaseChgDat>;
using UtwfCondStepChg1 = OmsCondStepChg<UtwfRequestChg1, UtwfOrderRaw1, UtwfRequestIni1>;
// ======================================================================== //

} // namespaces
#endif//__f9utw_UtwRequests_hpp__
