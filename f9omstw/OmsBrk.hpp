// \file f9omstw/OmsBrk.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsBrk_hpp__
#define __f9omstw_OmsBrk_hpp__
#include "f9omstw/OmsIvac.hpp"

namespace f9omstw {

// 不同的 OMS 實現, 必須定義自己需要的 class UtwsBrk; 或 UtwfBrk;
/// 這裡做為底層提供:
/// - 委託書號對照表: Market + Session + OrdNo
/// - 投資人帳號資料: OmsIvacMap
class OmsBrk : public OmsIvBase {
   fon9_NON_COPY_NON_MOVE(OmsBrk);
   using base = OmsIvBase;
   void ClearOrdNoMap();

protected:
   OmsIvacMap Ivacs_;

   virtual OmsIvacSP MakeIvac(IvacNo) = 0;

public:
   const fon9::CharVector  BrkId_;
   OmsBrk(const fon9::StrView& brkid) : base(OmsIvKind::Brk, nullptr), BrkId_(brkid) {
   }

   // 除非 ivac->IvacNo_ 檢查碼有錯, 或確實無此帳號, 否則不應刪除 ivac, 因為:
   // - 若已有正確委託, 則該委託風控異動時, 仍會使用移除前的 OmsIvacSP.
   // - 若之後又建立一筆相同 IvacNo 的資料, 則會與先前移除的 OmsIvacSP 不同!
   OmsIvacSP RemoveIvac(IvacNo ivacNo);
   OmsIvac* FetchIvac(IvacNo ivacNo);
   OmsIvac* GetIvac(IvacNC ivacNC) const {
      if (auto* p = this->Ivacs_.Get(ivacNC))
         return p->get();
      return nullptr;
   }
   OmsIvac* GetIvac(IvacNo ivacNo) const {
      if (auto* p = this->Ivacs_.Get(IvacNoToNC(ivacNo)))
         if (auto* ivac = p->get())
            if (ivac->IvacNo_ == ivacNo)
               return ivac;
      return nullptr;
   }

   virtual void OnParentSeedClear();
   virtual void OnDailyClear();

   /// 建立 grid view, 包含 BrkId_; 不含尾端分隔符號.
   virtual void MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) = 0;
   virtual void OnPodOp(OmsBrkTree& brkTree, fon9::seed::FnPodOp&& fnCallback) = 0;
};
//--------------------------------------------------------------------------//
inline OmsIvac::OmsIvac(IvacNo ivacNo, OmsBrkSP parent)
   : base(OmsIvKind::Ivac, std::move(parent))
   , IvacNo_{ivacNo} {
}

} // namespaces
#endif//__f9omstw_OmsBrk_hpp__
