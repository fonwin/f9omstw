// \file f9utw/UtwsSymb.hpp
// \author fonwinz@gmail.com
#ifndef __f9utw_UtwsSymb_hpp__
#define __f9utw_UtwsSymb_hpp__
#include "f9omstw/OmsSymb.hpp"
#include "f9omstw/OmsCxSymb.hpp"
#include "f9omstwf/OmsTwfExgSymbBasic.hpp"

namespace f9omstw {

/// 這裡提供一個 OMS 商品基本資料的範例.
class UtwsSymb : public OmsSymb, public OmsCxSymb, public TwfExgSymbBasic/*包含了 OmsMdLastPriceEv*/ {
   fon9_NON_COPY_NON_MOVE(UtwsSymb);
   using base = OmsSymb;

protected:
   void CheckCondReq(OmsCxMdEvArgs& args, OnOmsCxMdEvFnT onOmsCxEvFn);

public:
   using base::base;
   ~UtwsSymb();

   fon9::fmkt::SymbData* GetSymbData(int tabid) override;
   fon9::fmkt::SymbData* FetchSymbData(int tabid) override;
   TwfExgSymbBasic* GetTwfExgSymbBasic() override;

   static fon9::seed::LayoutSP MakeLayout(fon9::seed::TreeFlag flags, bool isStkMarket = false);

   static OmsSymbSP SymbMaker(const fon9::StrView& symbid) {
      return new UtwsSymb{symbid};
   }
   //--------------------------------------------------------------------//
   void LockMd() override;
   void UnlockMd() override;
   void RegCondReq(const MdLocker& mdLocker, const CondReq& src);
   void PrintMd(fon9::RevBuffer& rbuf, OmsCxSrcEvMask evMask);
   //--------------------------------------------------------------------//
   OmsMdLastPriceEv* GetMdLastPriceEv() override;
   OmsMdLastPriceEv& GetMdLastPrice() { return *this; }
   void OnMdLastPriceEv(const OmsMdLastPrice& bf, OmsCore& omsCore) override;
   //--------------------------------------------------------------------//
   struct MdLastPriceEv_OddLot : public OmsMdLastPriceEv {
      fon9_NON_COPY_NON_MOVE(MdLastPriceEv_OddLot);
      MdLastPriceEv_OddLot () = default;
      void OnMdLastPriceEv(const OmsMdLastPrice& bf, OmsCore&) override;
      friend class UtwsSymb;//using OmsMdLastPriceEv::SetIsNeedsOnMdLastPriceEv;
   };
   MdLastPriceEv_OddLot MdLastPriceEv_OddLot_;
   OmsMdLastPriceEv* GetMdLastPriceEv_OddLot() override;
   OmsMdLastPriceEv& GetMdLastPrice_OddLot() { return this->MdLastPriceEv_OddLot_; }
   //--------------------------------------------------------------------//
   struct MdBSEv : public OmsMdBSEv {
      fon9_NON_COPY_NON_MOVE(MdBSEv);
      MdBSEv() = default;
      void OnMdBSEv(const OmsMdBS& bf, OmsCore&) override;
      friend class UtwsSymb;//using OmsMdBSEv::SetIsNeedsOnMdBSEv;
   };
   MdBSEv        MdBSEv_;
   OmsMdBSEv* GetMdBSEv() override;
   OmsMdBSEv& GetMdBS() { return this->MdBSEv_; }
   //--------------------------------------------------------------------//
   struct MdBSEv_OddLot : public OmsMdBSEv {
      fon9_NON_COPY_NON_MOVE(MdBSEv_OddLot);
      MdBSEv_OddLot() = default;
      void OnMdBSEv(const OmsMdBS& bf, OmsCore&) override;
      friend class UtwsSymb;//using OmsMdBSEv::SetIsNeedsOnMdBSEv;
   };
   MdBSEv_OddLot MdBSEv_OddLot_;
   OmsMdBSEv* GetMdBSEv_OddLot() override;
   OmsMdBSEv& GetMdBS_OddLot() { return this->MdBSEv_OddLot_; }
};

} // namespaces
#endif//__f9utw_UtwsSymb_hpp__
