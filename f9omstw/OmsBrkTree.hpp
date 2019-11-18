// \file f9omstw/OmsBrkTree.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsBrkTree_hpp__
#define __f9omstw_OmsBrkTree_hpp__
#include "f9omstw/OmsBrk.hpp"
#include "f9omstw/OmsTree.hpp"
#include "f9omstw/OmsTools.hpp"
#include "fon9/StrTo.hpp"
#include "f9tws/ExgTypes.hpp"

namespace f9omstw {

fon9_WARN_DISABLE_PADDING;
/// 適用於台灣證券環境的「券商資料表」.
/// - 以一家台灣的綜合券商而言, 券商代號可能為 AA-BB 或 AAA-B, B=分公司代號(0-9,A-Z,a-z);
/// - 以一家台灣的期貨商而言, 期貨商代號為 X999BBB, BBB=000-999;
/// - 所以 OMS 可以用陣列來處理券商資料表:
///   - index = BB = xy = Alpha2Seq(x) * kSeq2AlphaSize + Alpha2Seq(y);   
///     或 index = B = Alpha2Seq(B)
///   - index = Pic9StrTo<3,uint16_t>(BBB);
/// - 為了簡化設計、加快執行期間的速度:
///   OmsCore 在初始化階段就會建立 OmsBrkTree 並將券商資料表填妥.
class OmsBrkTree : public OmsTree {
   fon9_NON_COPY_NON_MOVE(OmsBrkTree);
   using base = OmsTree;
   struct TreeOp;

   using BrkRecs = std::vector<OmsBrkSP>;
   BrkRecs  BrkRecs_;
   int      IdxStart_{-1};
   
   void BrksClear(void(OmsBrk::*fnClear)());

public:
   typedef int (*FnGetBrkIndex)(fon9::StrView brkid);
   const FnGetBrkIndex  FnGetBrkIndex_;

   OmsBrkTree(OmsCore& omsCore, fon9::seed::LayoutSP layout, FnGetBrkIndex fnGetBrkIndex);
   ~OmsBrkTree();

   typedef OmsBrkSP (*FnBrkMaker)(const fon9::StrView& brkid);
   void Initialize(FnBrkMaker fnBrkMaker, fon9::StrView start, size_t count, FnIncStr fnIncStr);

   /// 設定 f9fmkt_TradingSessionId_Normal, FixedPrice, OddLot 共用同一個 OrdNoMap.
   void InitializeTwsOrdNoMap(f9fmkt_TradingMarket mkt);
   /// 若上市上櫃共用委託書號表, 則可在上市初始化之後, 呼叫: InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwOTC, f9fmkt_TradingMarket_TwSEC);
   void InitializeTwsOrdNoMapRef(f9fmkt_TradingMarket mkt, f9fmkt_TradingMarket mktRefSource);
   void InitializeTwfOrdNoMap(f9fmkt_TradingMarket mkt);
   void InitializeTwfOrdNoMapRef(f9fmkt_TradingMarket mkt, f9fmkt_TradingMarket mktRefSource);

   static int TwsBrkIndex1(fon9::StrView brkid) {
      return brkid.size() == sizeof(f9tws::BrkId) ? fon9::Alpha2Seq(*(brkid.end() - 1)) : -1;
   }
   static int TwsBrkIndex2(fon9::StrView brkid) {
      return brkid.size() == sizeof(f9tws::BrkId)
         ? fon9::Alpha2Seq(*(brkid.end() - 2)) * fon9::kSeq2AlphaSize + fon9::Alpha2Seq(*(brkid.end() - 1))
         : -1;
   }
   static int TwfBrkIndex(fon9::StrView brkid) {
      return brkid.size() == 7 ? fon9::Pic9StrTo<3, uint16_t>(brkid.end() - 3) : -1;
   }

   size_t GetBrkIndex(const fon9::StrView& brkid) const {
      assert(this->IdxStart_ >= 0);
      return static_cast<size_t>(this->FnGetBrkIndex_(brkid) - this->IdxStart_);
   }
   OmsBrk* GetBrkRec(const fon9::StrView& brkid) const {
      if (OmsBrk* brk = this->GetBrkRec(this->GetBrkIndex(brkid)))
         if (ToStrView(brk->BrkId_) == brkid)
            return brk;
      return nullptr;
   }
   OmsBrk* GetBrkRec(size_t idx) const {
      if (fon9_LIKELY(idx < this->BrkRecs_.size()))
         return this->BrkRecs_[idx].get();
      return nullptr;
   }

   OmsIvBase* GetIvr(fon9::StrView brkid, IvacNo ivacNo, fon9::StrView subacNo) {
      if (auto* brk = this->GetBrkRec(brkid)) {
         if (auto* ivac = brk->GetIvac(ivacNo)) {
            if (subacNo.empty())
               return ivac;
            return ivac->GetSubac(subacNo);
         }
      }
      return nullptr;
   }
   OmsIvBase* FetchIvr(fon9::StrView brkid, IvacNo ivacNo, fon9::StrView subacNo) {
      if (auto* brk = this->GetBrkRec(brkid)) {
         if (auto* ivac = brk->FetchIvac(ivacNo)) {
            if (subacNo.empty())
               return ivac;
            return ivac->FetchSubac(subacNo);
         }
      }
      return nullptr;
   }

   void InThr_OnTreeOp(fon9::seed::FnTreeOp fnCallback) override;
   void InThr_OnParentSeedClear() override;

   constexpr static fon9::seed::TreeFlag DefaultTreeFlag() {
      return fon9::seed::TreeFlag{};
   }
};
fon9_WARN_POP;

} // namespaces
#endif//__f9omstw_OmsBrkTree_hpp__
