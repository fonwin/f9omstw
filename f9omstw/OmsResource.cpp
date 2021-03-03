// \file f9omstw/OmsResource.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsResource.hpp"
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {

OmsResource::UsrDefObj::~UsrDefObj() {
}

OmsResource::~OmsResource() {
}
void OmsResource::Plant() {
   this->Sapling_->AddNamedSapling(this->Symbs_, fon9::Named{"Symbs"});
   this->Sapling_->AddNamedSapling(this->Brks_, fon9::Named{"Brks"});
}

static void FrozeScLeaves(OmsBackend& backend, const OmsRxSNO lastSNO, const OmsBackend::Locker& locker) {
   OmsRxSNO sno = (lastSNO ? lastSNO : backend.LastSNO());
   while (sno > 0) {
      if (const OmsRxItem* item = backend.GetItem(sno, locker))
         if (auto* ordraw = static_cast<const OmsOrderRaw*>(item->CastToOrder())) {
            ordraw->IsFrozeScLeaves_ = true;
            if (lastSNO && (ordraw = ordraw->Order().Tail()) != nullptr && ordraw->RxSNO() >= lastSNO)
               ordraw->IsFrozeScLeaves_ = true;
         }
      --sno;
   }
}
void FrozeScLeaves(OmsBackend& backend, OmsRxSNO lastSNO, const OmsBackend::Locker* locker) {
   if (locker)
      FrozeScLeaves(backend, lastSNO, *locker);
   else
      FrozeScLeaves(backend, lastSNO, backend.Lock());
}

} // namespaces
