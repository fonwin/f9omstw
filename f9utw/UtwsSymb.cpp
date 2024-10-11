// \file f9utw/UtwsSymb.cpp
// \author fonwinz@gmail.com
#include "f9utw/UtwsSymb.hpp"
#include "f9utw/UtwOmsCoreUsrDef.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
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
UtwsSymb::~UtwsSymb() {
}
TwfExgSymbBasic* UtwsSymb::GetTwfExgSymbBasic() {
   return this;
}
fon9::fmkt::SymbData* UtwsSymb::GetSymbData(int tabid) {
   return GetUtwsSymbData(this, tabid);
}
fon9::fmkt::SymbData* UtwsSymb::FetchSymbData(int tabid) {
   return GetUtwsSymbData(this, tabid);
}
static fon9::seed::Fields UtwsSymb_MakeFields_MdDeal() {
   fon9::seed::Fields flds;
   flds.Add(fon9_MakeField(OmsMdLastPriceEv, LastPrice_, "LastPrice"));
   flds.Add(fon9_MakeField(OmsMdLastPriceEv, LastQty_,   "LastQty"));
   flds.Add(fon9_MakeField(OmsMdLastPriceEv, TotalQty_,  "TotalQty"));
   return flds;
}
static fon9::seed::Fields UtwsSymb_MakeFields_MdBS() {
   fon9::seed::Fields flds;
   char bsPriName[] = "-nP";
   char bsQtyName[] = "-nQ";
   int  idx;
   bsPriName[0] = bsQtyName[0] = 'S';
   for (idx = f9omstw_kMdBSCount - 1; idx >= 0; --idx) {
      bsPriName[1] = bsQtyName[1] = static_cast<char>(idx + '1');
      flds.Add(fon9_MakeField(UtwsSymb::MdBSEv, Sells_[idx].Pri_, bsPriName));
      flds.Add(fon9_MakeField(UtwsSymb::MdBSEv, Sells_[idx].Qty_, bsQtyName));
   }
   bsPriName[0] = bsQtyName[0] = 'B';
   for (idx = 0; idx < f9omstw_kMdBSCount; ++idx) {
      bsPriName[1] = bsQtyName[1] = static_cast<char>(idx + '1');
      flds.Add(fon9_MakeField(UtwsSymb::MdBSEv, Buys_[idx].Pri_, bsPriName));
      flds.Add(fon9_MakeField(UtwsSymb::MdBSEv, Buys_[idx].Qty_, bsQtyName));
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
   flds.Add(fon9_MakeField_const (UtwsSymb, SymbId_, "ShortId"));
   flds.Add(fon9_MakeField2_const(UtwsSymb, LongId));
   return new LayoutN(fon9_MakeField2(UtwsSymb, SymbId), treeflags,
         new Tab{fon9::Named{"Base"}, std::move(flds), TabFlag::NoSapling_NoSeedCommand_Writable},
         new Tab{fon9::Named{"MdDeal"},       UtwsSymb_MakeFields_MdDeal(), TabFlag::NoSapling_NoSeedCommand_Writable},
         new Tab{fon9::Named{"MdBS"},         UtwsSymb_MakeFields_MdBS(),   TabFlag::NoSapling_NoSeedCommand_Writable}
         );
}
//--------------------------------------------------------------------//
void UtwsSymb::LockMd() {
   this->CondReqList_.lock();
}
void UtwsSymb::UnlockMd() {
   this->CondReqList_.unlock();
}

static void RevPrint(fon9::RevBuffer& rbuf, const OmsMdLastPriceEv& dat) {
   fon9::RevPrint(rbuf, "|LastPrice=", dat.LastPrice_, "|LastQty=", dat.LastQty_, "|TotalQty=", dat.TotalQty_);
}
static void RevPrint(fon9::RevBuffer& rbuf, const OmsMdBSEv& dat) {
   fon9::RevPrint(rbuf, "|SP=", dat.Sell_.Pri_, "|SQ=", dat.Sell_.Qty_, "|BP=", dat.Buy_.Pri_, "|BQ=", dat.Buy_.Qty_);
}
bool UtwsSymb::AddCondReq(const MdLocker& mdLocker, int priority, const OmsCxBaseIniFn& cxReq, OmsCxBaseOrdRaw& cxRaw, OmsRequestRunStep& nextStep, fon9::RevBuffer& rbuf) {
   assert(cxReq.CondFn_ != nullptr && mdLocker.owns_lock());
   RevPrint(rbuf, '\n');
   if (IsEnumContains(cxReq.CondSrcEvMask_, OmsCxSrcEvMask::MdLastPrice_OddLot))
      RevPrint(rbuf, this->MdLastPriceEv_OddLot_);
   if (IsEnumContains(cxReq.CondSrcEvMask_, OmsCxSrcEvMask::MdBS_OddLot))
      RevPrint(rbuf, this->MdBSEv_OddLot_);
   if (IsEnumContainsAny(cxReq.CondSrcEvMask_, OmsCxSrcEvMask::MdLastPrice_OddLot | OmsCxSrcEvMask::MdBS_OddLot))
      RevPrint(rbuf, "|OddLot:");
   if (IsEnumContains(cxReq.CondSrcEvMask_, OmsCxSrcEvMask::MdLastPrice))
      RevPrint(rbuf, *this);
   if (IsEnumContains(cxReq.CondSrcEvMask_, OmsCxSrcEvMask::MdBS))
      RevPrint(rbuf, this->MdBSEv_);

   cxRaw.CondPri_ = cxReq.CondPri_;
   cxRaw.CondQty_ = cxReq.CondQty_;
   if (cxReq.CondFn_->OmsCx_IsNeedsFireNow(*this, cxRaw))
      return false;
   this->CondReqList_.Add(mdLocker, priority, cxReq, nextStep);

   if (IsEnumContains(cxReq.CondSrcEvMask_, OmsCxSrcEvMask::MdLastPrice))
      this->SetIsNeedsOnMdLastPriceEv(true);
   if (IsEnumContains(cxReq.CondSrcEvMask_, OmsCxSrcEvMask::MdLastPrice_OddLot))
      this->MdLastPriceEv_OddLot_.SetIsNeedsOnMdLastPriceEv(true);
   if (IsEnumContains(cxReq.CondSrcEvMask_, OmsCxSrcEvMask::MdBS))
      this->MdBSEv_.SetIsNeedsOnMdBSEv(true);
   if (IsEnumContains(cxReq.CondSrcEvMask_, OmsCxSrcEvMask::MdBS_OddLot))
      this->MdBSEv_OddLot_.SetIsNeedsOnMdBSEv(true);
   return true;
}
void UtwsSymb::CheckCondReq(OmsCxBaseCondEvArgs& args, OmsCoreMgr& coreMgr, fon9::RevBufferFixedMem& rbuf,
                            bool (OmsCxBaseCondFn::*onOmsCxEvFn)(const OmsCxBaseCondEvArgs& args)) {
   const char*    pbufTail = rbuf.GetCurrent();
   OmsCxSrcEvMask afEvMask{};
   OmsCore&       omsCore = *coreMgr.CurrentCore();
   args.LogRBuf_ = &rbuf;
   auto condList = this->CondReqList_.Lock();
   for (size_t idx = 0; idx < condList->size();) {
      const CondReq& creq = (*condList)[idx];
      //----- if (0); auto sno = dynamic_cast<const OmsRequestTrade*>(creq.CxReq_)->RxSNO(); //for debug.
      // 若 order 仍然有效, 則取出條件要判斷的內容參數;
      if ((args.CxRaw_ = creq.CxReq_->GetWaitingCxOrdRaw()) != nullptr) {
         // ----- 檢查條件是否成立?
         if (fon9_LIKELY((creq.CxReq_->CondFn_.get()->*onOmsCxEvFn)(args))) {
            // ----- 將 req 移到 [等候執行] 列表, 等候 OmsCore 的執行.
            assert(dynamic_cast<UtwOmsCoreUsrDef*>(omsCore.GetUsrDef()) != nullptr);
            static_cast<UtwOmsCoreUsrDef*>(omsCore.GetUsrDef())->AddCondFiredReq(omsCore, *creq.CxReq_, *creq.NextStep_, ToStrView(rbuf));
            // 還原 rbuf 已有的字串(行情內容)
            rbuf.SetPrefixUsed(const_cast<char*>(pbufTail));
         }
         else {
            // [idx]的條件單仍然有效,且此次異動判斷未成立,所以:
            // 保留[idx],下次有異動時繼續判斷;
            afEvMask |= creq.CxReq_->CondSrcEvMask_;
            ++idx;
            continue;
         }
      }
      // 從 condList 移除: (1)Order或條件已經失效; (2)條件成立已交給OmsCore處理;
      condList->erase(condList->begin() + fon9::signed_cast(idx));
   }
   this->                      SetIsNeedsOnMdLastPriceEv(IsEnumContains(afEvMask, OmsCxSrcEvMask::MdLastPrice));
   this->MdLastPriceEv_OddLot_.SetIsNeedsOnMdLastPriceEv(IsEnumContains(afEvMask, OmsCxSrcEvMask::MdLastPrice_OddLot));
   this->MdBSEv_              .SetIsNeedsOnMdBSEv       (IsEnumContains(afEvMask, OmsCxSrcEvMask::MdBS));
   this->MdBSEv_OddLot_       .SetIsNeedsOnMdBSEv       (IsEnumContains(afEvMask, OmsCxSrcEvMask::MdBS_OddLot));
}
//--------------------------------------------------------------------//
void UtwsSymb::OnMdLastPriceEv(const OmsMdLastPrice& bf, OmsCoreMgr& coreMgr) {
   OmsCxBaseCondEvArgs args{*this, bf, *this};
   fon9::RevBufferFixedSize<256> rbuf;
   // 條件成立: 從 condList 移除, 加入: 等候 OmsCore 執行的 Request 列表.
   // 必須在此建立 log 字串, 避免 Md 在 OmsCore 執行 Request 時有異動.
   RevPrint(rbuf, *static_cast<const OmsMdLastPriceEv*>(this));
   this->CheckCondReq(args, coreMgr, rbuf, &OmsCxBaseCondFn::OnOmsCx_MdLastPriceEv);
}
OmsMdLastPriceEv* UtwsSymb::GetMdLastPriceEv() {
   return this;
}
void UtwsSymb::MdLastPriceEv_OddLot::OnMdLastPriceEv(const OmsMdLastPrice& bf, OmsCoreMgr& coreMgr) {
   UtwsSymb&           mdSymb = fon9::ContainerOf(*this, &UtwsSymb::MdLastPriceEv_OddLot_);
   OmsCxBaseCondEvArgs args{mdSymb, bf, *this};
   fon9::RevBufferFixedSize<256> rbuf;
   fon9::RevPrint(rbuf, "|OddLot", *this);
   mdSymb.CheckCondReq(args, coreMgr, rbuf, &OmsCxBaseCondFn::OnOmsCx_MdLastPriceEv_OddLot);
}
OmsMdLastPriceEv* UtwsSymb::GetMdLastPriceEv_OddLot() {
   return &this->MdLastPriceEv_OddLot_;
}
//--------------------------------------------------------------------//
void UtwsSymb::MdBSEv::OnMdBSEv(const OmsMdBS& bf, OmsCoreMgr& coreMgr) {
   UtwsSymb&           mdSymb = fon9::ContainerOf(*this, &UtwsSymb::MdBSEv_);
   OmsCxBaseCondEvArgs args{mdSymb, bf, *this};
   fon9::RevBufferFixedSize<256> rbuf;
   RevPrint(rbuf, *this);
   mdSymb.CheckCondReq(args, coreMgr, rbuf, &OmsCxBaseCondFn::OnOmsCx_MdBSEv);
}
OmsMdBSEv* UtwsSymb::GetMdBSEv() {
   return &this->MdBSEv_;
}
//--------------------------------------------------------------------//
void UtwsSymb::MdBSEv_OddLot::OnMdBSEv(const OmsMdBS& bf, OmsCoreMgr& coreMgr) {
   UtwsSymb&           mdSymb = fon9::ContainerOf(*this, &UtwsSymb::MdBSEv_OddLot_);
   OmsCxBaseCondEvArgs args{mdSymb, bf, *this};
   fon9::RevBufferFixedSize<256> rbuf;
   fon9::RevPrint(rbuf, "|OddLot", *this);
   mdSymb.CheckCondReq(args, coreMgr, rbuf, &OmsCxBaseCondFn::OnOmsCx_MdBSEv_OddLot);
}
OmsMdBSEv* UtwsSymb::GetMdBSEv_OddLot() {
   return &this->MdBSEv_OddLot_;
}

} // namespaces
