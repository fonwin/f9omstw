// \file f9omstw/OmsIvac.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsIvac_hpp__
#define __f9omstw_OmsIvac_hpp__
#include "f9omstw/OmsSubac.hpp"
#include "f9omstw/IvacNo.hpp"
#include "fon9/LevelArray.hpp"
#include "fon9/seed/PodOp.hpp"

namespace f9omstw {

class OmsIvac : public OmsIvBase {
   fon9_NON_COPY_NON_MOVE(OmsIvac);
   using base = OmsIvBase;

protected:
   OmsSubacMap  Subacs_;

   virtual OmsSubacSP MakeSubac(fon9::StrView subacNo) = 0;

public:
   /// 帳號包含檢查碼.
   const IvacNo   IvacNo_;
   char           padding___[4];

   OmsIvac(IvacNo ivacNo, OmsBrkSP parent);
   ~OmsIvac();

   /// 建立 grid view, 包含 IvacNo_; 不含尾端分隔符號.
   virtual void MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) = 0;
   virtual void OnPodOp(OmsIvacTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) = 0;
   virtual void OnParentSeedClear();

   // 除非確實無此子帳, 否則不應刪除 subac, 因為:
   // - 若已有正確委託, 則該委託風控異動時, 仍會使用移除前的 OmsSubacSP.
   // - 若之後又建立一筆相同 SubacNo 的資料, 則會與先前移除的 OmsSubacSP 不同!
   OmsSubacSP RemoveSubac(fon9::StrView subacNo);
   OmsSubac* FetchSubac(fon9::StrView subacNo);
   OmsSubac* GetSubac(fon9::StrView subacNo) const;
};

/// key = 移除檢查碼之後的帳號;
using OmsIvacMap = fon9::LevelArray<IvacNC, OmsIvacSP, 3>;

//--------------------------------------------------------------------------//

inline OmsSubac::OmsSubac(const fon9::StrView& subacNo, OmsIvacSP parent)
   : base(OmsIvKind::Subac, std::move(parent))
   , SubacNo_{subacNo} {
}

} // namespaces
#endif//__f9omstw_OmsIvac_hpp__
