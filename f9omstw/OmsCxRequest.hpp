// \file f9omstw/OmsCxRequest.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCxRequest_hpp__
#define __f9omstw_OmsCxRequest_hpp__
#include "f9omstw/OmsRequestTrade.hpp"
#include "f9omstw/OmsMdEvent.hpp"

namespace f9omstw {

//--------------------------------------------------------------------------//
/// - 包含 CondPri 的修改;
/// - 其他 Cond 欄位不允許修改, 若要調整, 應刪除後重下;
struct OmsCxLastPriceChangeable {
   fon9::fmkt::Pri   CondPri_{fon9::fmkt::Pri::Null()};

   template <class Derivid>
   static void CxMakeFields(fon9::seed::Fields& flds, const Derivid* = nullptr) {
      MakeFields(flds, fon9_OffsetOfBase(Derivid, OmsCxLastPriceChangeable));
   }
   static void MakeFields(fon9::seed::Fields& flds, int ofsadj);

   void ContinuePrevUpdate(const OmsCxLastPriceChangeable& prev) {
      this->CondPri_ = prev.CondPri_;
   }
};

using OmsCxLastPriceChgDat = OmsCxLastPriceChangeable;
using OmsCxLastPriceOrdRaw = OmsCxLastPriceChangeable;
//--------------------------------------------------------------------------//
struct OmsCxLastPriceIniDat {
   fon9::CharAry<2>  CondOp_{nullptr};
   char              Padding___[6];
   fon9::fmkt::Pri   CondPri_{fon9::fmkt::Pri::Null()};

   template <class Derivid>
   static void CxMakeFields(fon9::seed::Fields& flds, const Derivid* = nullptr) {
      MakeFields(flds, fon9_OffsetOfBase(Derivid, OmsCxLastPriceIniDat));
   }
   static void MakeFields(fon9::seed::Fields& flds, int ofsadj);
};

using OmsCxLastPriceCondFn = bool (*)(const OmsCxLastPriceOrdRaw& ordraw, const OmsMdLastPrice& bf, const OmsMdLastPriceEv& af);
/// 走[下單流程]的 Request 需要 CondFn_; 所以需繼承 OmsCxLastPriceIniFn;
/// 走[回報流程]的 Request 不需 CondFn_; 所以只要繼承 OmsCxLastPriceIniDat;
struct OmsCxLastPriceIniFn : public OmsCxLastPriceIniDat {
   OmsCxLastPriceCondFn    CondFn_{};
   /// 根據 CondOp_、CondPri_ 設定 CondFn_;
   /// 若沒有指定 CondOp_、CondPri_ 則: CondFn_ = nullptr, 返回 true,
   /// 若 CondOp_、CondPri_ 不正確, 則: CondFn_ = nullptr, 返回 false,
   bool ParseCondOp();
};
//--------------------------------------------------------------------------//
template <class TBase, class TCxData>
class OmsCxCombine : public TBase, public TCxData {
   fon9_NON_COPY_NON_MOVE(OmsCxCombine);
   using base = TBase;
public:
   using base::base;
   OmsCxCombine() = default;
   static void MakeFields(fon9::seed::Fields& flds) {
      base::MakeFields(flds);
      TCxData::CxMakeFields(flds, static_cast<OmsCxCombine*>(nullptr));
   }
};
//--------------------------------------------------------------------------//

} // namespaces
#endif//__f9omstw_OmsCxRequest_hpp__
