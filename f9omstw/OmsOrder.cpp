// \file f9omstw/OmsOrder.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsOrder.hpp"
#include "f9omstw/OmsIvSymb.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

OmsOrderFactory::~OmsOrderFactory() {
}
OmsOrderRaw* OmsOrderFactory::MakeOrderRaw(OmsOrder& owner, const OmsTradingRequest& req) {
   OmsOrderRaw* raw = this->MakeOrderRawImpl();
   if (auto last = owner.Last())
      raw->Initialize(last, req);
   else
      raw->Initialize(owner);
   return raw;
}

//--------------------------------------------------------------------------//

fon9_MSC_WARN_DISABLE(4355); /* 'this': used in base member initializer list */
OmsOrder::OmsOrder(OmsRequestNew& initiator, OmsOrderFactory& creator, OmsScResource&& scRes)
   : Initiator_{&initiator}
   , Creator_(&creator)
   , ScResource_(std::move(scRes))
   , Head_{creator.MakeOrderRaw(*this, initiator)} {
   this->Last_ = const_cast<OmsOrderRaw*>(this->Head_);

   assert(initiator.LastUpdated() == nullptr && initiator.AbandonReason() == nullptr);
   initiator.SetInitiatorFlag();
}
fon9_MSC_WARN_POP;

OmsOrder::OmsOrder() : Initiator_{nullptr}, Creator_{nullptr}, Head_{nullptr} {
}

void OmsOrder::Initialize(OmsRequestNew& initiator, OmsOrderFactory& creator, OmsScResource&& scRes) {
   *const_cast<const OmsOrderFactory**>(&this->Creator_) = &creator;
   *const_cast<const OmsRequestNew**>(&this->Initiator_) = &initiator;
   *const_cast<OmsScResource*>(&this->ScResource_) = std::move(scRes);
   *const_cast<const OmsOrderRaw**>(&this->Head_) = creator.MakeOrderRaw(*this, initiator);
   this->Last_ = const_cast<OmsOrderRaw*>(this->Head_);
   assert(initiator.LastUpdated() == nullptr && initiator.AbandonReason() == nullptr);
   initiator.SetInitiatorFlag();
}

OmsOrder::~OmsOrder() {
   const OmsOrderRaw* curr = this->Last_;
   while (curr) {
      const OmsOrderRaw* next = curr;
      curr = curr->Prev_;
      const_cast<OmsOrderRaw*>(next)->FreeThis();
   }
}
void OmsOrder::FreeThis() {
   delete this;
}

//--------------------------------------------------------------------------//

OmsOrderRaw::~OmsOrderRaw() {
}
void OmsOrderRaw::FreeThis() {
   delete this;
}
void OmsOrderRaw::Initialize(OmsOrder& owner) {
   *const_cast<OmsOrder**>(&this->Owner_) = &owner;
   *const_cast<const OmsRequestBase**>(&this->Request_) = owner.Initiator_;
}
void OmsOrderRaw::Initialize(const OmsOrderRaw* prev, const OmsRequestBase& req) {
   assert(prev != nullptr && prev->Next_ == nullptr);
   *const_cast<OmsOrder**>(&this->Owner_) = prev->Owner_;
   *const_cast<const OmsRequestBase**>(&this->Request_) = &req;
   *const_cast<const OmsOrderRaw**>(&this->Prev_) = prev;
   *const_cast<uint32_t*>(&this->UpdSeq_) = prev->UpdSeq_ + 1;
   prev->Next_ = this;
   this->ContinuePrevUpdate();
}
void OmsOrderRaw::ContinuePrevUpdate() {
   assert(this->Prev_ != nullptr);
   this->OrderSt_ = this->Prev_->OrderSt_;
   this->UpdateTime_ = fon9::UtcNow();
}

void OmsOrderRaw::MakeFields(fon9::seed::Fields& flds) {
   using namespace fon9;
   using namespace fon9::seed;
   flds.Add(FieldSP{new FieldIntHx<underlying_type_t<f9fmkt_OrderSt>>         (Named{"OrdSt"}, fon9_OffsetOfRawPointer(OmsOrderRaw, OrderSt_))});
   flds.Add(FieldSP{new FieldIntHx<underlying_type_t<f9fmkt_TradingRequestSt>>(Named{"ReqSt"}, fon9_OffsetOfRawPointer(OmsOrderRaw, RequestSt_))});
   flds.Add(fon9_MakeField(Named{"UpdateTime"}, OmsOrderRaw, UpdateTime_));
}

} // namespaces
