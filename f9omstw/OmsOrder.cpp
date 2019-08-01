// \file f9omstw/OmsOrder.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsOrder.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/RawRd.hpp"

namespace f9omstw {

fon9_MSC_WARN_DISABLE(4355); /* 'this': used in base member initializer list */
OmsOrder::OmsOrder(OmsRequestIni& initiator, OmsOrderFactory& creator, OmsScResource&& scRes)
   : ScResource_(std::move(scRes))
   , Initiator_{&initiator}
   , Creator_(&creator)
   , Head_{creator.MakeOrderRaw(*this, initiator)} {
   this->LastNotPending_ = this->Tail_ = const_cast<OmsOrderRaw*>(this->Head_);

   assert(initiator.LastUpdated() == nullptr && !initiator.IsAbandoned());
   initiator.SetInitiatorFlag();

   if (0);// 應移除: LastNotPending_, Initiator_?
   // - Initiator() = if (auto* ini = this->Head_) return ini->IsInitiator();
   //   如何決定 rpt 是否為新的 Initiator 呢?
   //    - (BeforeQty==0 && AfterQty!=0)?
   // - LastNotPending() => 直接使用 Tail() 即可?
   //   即使是 Pending(OrderSt_ReportPending), OrderRaw 也是從上次取得?
   //   => 但是 OrderSt_ 怎麼辦?
   // - 移除 OrderRaw.ExgLastTime? LastPriTime? 改成 SNO?
   //    => 不行 => 因為本機送出的 req 收到的回報不會留下記錄, 如果不把時間記錄在 OrderRaw, 那就會遺失了!
   // - 移除 OrderRaw.LastFilledTime
   //    => 因為 FilledLast_(而且即使亂序也會是 MatchKey 最大的那筆) 就有時間了?
}
fon9_MSC_WARN_POP;

OmsOrder::OmsOrder() {
}
void OmsOrder::Initialize(OmsRequestIni& initiator, OmsOrderFactory& creator, OmsScResource* scRes) {
   assert(this->Initiator_ == nullptr && this->Head_ == nullptr);
   this->Creator_ = &creator;
   this->Initiator_ = &initiator;
   this->Head_ = creator.MakeOrderRaw(*this, initiator);
   this->LastNotPending_ = this->Tail_ = const_cast<OmsOrderRaw*>(this->Head_);
   if (scRes)
      this->ScResource_ = std::move(*scRes);
   assert(initiator.LastUpdated() == nullptr && !initiator.IsAbandoned());
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
void OmsOrder::ProcessPendingReport(OmsResource& res) {
   assert(this->LastNotPending_ == this->Tail_);
   assert(this->FirstPending_ != nullptr);
   const OmsOrderRaw* pord = this->FirstPending_;
   const OmsOrderRaw* afFirstPending = nullptr;
   this->FirstPending_ = nullptr;
   for (;;) {
      const OmsOrderRaw* bfNotPending = this->LastNotPending_;
      while (pord) {
         if (pord->OrderSt_ == f9fmkt_OrderSt_ReportPending) {
            if (pord->Request_->LastUpdated() == pord) {
               pord->Request_->ProcessPendingReport(res);
               if (afFirstPending == nullptr && pord->Request_->LastUpdated() == pord)
                  afFirstPending = pord;
            }
         }
         pord = pord->Next();
      }
      if (bfNotPending == this->LastNotPending_)
         break;
      pord = afFirstPending;
   }
   this->FirstPending_ = afFirstPending;
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
   this->Market_ = order.Initiator_->Market();
   this->SessionId_ = order.Initiator_->SessionId();
}
OmsOrderRaw::OmsOrderRaw(const OmsOrderRaw* tail, const OmsRequestBase& req)
   : Order_(tail->Order_)
   , Request_(&req)
   , UpdSeq_{tail->UpdSeq_ + 1}
   , OrdNo_{""} {
   assert(tail->Next_ == nullptr && tail->Order_->Tail_ == tail);
   tail->Next_ = tail->Order_->Tail_ = this;
   this->RxKind_ = f9fmkt_RxKind_Order;
   this->Market_ = req.Market();
   this->SessionId_ = req.SessionId();
}
void OmsOrderRaw::Initialize(const OmsOrderRaw* tail, const OmsRequestBase& req) {
   assert(tail != nullptr && tail->Next_ == nullptr && tail->Order_->Tail_ == tail);
   tail->Next_ = tail->Order_->Tail_ = this;
   this->Market_ = req.Market();
   this->SessionId_ = req.SessionId();

   *const_cast<OmsOrder**>            (&this->Order_)   = tail->Order_;
   *const_cast<const OmsRequestBase**>(&this->Request_) = &req;
   *const_cast<uint32_t*>             (&this->UpdSeq_)  = tail->UpdSeq_ + 1;
   this->ContinuePrevUpdate();
}
void OmsOrderRaw::ContinuePrevUpdate() {
   this->OrderSt_ = this->Order_->OrderSt();
   this->OrdNo_   = this->Order_->LastNotPending()->OrdNo_;
   this->ErrCode_ = OmsErrCode_NoError;
}
void OmsOrderRaw::OnOrderReject() {
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
