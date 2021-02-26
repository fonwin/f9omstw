// \file f9omstw/OmsBrk.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsBrk.hpp"
#include "f9omstw/OmsBrkTree.hpp"
#include "f9omstw/OmsIvacTree.hpp"

namespace f9omstw {

OmsIvac* OmsBrk::FetchIvac(IvacNo ivacNo) {
   if (OmsIvacSP* psp = this->GetIvacSP(ivacNo)) {
      if (OmsIvac* pivac = psp->get())
         return pivac->IvacNo_ == ivacNo ? pivac : nullptr;
      // 建立 ivac 之前, 是否需要檢查 ivacNo 的 check code 是否正確?
      // => 不檢查, 增加 ForceFetchIvac(); 用來重建「確定檢查碼正確」的帳號.
      return (*psp = this->MakeIvac(ivacNo)).get();
   }
   return nullptr;
}
OmsIvac* OmsBrk::ForceFetchIvac(IvacNo ivacNo) {
   if (OmsIvacSP* psp = this->GetIvacSP(ivacNo)) {
      if (OmsIvac* pivac = psp->get())
         if (pivac->IvacNo_ == ivacNo) // 帳號檢查碼正確 => pivac 可以直接使用.
            return pivac;
      // 帳號資料不存在, 或檢查碼不正確, 則建立新的.
      return (*psp = this->MakeIvac(ivacNo)).get();
   }
   return nullptr;
}
OmsIvacSP OmsBrk::RemoveIvac(IvacNo ivacNo) {
   if (OmsIvacSP* psp = this->GetIvacSP(ivacNo)) {
      if (OmsIvac* pivac = psp->get()) // 帳號存在
         if (pivac->IvacNo_ == ivacNo) // 且檢查碼正確 => 才算是找到要移除的帳號.
            return std::move(*psp);    // => 此時才能將指定的帳號移除.
   }
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
//--------------------------------------------------------------------------//
void OmsBrk::MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) {
   if (tab)
      FieldsCellRevPrint(tab->Fields_, fon9::seed::SimpleRawRd{*this}, rbuf);
   fon9::RevPrint(rbuf, this->BrkId_);
}
//--------------------------------------------------------------------------//
OmsBrk::PodOp::PodOp(OmsBrkTree& sender, OmsBrk* brk)
   : base(sender, fon9::seed::OpResult::no_error, ToStrView(brk->BrkId_))
   , Brk_{brk} {
}
fon9::seed::TreeSP OmsBrk::PodOp::GetSapling(fon9::seed::Tab& tab) {
   assert(OmsBrkTree::IsInOmsThread(this->Sender_));
   return new OmsIvacTree(static_cast<OmsBrkTree*>(this->Sender_)->OmsCore_, tab.SaplingLayout_, this->Brk_);
}
void OmsBrk::OnPodOp(OmsBrkTree& brkTree, fon9::seed::FnPodOp&& fnCallback) {
   PodOp op{brkTree, this};
   fnCallback(op, &op);
}

} // namespaces
