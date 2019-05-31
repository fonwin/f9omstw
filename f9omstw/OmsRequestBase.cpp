// \file f9omstw/OmsRequestBase.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestBase.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

OmsRequestBase::~OmsRequestBase() {
   if (this->RequestFlags_ & OmsRequestFlag_Abandon)
      delete this->AbandonReason_;
   else if (this->RequestFlags_ & OmsRequestFlag_Initiator) {
      assert(this->LastUpdated() != nullptr);
      this->LastUpdated()->Order_->FreeThis();
   }
}
const OmsRequestBase* OmsRequestBase::CastToRequest() const {
   return this;
}
void OmsRequestBase::OnRxItem_AddRef() const {
   intrusive_ptr_add_ref(this);
}
void OmsRequestBase::OnRxItem_Release() const {
   intrusive_ptr_release(this);
}
const OmsRequestBase* OmsRequestBase::PreCheckIniRequest(OmsRxSNO* pIniSNO, OmsResource& res) {
   assert(this->LastUpdated_ == nullptr && this->AbandonReason_ == nullptr);
   const OmsRequestBase* inireq;
   if (pIniSNO && *pIniSNO != 0) {
      if ((inireq = res.GetRequest(*pIniSNO)) == nullptr)
         goto __ABANDON_ORDER_NOT_FOUND;
      if (this->BrkId_.empty())
         this->BrkId_ = inireq->BrkId_;
      else if (this->BrkId_ != inireq->BrkId_)
         goto __ABANDON_ORDER_NOT_FOUND;

      if (*this->OrdNo_.begin() == 0)
         this->OrdNo_ = inireq->OrdNo_;
      else if (this->OrdNo_ != inireq->OrdNo_)
         goto __ABANDON_ORDER_NOT_FOUND;

      if (this->Market_ == f9fmkt_TradingMarket_Unknown)
         this->Market_ = inireq->Market_;
      else if (this->Market_ != inireq->Market_)
         goto __ABANDON_ORDER_NOT_FOUND;

      if (this->SessionId_ == f9fmkt_TradingSessionId_Unknown)
         this->SessionId_ = inireq->SessionId_;
      else if (this->SessionId_ != inireq->SessionId_)
         goto __ABANDON_ORDER_NOT_FOUND;
   }
   else {
      inireq = nullptr;
      if (const OmsBrk* brk = res.Brks_->GetBrkRec(ToStrView(this->BrkId_))) {
         if (const OmsOrdNoMap* ordNoMap = brk->GetOrdNoMap(*this)) {
            if (OmsOrder* ord = ordNoMap->GetOrder(this->OrdNo_))
               inireq = ord->Initiator_;
         }
      }
   }
   if (inireq && inireq->LastUpdated() != nullptr) {
      if (pIniSNO && *pIniSNO == 0)
         *pIniSNO = inireq->RxSNO();
      return inireq;
   }

__ABANDON_ORDER_NOT_FOUND:
   this->Abandon("Order not found.");
   return nullptr;
}

void OmsRequestBase::MakeFieldsImpl(fon9::seed::Fields& flds) {
   flds.Add(fon9_MakeField(OmsRequestBase, RequestKind_, "Kind"));
   flds.Add(fon9_MakeField2(OmsRequestBase, Market));
   flds.Add(fon9_MakeField2(OmsRequestBase, SessionId));
   flds.Add(fon9_MakeField2(OmsRequestBase, BrkId));
   flds.Add(fon9_MakeField2(OmsRequestBase, OrdNo));
   flds.Add(fon9_MakeField2(OmsRequestBase, ReqUID));
   flds.Add(fon9_MakeField2(OmsRequestBase, CrTime));
}

} // namespaces
