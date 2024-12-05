// \file f9utw/UtwsSymb.cpp
// \author fonwinz@gmail.com
#include "f9utw/UtwsSymb.hpp"
#include "f9utw/UtwOmsCoreUsrDef.hpp"
#include "f9omstw/OmsCxReqBase.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

static const int32_t kUtwsSymbOffset[]{
   0,
   fon9_OffsetOfBase(UtwsSymb, OmsMdLastPriceEv),
   fon9_OffsetOf(UtwsSymb, MdBSEv_),
   // 零股放在最後面,在期貨(無零股的)建立 MakeLayout() 時, 就可以直接不建立零股的Tab;
   fon9_OffsetOf(UtwsSymb, MdLastPriceEv_OddLot_),
   fon9_OffsetOf(UtwsSymb, MdBSEv_OddLot_),
};
static inline fon9::fmkt::SymbData* GetUtwsSymbData(UtwsSymb* pthis, int tabid) {
   return static_cast<size_t>(tabid) < fon9::numofele(kUtwsSymbOffset)
      ? reinterpret_cast<fon9::fmkt::SymbData*>(reinterpret_cast<char*>(pthis) + kUtwsSymbOffset[tabid])
      : nullptr;
}
fon9::fmkt::SymbData* UtwsSymb::GetSymbData(int tabid) {
   return GetUtwsSymbData(this, tabid);
}
fon9::fmkt::SymbData* UtwsSymb::FetchSymbData(int tabid) {
   return GetUtwsSymbData(this, tabid);
}
//--------------------------------------------------------------------//
static fon9::seed::Fields UtwsSymb_MakeFields_MdDeal() {
   fon9::seed::Fields flds;
   flds.Add(fon9_MakeField(OmsMdLastPriceEv, LastPrice_, "LastPrice"));
   flds.Add(fon9_MakeField(OmsMdLastPriceEv, LastQty_,   "LastQty"));
   flds.Add(fon9_MakeField(OmsMdLastPriceEv, TotalQty_,  "TotalQty"));
   return flds;
}
static fon9::seed::Fields UtwsSymb_MakeFields_MdBS() {
   fon9::seed::Fields flds;
   int  idx;
   char bsName[] = "-n";
   bsName[0] = 'S';
   for (idx = f9omstw_kMdBSCount - 1; idx >= 0; --idx) {
      bsName[1] = static_cast<char>(idx + '1');
      flds.Add(fon9_MakeField(UtwsSymb::MdBSEv, Sells_[idx], bsName));
   }
   bsName[0] = 'B';
   for (idx = 0; idx < f9omstw_kMdBSCount; ++idx) {
      bsName[1] = static_cast<char>(idx + '1');
      flds.Add(fon9_MakeField(UtwsSymb::MdBSEv, Buys_[idx], bsName));
   }
   return flds;
}
fon9::seed::LayoutSP UtwsSymb::MakeLayout(fon9::seed::TreeFlag treeflags, bool isStkMarket) {
   using namespace fon9::seed;
   auto flds = UtwsSymb::MakeFields();
   if (isStkMarket) {
      return new LayoutN(fon9_MakeField2(UtwsSymb, SymbId), treeflags,
                         new Tab{fon9::Named{"Base"}, std::move(flds), TabFlag::NoSapling_NoSeedCommand_Writable},
                         new Tab{fon9::Named{"MdDeal"},       UtwsSymb_MakeFields_MdDeal(), TabFlag::NoSapling_NoSeedCommand_Writable},
                         new Tab{fon9::Named{"MdBS"},         UtwsSymb_MakeFields_MdBS(),   TabFlag::NoSapling_NoSeedCommand_Writable},
                         new Tab{fon9::Named{"MdDealOddLot"}, UtwsSymb_MakeFields_MdDeal(), TabFlag::NoSapling_NoSeedCommand_Writable},
                         new Tab{fon9::Named{"MdBSOddLot"},   UtwsSymb_MakeFields_MdBS(),   TabFlag::NoSapling_NoSeedCommand_Writable}
      );
   }
   else {
      flds.Add(fon9_MakeField_const (UtwsSymb, SymbId_, "ShortId"));
      flds.Add(fon9_MakeField2_const(UtwsSymb, LongId));
      return new LayoutN(fon9_MakeField2(UtwsSymb, SymbId), treeflags,
                         new Tab{fon9::Named{"Base"}, std::move(flds), TabFlag::NoSapling_NoSeedCommand_Writable},
                         new Tab{fon9::Named{"MdDeal"},       UtwsSymb_MakeFields_MdDeal(), TabFlag::NoSapling_NoSeedCommand_Writable},
                         new Tab{fon9::Named{"MdBS"},         UtwsSymb_MakeFields_MdBS(),   TabFlag::NoSapling_NoSeedCommand_Writable}
      );
   }
}
// ======================================================================== //
UtwsSymb::~UtwsSymb() {
}
TwfExgSymbBasic* UtwsSymb::GetTwfExgSymbBasic() {
   return this;
}
// ======================================================================== //
void UtwsSymb::LockMd() {
   this->LockReqList();
}
void UtwsSymb::UnlockMd() {
   this->UnlockReqList();
}
void UtwsSymb::PrintMd(fon9::RevBuffer& rbuf, OmsCxSrcEvMask evMask) {
   RevPrint(rbuf, '\n');
   if (IsEnumContains(evMask, OmsCxSrcEvMask::MdLastPrice_OddLot))
      RevPrint(rbuf, this->GetMdLastPrice_OddLot());
   if (IsEnumContains(evMask, OmsCxSrcEvMask::MdBS_OddLot))
      RevPrint(rbuf, this->GetMdBS_OddLot());
   if (IsEnumContainsAny(evMask, OmsCxSrcEvMask::MdLastPrice_OddLot | OmsCxSrcEvMask::MdBS_OddLot))
      RevPrint(rbuf, "|OddLot:");
   if (IsEnumContains(evMask, OmsCxSrcEvMask::MdLastPrice))
      RevPrint(rbuf, this->GetMdLastPrice());
   if (IsEnumContains(evMask, OmsCxSrcEvMask::MdBS))
      RevPrint(rbuf, this->GetMdBS());
   RevPrint(rbuf, '|', this->SymbId_, ':');
}
void UtwsSymb::RegCondReq(const MdLocker& mdLocker, const CondReq& src) {
   assert(mdLocker.owns_lock());
   this->CondReqList_Add(mdLocker, src);
   //----- 檢查優先權排序是否正確:
   #ifdef _DEBUG
      auto& clist = this->GetCondReqList_ForDebug();
      const auto clistSz = clist.size();
      for (auto idx = clistSz; idx > 0;) {
         const auto& creq = clist[--idx];
         if (auto* txreq = dynamic_cast<const OmsRequestTrade*>(creq.CxRequest_)) {
            if (creq.CxRequest_ != src.CxRequest_)
               printf("%llu, ", txreq->RxSNO());
            else {
               bool isSortedOK = true;
               if (idx > 0 && (clist[idx - 1].Priority_ <= src.Priority_))
                  isSortedOK = false;
               if ((idx + 1 < clistSz) && clist[idx + 1].Priority_ > src.Priority_)
                  isSortedOK = false;
               printf("[%llu:%s], ", txreq->RxSNO(), isSortedOK ? "OK" : "ERR");
            }
         }
      }
      puts("");
   #endif
   //-----
   const OmsCxSrcEvMask evMask = src.CxUnit_->RegEvMask();
   if (IsEnumContains(evMask, OmsCxSrcEvMask::MdLastPrice))          this->                      SetIsNeedsOnMdLastPriceEv(true);
   if (IsEnumContains(evMask, OmsCxSrcEvMask::MdLastPrice_OddLot))   this->MdLastPriceEv_OddLot_.SetIsNeedsOnMdLastPriceEv(true);
   if (IsEnumContains(evMask, OmsCxSrcEvMask::MdBS))                 this->MdBSEv_.              SetIsNeedsOnMdBSEv(true);
   if (IsEnumContains(evMask, OmsCxSrcEvMask::MdBS_OddLot))          this->MdBSEv_OddLot_.       SetIsNeedsOnMdBSEv(true);
}
// ======================================================================== //
void UtwsSymb::CheckCondReq(OmsCxMdEvArgs& args, OnOmsCxMdEvFnT onOmsCxEvFn) {
   auto           condList = this->LockCondReqList();
   OmsCxSrcEvMask afEvMask = condList->CheckCondReq(args, onOmsCxEvFn);
   this->                      SetIsNeedsOnMdLastPriceEv(IsEnumContains(afEvMask, OmsCxSrcEvMask::MdLastPrice));
   this->MdLastPriceEv_OddLot_.SetIsNeedsOnMdLastPriceEv(IsEnumContains(afEvMask, OmsCxSrcEvMask::MdLastPrice_OddLot));
   this->MdBSEv_              .SetIsNeedsOnMdBSEv       (IsEnumContains(afEvMask, OmsCxSrcEvMask::MdBS));
   this->MdBSEv_OddLot_       .SetIsNeedsOnMdBSEv       (IsEnumContains(afEvMask, OmsCxSrcEvMask::MdBS_OddLot));
}
//--------------------------------------------------------------------//
void UtwsSymb::OnMdLastPriceEv(const OmsMdLastPrice& bf, OmsCore& omsCore) {
   OmsCxMdEvArgs  args{omsCore, *this, bf, *this, OmsCxSrcEvMask::MdLastPrice};
   // 條件成立: 從 condList 移除, 加入: 等候 OmsCore 執行的 Request 列表.
   // 必須在此建立 log 字串, 避免 Md 在 OmsCore 執行 Request 時有異動.
   RevPrint(args.LogRBuf_, *static_cast<const OmsMdLastPriceEv*>(this));
   this->CheckCondReq(args, &OmsCxUnit::OnOmsCx_MdLastPriceEv);
}
OmsMdLastPriceEv* UtwsSymb::GetMdLastPriceEv() {
   return this;
}
//--------------------------------------------------------------------//
void UtwsSymb::MdLastPriceEv_OddLot::OnMdLastPriceEv(const OmsMdLastPrice& bf, OmsCore& omsCore) {
   UtwsSymb&      mdSymb = fon9::ContainerOf(*this, &UtwsSymb::MdLastPriceEv_OddLot_);
   OmsCxMdEvArgs  args{omsCore, mdSymb, bf, *this, OmsCxSrcEvMask::MdLastPrice_OddLot};
   fon9::RevPrint(args.LogRBuf_, "|OddLot", *this);
   mdSymb.CheckCondReq(args, &OmsCxUnit::OnOmsCx_MdLastPriceEv_OddLot);
}
OmsMdLastPriceEv* UtwsSymb::GetMdLastPriceEv_OddLot() {
   return &this->MdLastPriceEv_OddLot_;
}
//--------------------------------------------------------------------//
void UtwsSymb::MdBSEv::OnMdBSEv(const OmsMdBS& bf, OmsCore& omsCore) {
   UtwsSymb&      mdSymb = fon9::ContainerOf(*this, &UtwsSymb::MdBSEv_);
   OmsCxMdEvArgs  args{omsCore, mdSymb, bf, *this, OmsCxSrcEvMask::MdBS};
   RevPrint(args.LogRBuf_, *this);
   mdSymb.CheckCondReq(args, &OmsCxUnit::OnOmsCx_MdBSEv);
}
OmsMdBSEv* UtwsSymb::GetMdBSEv() {
   return &this->MdBSEv_;
}
//--------------------------------------------------------------------//
void UtwsSymb::MdBSEv_OddLot::OnMdBSEv(const OmsMdBS& bf, OmsCore& omsCore) {
   UtwsSymb&      mdSymb = fon9::ContainerOf(*this, &UtwsSymb::MdBSEv_OddLot_);
   OmsCxMdEvArgs  args{omsCore, mdSymb, bf, *this, OmsCxSrcEvMask::MdBS_OddLot};
   fon9::RevPrint(args.LogRBuf_, "|OddLot", *this);
   mdSymb.CheckCondReq(args, &OmsCxUnit::OnOmsCx_MdBSEv_OddLot);
}
OmsMdBSEv* UtwsSymb::GetMdBSEv_OddLot() {
   return &this->MdBSEv_OddLot_;
}

} // namespaces
