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
void OmsBrk::OnParentSeedClear() {
   this->ClearOrdNoMap();

   OmsIvacMap  tmpIvacs{std::move(this->Ivacs_)};
   IvacNC      i;
   if (auto* pivacSP = tmpIvacs.GetFirst(i)) {
      do {
         if (auto* ivac = pivacSP->get())
            ivac->OnParentSeedClear();
      } while ((pivacSP = tmpIvacs.GetNext(i)) != nullptr);
   }
}
void OmsBrk::OnDailyClear() {
   this->ClearOrdNoMap();
   // TODO: Ivacs_ 的 OnDailyClear();
   // - 移除後重新匯入? 如果有註冊「帳號回報」, 是否可以不要重新註冊?
   // - 匯入時處理 IsDailyClear?
   // - 先呼叫全部帳號的 OnDailyClear(); ?
}
void OmsBrk::ClearOrdNoMap() {
}

} // namespaces
