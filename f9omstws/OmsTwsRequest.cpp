﻿// \file f9omstws/OmsTwsRequest.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsRequest.hpp"
#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstw/OmsTradingLineMgr.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwsRequestIni::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwsRequestIni>(flds);
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, Side));
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, Symbol));
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, Qty));
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, Pri));
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, PriType));
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, IvacNoFlag));
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, OType));
   flds.Add(fon9_MakeField2(OmsTwsRequestIni, TimeInForce));
}
const char* OmsTwsRequestIni::IsIniFieldEqual(const OmsRequestBase& req) const {
   if (auto r = dynamic_cast<const OmsTwsRequestIni*>(&req)) {
      #define CHECK_IniFieldEqual(fldName)   do { if(this->fldName##_ != r->fldName##_) return #fldName; } while(0)
      CHECK_IniFieldEqual(Side);
      CHECK_IniFieldEqual(Symbol);
      if (r->OType_ != OmsTwsOType{})
         CHECK_IniFieldEqual(OType);
      return base::IsIniFieldEqualImpl(*r);
   }
   return "RequestTwsIni";
}
const OmsRequestIni* OmsTwsRequestIni::BeforeReq_CheckOrdKey(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) {
   if (this->Market_ == f9fmkt_TradingMarket_Unknown || this->SessionId_ == f9fmkt_TradingSessionId_Unknown) {
      if (!scRes.Symb_)
         scRes.Symb_ = res.Symbs_->GetOmsSymb(ToStrView(this->Symbol_));
      if (this->SessionId_ == f9fmkt_TradingSessionId_Unknown) {
         if (this->Pri_.IsNull() && (this->PriType_ == f9fmkt_PriType_Limit || this->PriType_ == f9fmkt_PriType_Unknown))
            this->SessionId_ = f9fmkt_TradingSessionId_FixedPrice;
         else {
            auto shUnit = fon9::fmkt::GetSymbTwsShUnit(scRes.Symb_.get());
            if (this->Qty_ < shUnit)
               this->SessionId_ = f9fmkt_TradingSessionId_OddLot;
            else
               this->SessionId_ = f9fmkt_TradingSessionId_Normal;
         }
      }
   }
   return base::BeforeReq_CheckOrdKey(runner, res, scRes);
}
//--------------------------------------------------------------------------//
void OmsTwsRequestChg::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwsRequestChg>(flds);
   flds.Add(fon9_MakeField2(OmsTwsRequestChg, Qty));
   flds.Add(fon9_MakeField2(OmsTwsRequestChg, PriType));
   flds.Add(fon9_MakeField2(OmsTwsRequestChg, Pri));
}
bool OmsTwsRequestChg::ValidateInUser(OmsRequestRunner& reqRunner) {
   if (fon9_LIKELY(this->RequestUpd_AutoRxKind(*this, reqRunner)))
      return base::ValidateInUser(reqRunner);
   return false;
}

OmsTwsRequestChg::OpQueuingRequestResult OmsTwsRequestChg::OpQueuingRequest(fon9::fmkt::TradingLineManager& from,
                                                                            TradingRequest& queuingRequest) {
   if (auto* lmgr = dynamic_cast<OmsTradingLineMgrBase*>(&from))
      return lmgr->OpQueuingRequest<OmsTwsOrderRaw, OmsTwsRequestIni>(*this, queuingRequest);
   return Op_NotSupported;
}
//--------------------------------------------------------------------------//
OmsOrderRaw* OmsTwsRequestForce::BeforeReqInCore(OmsRequestRunner& runner, OmsResource& res) {
   if (auto* order = this->BeforeReqInCore_GetOrder(runner, res)) {
      auto pol = static_cast<OmsRequestTrade*>(runner.Request_.get())->Policy();
      // 檢核是否有對應強迫權限
      if (fon9_UNLIKELY(!IsEnumContains(pol->ScForceFlags(), this->ScForceFlag_))) {
         runner.RequestAbandon(&res, OmsErrCode_DenyScForce_NoPermission, fon9::RevPrintTo<std::string>("UserScForceFlag=", pol->ScForceFlags()));
         return nullptr;
      }
      // 只有下單要求狀態在 f9fmkt_OrderSt_NewCheckingRejected 可以強迫
      if (fon9_UNLIKELY(order->LastOrderSt() != f9fmkt_OrderSt_NewCheckingRejected)) {
         runner.RequestAbandon(&res, OmsErrCode_DenyScForce_Bad_OrdSt);
         return nullptr;
      }
      // 重設 iniRequest 的 Policy, 後續建立委託書號時, 才能依照強迫者權限建立
      auto* iniReq = static_cast<const OmsRequestTrade*>(order->Initiator());
      this->BeforeReq_ResetIniPolicy(iniReq, pol);

      return order->BeginUpdate(*this);
   }
   return nullptr;
}
void OmsTwsRequestForce::BeforeReq_ResetIniPolicy(const OmsRequestTrade* iniReq, const OmsRequestPolicy* reqPol) {
   assert(reqPol != nullptr && iniReq != nullptr);
   const_cast<OmsRequestTrade*>(iniReq)->ForceResetPolicy(std::move(reqPol));
}
void OmsTwsRequestForce::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwsRequestForce>(flds);
   flds.Add(fon9_MakeField2(OmsTwsRequestForce, ScForceFlag));
}
} // namespaces
