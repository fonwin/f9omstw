// \file f9omstwf/OmsTwfRequest9.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfRequest9.hpp"
#include "f9omstwf/OmsTwfOrder9.hpp"
#include "f9omstw/OmsTradingLineMgr.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsTwfRequestIni9::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields(flds);
   flds.Add(fon9_MakeField2(OmsTwfRequestIni9, BidQty));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni9, BidPri));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni9, OfferQty));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni9, OfferPri));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni9, TimeInForce));
   flds.Add(fon9_MakeField2(OmsTwfRequestIni9, IvacNoFlag));
}
//--------------------------------------------------------------------------//
void OmsTwfRequestChg9::MakeFields(fon9::seed::Fields& flds) {
   base::MakeFields<OmsTwfRequestChg9>(flds);
   flds.Add(fon9_MakeField2(OmsTwfRequestChg9, BidQty));
   flds.Add(fon9_MakeField2(OmsTwfRequestChg9, BidPri));
   flds.Add(fon9_MakeField2(OmsTwfRequestChg9, OfferQty));
   flds.Add(fon9_MakeField2(OmsTwfRequestChg9, OfferPri));
}
bool OmsTwfRequestChg9::ValidateInUser(OmsRequestRunner& reqRunner) {
   if (fon9_LIKELY(this->RxKind_ == f9fmkt_RxKind_Unknown)) {
      if (this->BidQty_ == 0 && this->OfferQty_ == 0) {
         if (this->BidPri_.IsNullOrZero() && this->OfferPri_.IsNullOrZero())
            this->RxKind_ = f9fmkt_RxKind_RequestDelete;
         else
            this->RxKind_ = f9fmkt_RxKind_RequestChgPri;
      }
      else
         this->RxKind_ = f9fmkt_RxKind_RequestChgQty;
   }
   return base::ValidateInUser(reqRunner);
}
OmsOrderRaw* OmsTwfRequestChg9::BeforeReqInCore(OmsRequestRunner& runner, OmsResource& res) {
   return this->BeforeReqInCoreT<OmsTwfOrder9>(runner, res);
}
OmsTwfRequestChg9::OpQueuingRequestResult OmsTwfRequestChg9::OpQueuingRequest(fon9::fmkt::TradingLineManager& from,
                                                                              TradingRequest& queuingRequest) {
   if (queuingRequest.RxKind() != f9fmkt_RxKind_RequestNew || this->IniSNO() != queuingRequest.RxSNO())
      return Op_NotTarget;
   switch (this->RxKind()) {
   default:
   case f9fmkt_RxKind_Unknown:
   case f9fmkt_RxKind_RequestNew:
   case f9fmkt_RxKind_Filled:
   case f9fmkt_RxKind_Order:
   case f9fmkt_RxKind_Event:
      return Op_NotSupported;

   case f9fmkt_RxKind_RequestDelete:
   case f9fmkt_RxKind_RequestChgQty:
   case f9fmkt_RxKind_RequestChgPri:
   case f9fmkt_RxKind_RequestQuery:
      break;
   }
   auto* lmgr = dynamic_cast<OmsTradingLineMgrBase*>(&from);
   if (!lmgr)
      return Op_NotSupported;
   OmsRequestRunnerInCore* currRunner = lmgr->CurrRunner();
   if (!currRunner)
      return Op_NotSupported;

   // 是否要針對 iniReq 建立一個 ordraw 呢?
   // auto* iniReq = dynamic_cast<OmsTwfRequestIni9*>(&queuingRequest);
   // if (iniReq == nullptr)
   //    return Op_NotTarget;
   // OmsRequestRunnerInCore iniRunner{currRunner->Resource_, *iniReq->LastUpdated()->Order().BeginUpdate(*iniReq)};
   // ...
   // iniRunner.Update(f9fmkt_TradingRequestSt_QueuingCanceled);
   // -----
   auto& ordraw = currRunner->OrderRawT<OmsTwfOrderRaw9>();
   if (this->RxKind() == f9fmkt_RxKind_RequestQuery) {
      // 查詢, 不改變任何狀態: 讓回報機制直接回覆最後的 OrderRaw 即可.
      currRunner->Update(f9fmkt_TradingRequestSt_Done);
      return Op_ThisDone;
   }
   struct AssignAux {
      static inline void Pri(OmsTwfPri& dst, OmsTwfPri src) {
         if (!src.IsNull())
            dst = src;
      }
   };
   if (this->RxKind() == f9fmkt_RxKind_RequestChgPri) {
      AssignAux::Pri(ordraw.LastBidPri_, this->BidPri_);
      AssignAux::Pri(ordraw.LastOfferPri_, this->OfferPri_);
      currRunner->Update(f9fmkt_TradingRequestSt_Done);
      return Op_ThisDone;
   }
   assert(this->RxKind() == f9fmkt_RxKind_RequestChgQty || this->RxKind() == f9fmkt_RxKind_RequestDelete);
   OmsTradingLineMgrBase::CalcInternalChgQty(ordraw.Bid_, this->BidQty_);
   OmsTradingLineMgrBase::CalcInternalChgQty(ordraw.Offer_, this->OfferQty_);
   if (ordraw.Bid_.AfterQty_ == 0 && ordraw.Offer_.AfterQty_ == 0) {
      ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_NewQueuingCanceled;
      currRunner->Update(f9fmkt_TradingRequestSt_QueuingCanceled);
      return Op_AllDone;
   }
   currRunner->Update(f9fmkt_TradingRequestSt_Done);
   return Op_ThisDone;
}

} // namespaces
