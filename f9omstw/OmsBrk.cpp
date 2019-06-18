// \file f9omstw/OmsBrk.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsBrk.hpp"

namespace f9omstw {

OmsIvac* OmsBrk::FetchIvac(IvacNo ivacNo) {
   IvacNC ivn = IvacNoToNC(ivacNo);
   if (ivn > IvacNC::Max)
      return nullptr;
   OmsIvacSP& sp = this->Ivacs_[ivn];
   if (sp)
      return sp->IvacNo_ == ivacNo ? sp.get() : nullptr;
   // TODO: 檢查 ivacNo 的 check code 是否正確?
   sp = this->MakeIvac(ivacNo);
   return sp.get();
}
OmsIvacSP OmsBrk::RemoveIvac(IvacNo ivacNo) {
   IvacNC ivn = IvacNoToNC(ivacNo);
   if (ivn > IvacNC::Max)
      return nullptr;
   OmsIvacSP& sp = this->Ivacs_[ivn];
   if (sp && sp->IvacNo_ == ivacNo)
      return std::move(sp);
   return nullptr;
}
//--------------------------------------------------------------------------//
fon9_MSC_WARN_DISABLE(4583); // 'OmsMarketRec::SessionAry_': destructor is not implicitly called
OmsMarketRec::~OmsMarketRec() {
   fon9::destroy_at(&this->SessionAry_);
}
void OmsMarketRec::InitializeSessionAry() {
   unsigned idx = 0;
   for (auto& v : this->SessionAry_)
      fon9::InplaceNew(&v, *this, IndexTo_f9fmkt_TradingSessionId(idx++));
}

OmsBrk::~OmsBrk() {
   fon9::destroy_at(&this->MarketAry_);
}
void OmsBrk::InitializeMarketAry() {
   unsigned idx = 0;
   for (auto& v : this->MarketAry_)
      fon9::InplaceNew(&v, *this, IndexTo_f9fmkt_TradingMarket(idx++));
}
fon9_MSC_WARN_POP;
//--------------------------------------------------------------------------//
void OmsBrk::OnParentSeedClear() {
   OmsIvacMap  tmpIvacs{std::move(this->Ivacs_)};
   IvacNC      i;
   if (auto* pivacSP = tmpIvacs.GetFirst(i)) {
      do {
         if (auto* ivac = pivacSP->get())
            ivac->OnParentSeedClear();
      } while ((pivacSP = tmpIvacs.GetNext(i)) != nullptr);
   }
}

} // namespaces
