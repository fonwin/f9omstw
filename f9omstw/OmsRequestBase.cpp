// \file f9omstw/OmsRequestBase.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestBase.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

OmsRequestBase::~OmsRequestBase() {
   if (this->RxItemFlags_ & OmsRequestFlag_Abandon)
      delete this->AbandonReason_;
   else if (this->RxItemFlags_ & OmsRequestFlag_Initiator) {
      assert(this->LastUpdated() != nullptr);
      this->LastUpdated()->Order_->FreeThis();
   }
}
const OmsRequestBase* OmsRequestBase::CastToRequest() const {
   return this;
}
const OmsRequestIni* OmsRequestBase::GetOrderInitiatorByOrdKey(OmsResource& res) const {
   if (const OmsBrk* brk = res.Brks_->GetBrkRec(ToStrView(this->BrkId_))) {
      if (const OmsOrdNoMap* ordNoMap = brk->GetOrdNoMap(*this)) {
         if (OmsOrder* ord = ordNoMap->GetOrder(this->OrdNo_)) {
            assert(ord->Initiator_ != nullptr && ord->Initiator_->LastUpdated() != nullptr);
            return ord->Initiator_;
         }
      }
   }
   return nullptr;
}
const OmsRequestBase* OmsRequestBase::GetRequestInitiator(OmsRxSNO* pIniSNO, OmsResource& res) {
   const OmsRequestBase* inireq;
   if (pIniSNO == nullptr || *pIniSNO == 0) {
      if ((inireq = this->GetOrderInitiatorByOrdKey(res)) == nullptr)
         return nullptr;
      if (inireq->LastUpdated() == nullptr)
         return nullptr;
   }
   else {
      if ((inireq = res.GetRequest(*pIniSNO)) == nullptr)
         return nullptr;
      const OmsOrderRaw* lastUpdated = inireq->LastUpdated();
      if (lastUpdated == nullptr)
         return nullptr;

      if (this->BrkId_.empty())
         this->BrkId_ = inireq->BrkId_;
      else if (this->BrkId_ != inireq->BrkId_)
         return nullptr;

      lastUpdated = lastUpdated->Order_->Last();
      if (this->OrdNo_.empty1st())
         this->OrdNo_ = lastUpdated->OrdNo_;
      else if (this->OrdNo_ != lastUpdated->OrdNo_)
         return nullptr;

      if (this->Market_ == f9fmkt_TradingMarket_Unknown)
         this->Market_ = inireq->Market_;
      else if (this->Market_ != inireq->Market_)
         return nullptr;

      if (this->SessionId_ == f9fmkt_TradingSessionId_Unknown)
         this->SessionId_ = inireq->SessionId_;
      else if (this->SessionId_ != inireq->SessionId_)
         return nullptr;
   }
   if (pIniSNO && *pIniSNO == 0)
      *pIniSNO = inireq->RxSNO();
   return inireq;
}
const OmsRequestBase* OmsRequestBase::PreCheck_GetRequestInitiator(OmsRequestRunner& runner, OmsRxSNO* pIniSNO, OmsResource& res) {
   assert(this->LastUpdated_ == nullptr && this->AbandonReason_ == nullptr);
   if (const OmsRequestBase* inireq = this->GetRequestInitiator(pIniSNO, res))
      return inireq;
   runner.RequestAbandon(&res, OmsErrCode_OrderNotFound);
   return nullptr;
}

void OmsRequestBase::MakeFieldsImpl(fon9::seed::Fields& flds) {
   flds.Add(fon9_MakeField(OmsRequestBase,  RxKind_, "Kind"));
   flds.Add(fon9_MakeField2(OmsRequestBase, Market));
   flds.Add(fon9_MakeField2(OmsRequestBase, SessionId));
   flds.Add(fon9_MakeField2(OmsRequestBase, BrkId));
   flds.Add(fon9_MakeField2(OmsRequestBase, OrdNo));
   flds.Add(fon9_MakeField2(OmsRequestBase, ReqUID));
   flds.Add(fon9_MakeField2(OmsRequestBase, CrTime));
}
fon9::seed::FieldSP OmsRequestBase::MakeField_RxSNO() {
   return fon9_MakeField2_const(OmsRequestBase, RxSNO);
}

} // namespaces
