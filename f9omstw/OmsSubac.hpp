// \file f9omstw/OmsSubac.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsSubac_hpp__
#define __f9omstw_OmsSubac_hpp__
#include "f9omstw/OmsIvSymb.hpp"
#include "fon9/seed/PodOp.hpp"

namespace f9omstw {

using OmsSubacSP = fon9::intrusive_ptr<OmsSubac>;
struct OmsSubacSP_Comper;
using OmsSubacMap = fon9::SortedVectorSet<OmsSubacSP, OmsSubacSP_Comper>;
using OmsSubacTree = OmsTreeSvctSet<OmsSubacMap, OmsIvBaseSP>;
//--------------------------------------------------------------------------//
class OmsSubac : public OmsIvBase {
   fon9_NON_COPY_NON_MOVE(OmsSubac);
   using base = OmsIvBase;
public:
   const fon9::CharVector  SubacNo_;

   OmsSubac(const fon9::StrView& subacNo, OmsIvacSP parent);
   ~OmsSubac();

   /// 建立 grid view, 包含 SubacNo_; 不含尾端分隔符號.
   /// 預設簡單輸出:
   /// `FieldsCellRevPrint(tab->Fields_, SimpleRawRd{*this}, rbuf);`
   /// `RevPrint(rbuf, this->SubacNo_);`
   virtual void MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf);

   struct PodOp : public fon9::seed::PodOpDefault {
      fon9_NON_COPY_NON_MOVE(PodOp);
      using base = fon9::seed::PodOpDefault;
   public:
      OmsSubac* Subac_;
      PodOp(fon9::seed::Tree& sender, OmsSubac* subac, const fon9::StrView& strKeyText)
         : base(sender, fon9::seed::OpResult::no_error, strKeyText)
         , Subac_{subac} {
      }
      void BeginRead(fon9::seed::Tab& tab, fon9::seed::FnReadOp fnCallback) override;
      void BeginWrite(fon9::seed::Tab& tab, fon9::seed::FnWriteOp fnCallback) override;

      typedef OmsIvSymbSP (*FnIvSymbMaker)(const fon9::StrView& symbid, OmsIvBase* owner);
      /// 不需要每個 ivSymbs 都有一個 fon9::seed::Tree,
      /// 所以在需要透過 fon9::seed 機制管理時, 才建立一個 tree 與 ivSymbs 對應。
      fon9::seed::TreeSP MakeIvSymbTree(fon9::seed::Tab& tab, FnIvSymbMaker fnIvSymbMaker, OmsIvSymbs& ivSymbs) const;
   };
   virtual void OnPodOp(OmsSubacTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) = 0;
};
//--------------------------------------------------------------------------//
struct OmsSubacSP_Comper {
   bool operator()(const OmsSubacSP& lhs, const OmsSubacSP& rhs) const {
      return lhs->SubacNo_ < rhs->SubacNo_;
   }
   bool operator()(const fon9::StrView& lhs, const OmsSubacSP& rhs) const {
      return lhs < ToStrView(rhs->SubacNo_);
   }
   bool operator()(const OmsSubacSP& lhs, const fon9::StrView& rhs) const {
      return ToStrView(lhs->SubacNo_) < rhs;
   }
};

} // namespaces
#endif//__f9omstw_OmsSubac_hpp__
