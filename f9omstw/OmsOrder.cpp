// \file f9omstw/OmsOrder.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsOrder.hpp"
#include "f9omstw/OmsIvSymb.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/RawRd.hpp"

namespace f9omstw {

OmsOrderFactory::~OmsOrderFactory() {
}
OmsOrderRaw* OmsOrderFactory::MakeOrderRaw(OmsOrder& order, const OmsRequestBase& req) {
   OmsOrderRaw* raw = this->MakeOrderRawImpl();
   if (auto last = order.Last())
      raw->Initialize(last, req);
   else
      raw->Initialize(order);
   return raw;
}

//--------------------------------------------------------------------------//

fon9_MSC_WARN_DISABLE(4355); /* 'this': used in base member initializer list */
OmsOrder::OmsOrder(OmsRequestIni& initiator, OmsOrderFactory& creator, OmsScResource&& scRes)
   : ScResource_(std::move(scRes))
   , Initiator_{&initiator}
   , Creator_(&creator)
   , Head_{creator.MakeOrderRaw(*this, initiator)} {
   this->Last_ = const_cast<OmsOrderRaw*>(this->Head_);

   assert(initiator.LastUpdated() == nullptr && initiator.AbandonReason() == nullptr);
   initiator.SetInitiatorFlag();
}
fon9_MSC_WARN_POP;

OmsOrder::OmsOrder() : Initiator_{nullptr}, Creator_{nullptr}, Head_{nullptr} {
}
void OmsOrder::Initialize(OmsRequestIni& initiator, OmsOrderFactory& creator, OmsScResource* scRes) {
   *const_cast<const OmsOrderFactory**>(&this->Creator_) = &creator;
   *const_cast<const OmsRequestIni**>(&this->Initiator_) = &initiator;
   *const_cast<const OmsOrderRaw**>(&this->Head_) = creator.MakeOrderRaw(*this, initiator);
   this->Last_ = const_cast<OmsOrderRaw*>(this->Head_);
   if (scRes)
      this->ScResource_ = std::move(*scRes);
   assert(initiator.LastUpdated() == nullptr && initiator.AbandonReason() == nullptr);
   initiator.SetInitiatorFlag();
}

OmsOrder::~OmsOrder() {
}
void OmsOrder::FreeThis() {
   delete this;
}
OmsBrk* OmsOrder::GetBrk(OmsResource& res) const {
   if (auto* ivr = this->ScResource_.Ivr_.get())
      return f9omstw::GetBrk(ivr);
   return res.Brks_->GetBrkRec(ToStrView(this->Initiator_->BrkId_));
}

//--------------------------------------------------------------------------//

OmsOrderRaw::~OmsOrderRaw() {
}
void OmsOrderRaw::FreeThis() {
   delete this;
}
const OmsOrderRaw* OmsOrderRaw::CastToOrder() const {
   return this;
}

void OmsOrderRaw::Initialize(OmsOrder& order) {
   *const_cast<OmsOrder**>(&this->Order_) = &order;
   *const_cast<const OmsRequestBase**>(&this->Request_) = order.Initiator_;
}
OmsOrderRaw::OmsOrderRaw(const OmsOrderRaw* prev, const OmsRequestBase& req)
   : Order_(prev->Order_)
   , Request_(&req)
   , Prev_{prev}
   , UpdSeq_{prev->UpdSeq_ + 1}
   , OrdNo_{""} {
   assert(prev->Next_ == nullptr && prev->Order_->Last_ == prev);
   prev->Next_ = prev->Order_->Last_ = this;
}
void OmsOrderRaw::Initialize(const OmsOrderRaw* prev, const OmsRequestBase& req) {
   assert(prev != nullptr && prev->Next_ == nullptr && prev->Order_->Last_ == prev);
   prev->Next_ = prev->Order_->Last_ = this;

   *const_cast<OmsOrder**>            (&this->Order_)   = prev->Order_;
   *const_cast<const OmsRequestBase**>(&this->Request_) = &req;
   *const_cast<const OmsOrderRaw**>   (&this->Prev_)    = prev;
   *const_cast<uint32_t*>             (&this->UpdSeq_)  = prev->UpdSeq_ + 1;
   this->ContinuePrevUpdate();
}
void OmsOrderRaw::ContinuePrevUpdate() {
   assert(this->Prev_ != nullptr);
   this->OrderSt_ = this->Prev_->OrderSt_;
   this->ErrCode_ = OmsErrCode_NoError;
   this->OrdNo_   = this->Prev_->OrdNo_;
}

void OmsOrderRaw::MakeFieldsImpl(fon9::seed::Fields& flds) {
   using namespace fon9;
   using namespace fon9::seed;
   flds.Add(FieldSP{new FieldIntHx<underlying_type_t<f9fmkt_OrderSt>>         (Named{"OrdSt"}, fon9_OffsetOfRawPointer(OmsOrderRaw, OrderSt_))});
   flds.Add(FieldSP{new FieldIntHx<underlying_type_t<f9fmkt_TradingRequestSt>>(Named{"ReqSt"}, fon9_OffsetOfRawPointer(OmsOrderRaw, RequestSt_))});
   flds.Add(fon9_MakeField2(OmsOrderRaw, ErrCode));
   flds.Add(fon9_MakeField2(OmsOrderRaw, OrdNo));
   flds.Add(fon9_MakeField2(OmsOrderRaw, Message));
   flds.Add(fon9_MakeField2(OmsOrderRaw, UpdateTime));
}

} // namespaces
