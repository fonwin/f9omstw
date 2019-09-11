// \file f9omstw/OmsIvac.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsIvac_hpp__
#define __f9omstw_OmsIvac_hpp__
#include "f9omstw/OmsSubac.hpp"
#include "f9omstw/IvacNo.hpp"
#include "fon9/LevelArray.hpp"

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
   /// 投資人帳號預設的下單線路.
   LgOut          LgOut_{};
   char           padding___[3];

   OmsIvac(IvacNo ivacNo, OmsBrkSP parent);
   ~OmsIvac();

   virtual void OnParentSeedClear();

   /// 建立 grid view, 包含 IvacNo_; 不含尾端分隔符號.
   /// 預設簡單輸出:
   /// `FieldsCellRevPrint(tab->Fields_, SimpleRawRd{*this}, rbuf);`
   /// `RevPrint(rbuf, this->IvacNo_);`
   virtual void MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf);

   struct PodOp : public fon9::seed::PodOpDefault {
      fon9_NON_COPY_NON_MOVE(PodOp);
      using base = fon9::seed::PodOpDefault;
   public:
      OmsIvac* Ivac_;
      PodOp(fon9::seed::Tree& sender, OmsIvac* ivac, const fon9::StrView& strKeyText)
         : base(sender, fon9::seed::OpResult::no_error, strKeyText)
         , Ivac_{ivac} {
      }
      void BeginRead(fon9::seed::Tab& tab, fon9::seed::FnReadOp fnCallback) override;
      void BeginWrite(fon9::seed::Tab& tab, fon9::seed::FnWriteOp fnCallback) override;

      typedef OmsSubacSP (*FnSubacMaker)(const fon9::StrView& subacNo, OmsIvBase* owner);
      fon9::seed::TreeSP MakeSubacTree(fon9::seed::Tab& tab, FnSubacMaker fnSubacMaker) const;

      typedef OmsIvSymbSP (*FnIvSymbMaker)(const fon9::StrView& symbid, OmsIvBase* owner);
      fon9::seed::TreeSP MakeIvSymbTree(fon9::seed::Tab& tab, FnIvSymbMaker fnIvSymbMaker, OmsIvSymbs& ivSymbs) const;
   };
   virtual void OnPodOp(OmsIvacTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) = 0;

   // 除非確實無此子帳, 否則不應刪除 subac, 因為:
   // - 若已有正確委託, 則該委託風控異動時, 仍會使用移除前的 OmsSubacSP.
   // - 若之後又建立一筆相同 SubacNo 的資料, 則會與先前移除的 OmsSubacSP 不同!
   OmsSubacSP RemoveSubac(fon9::StrView subacNo);
   OmsSubac* FetchSubac(fon9::StrView subacNo);
   OmsSubac* GetSubac(fon9::StrView subacNo) const;

   OmsSubacMap&       Subacs()       { return this->Subacs_; }
   const OmsSubacMap& Subacs() const { return this->Subacs_; }
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
