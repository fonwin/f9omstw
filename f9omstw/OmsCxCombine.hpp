// \file f9omstw/OmsCxCombine.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCxCombine_hpp__
#define __f9omstw_OmsCxCombine_hpp__
#include "f9omstw/OmsOrder.hpp"

namespace f9omstw {

// ======================================================================== //
/// 協助合併 TBase::MakeFields() 及 TCxData::CxMakeFields();
template <class TBase, class TCxData>
class OmsCxCombine : public TBase, public TCxData {
   fon9_NON_COPY_NON_MOVE(OmsCxCombine);
public:
   using TBase::TBase;
   OmsCxCombine() = default;
   static void MakeFields(fon9::seed::Fields& flds) {
      TBase::MakeFields(flds);
      TCxData::CxMakeFields(flds, static_cast<OmsCxCombine*>(nullptr));
   }
};
//--------------------------------------------------------------------------//
template <class TOrdRaw, class TCxData>
class OmsCxCombineOrdRaw : public OmsCxCombine<TOrdRaw, TCxData> {
   fon9_NON_COPY_NON_MOVE(OmsCxCombineOrdRaw);
   using base = OmsCxCombine<TOrdRaw, TCxData>;
public:
   using base::base;
   OmsCxCombineOrdRaw() = default;

   void ContinuePrevUpdate(const OmsOrderRaw& prev) override {
      if (fon9_LIKELY(prev.UpdateOrderSt_ >= f9fmkt_OrderSt_NewStarting))
         TOrdRaw::ContinuePrevUpdate(prev);
      else {
         // 在 WaitingCond 狀態下的委託: assert(LeavesQty_ != 0)
         // (1) 若 LeavesQty_ != AfterQty_ : assert(AfterQty_ == 0); 則為: [尚未風控] 的條件單.
         // (2) 若 LeavesQty_ == AfterQty_ :                         則為: [已經風控] 的條件單;
         const auto afQty = static_cast<const OmsCxCombineOrdRaw*>(&prev)->AfterQty_;
         TOrdRaw::ContinuePrevUpdate(prev);
         this->BeforeQty_ = this->AfterQty_ = afQty;
      }
      TCxData::ContinuePrevUpdate(*static_cast<const OmsCxCombineOrdRaw*>(&prev));
   }
};
// ======================================================================== //

} // namespaces
#endif//__f9omstw_OmsCxCombine_hpp__
