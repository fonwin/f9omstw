// \file f9omstw/OmsBrkTree.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsBrkTree.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/RevPrint.hpp"

namespace f9omstw {

OmsBrkTree::OmsBrkTree(OmsCore& omsCore, fon9::seed::LayoutSP layout, FnGetBrkIndex fnGetBrkIndex)
   : base(omsCore, std::move(layout))
   , FnGetBrkIndex_{fnGetBrkIndex} {
}
OmsBrkTree::~OmsBrkTree() {
}
void OmsBrkTree::Initialize(FnBrkMaker fnBrkMaker, fon9::StrView start, size_t count, FnIncStr fnIncStr) {
   assert(this->IdxStart_ < 0 && this->BrkRecs_.empty() && !start.empty() && count > 0);
   this->IdxStart_ = this->FnGetBrkIndex_(start);
   if (count <= 0)
      return;
   this->BrkRecs_.reserve(count);
   fon9::CharVector  brkidBuffer{start};
   start = ToStrView(brkidBuffer);
   brkidBuffer.push_back('\0');
   for (size_t L = 0;;) {
      this->BrkRecs_.emplace_back(fnBrkMaker(start));
      if (L != this->GetBrkIndex(start)) {
         // 也許建構時 FnGetBrkIndex 選錯, 例如券商代號是2碼, 應使用 &OmsBrkTree::TwsBrkIndex2;
         fprintf(stderr, "[ERROR] OmsBrkTree::Initialize|BrkId=%s|Index=%u|Expected=%u\n",
                 start.begin(), static_cast<unsigned>(this->GetBrkIndex(start)), static_cast<unsigned>(L));
      }
      if (++L >= count)
         break;
      fnIncStr(const_cast<char*>(start.begin()), const_cast<char*>(start.end()));
   }
}

static void RefSameOrdNoMap(OmsMarketRec& mktRec, OmsMarketRec& mktSrc, f9fmkt_TradingSessionId sesid) {
   assert(mktSrc.GetSession(sesid).GetOrdNoMap() != nullptr);
   mktRec.GetSession(sesid).InitializeOrdNoMapRef(mktSrc.GetSession(sesid).GetOrdNoMap());
}
void OmsBrkTree::InitializeTwsOrdNoMapRef(f9fmkt_TradingMarket mkt, f9fmkt_TradingMarket mktRefSource) {
   assert(mktRefSource != f9fmkt_TradingMarket_Unknown);
   for (OmsBrkSP& pbrk : this->BrkRecs_) {
      OmsBrk& brk = *pbrk;
      auto&   mktRec = brk.GetMarket(mkt);
      auto&   mktSrc = brk.GetMarket(mktRefSource);
      RefSameOrdNoMap(mktRec, mktSrc, f9fmkt_TradingSessionId_Normal);
      RefSameOrdNoMap(mktRec, mktSrc, f9fmkt_TradingSessionId_FixedPrice);
      RefSameOrdNoMap(mktRec, mktSrc, f9fmkt_TradingSessionId_OddLot);
   }
}
void OmsBrkTree::InitializeTwsOrdNoMap(f9fmkt_TradingMarket mkt) {
   for (OmsBrkSP& pbrk : this->BrkRecs_) {
      OmsBrk& brk = *pbrk;
      auto&   mktRec = brk.GetMarket(mkt);
      auto    ordNoMap = mktRec.GetSession(f9fmkt_TradingSessionId_Normal).InitializeOrdNoMap();
      mktRec.GetSession(f9fmkt_TradingSessionId_FixedPrice).InitializeOrdNoMapRef(ordNoMap);
      mktRec.GetSession(f9fmkt_TradingSessionId_OddLot).InitializeOrdNoMapRef(ordNoMap);
   }
}

void OmsBrkTree::InitializeTwfOrdNoMapRef(f9fmkt_TradingMarket mkt, f9fmkt_TradingMarket mktRefSource) {
   assert(mktRefSource != f9fmkt_TradingMarket_Unknown);
   for (OmsBrkSP& pbrk : this->BrkRecs_) {
      OmsBrk& brk = *pbrk;
      auto&   mktRec = brk.GetMarket(mkt);
      auto&   mktSrc = brk.GetMarket(mktRefSource);
      RefSameOrdNoMap(mktRec, mktSrc, f9fmkt_TradingSessionId_Normal);
      RefSameOrdNoMap(mktRec, mktSrc, f9fmkt_TradingSessionId_AfterHour);
   }
}
void OmsBrkTree::InitializeTwfOrdNoMap(f9fmkt_TradingMarket mkt) {
   for (OmsBrkSP& pbrk : this->BrkRecs_) {
      OmsBrk& brk = *pbrk;
      auto&   mktRec = brk.GetMarket(mkt);
      auto    ordNoMap = mktRec.GetSession(f9fmkt_TradingSessionId_Normal).InitializeOrdNoMap();
      mktRec.GetSession(f9fmkt_TradingSessionId_AfterHour).InitializeOrdNoMapRef(ordNoMap);
   }
}

OmsOrdNoMapSP OmsSessionRec::InitializeOrdNoMap() {
   assert(this->OrdNoMap_.get() == nullptr);
   this->OrdNoMap_.reset(new OmsOrdNoMap{});
   return this->OrdNoMap_;
}
OmsOrdNoMapSP OmsSessionRec::InitializeOrdNoMapRef(OmsOrdNoMapSP ordNoMap) {
   assert(this->OrdNoMap_.get() == nullptr && ordNoMap.get() != nullptr);
   return this->OrdNoMap_ = std::move(ordNoMap);
}
//--------------------------------------------------------------------------//
void OmsBrkTree::BrksClear(void(OmsBrk::*fnClear)()) {
   for (const OmsBrkSP& brksp : this->BrkRecs_) {
      if (OmsBrk* brk = brksp.get()) {
         (brk->*fnClear)();
      }
   }
}
void OmsBrkTree::InThr_OnParentSeedClear() {
   this->BrksClear(&OmsBrk::OnParentSeedClear);
}
//--------------------------------------------------------------------------//
struct OmsBrkTree::TreeOp : public fon9::seed::TreeOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
   using base = fon9::seed::TreeOp;
   using base::base;

   void GridView(const fon9::seed::GridViewRequest& req, fon9::seed::FnGridViewOp fnCallback) override {
      fon9::seed::GridViewResult res{this->Tree_, req.Tab_};
      res.ContainerSize_ = static_cast<OmsBrkTree*>(&this->Tree_)->BrkRecs_.size();
      size_t istart;
      if (fon9::seed::IsTextBegin(req.OrigKey_))
         istart = 0;
      else if (fon9::seed::IsTextEnd(req.OrigKey_))
         istart = res.ContainerSize_;
      else {
         istart = static_cast<size_t>(static_cast<OmsBrkTree*>(&this->Tree_)->GetBrkIndex(req.OrigKey_));
         if (istart >= res.ContainerSize_) {
            res.OpResult_ = fon9::seed::OpResult::not_found_key;
            fnCallback(res);
            return;
         }
      }
      fon9::seed::MakeGridViewArrayRange(istart, res.ContainerSize_, req, res,
                                         [&res, this](size_t ivalue, fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) {
         if (ivalue >= res.ContainerSize_)
            return false;
         if (auto* brk = static_cast<OmsBrkTree*>(&this->Tree_)->BrkRecs_[ivalue].get())
            brk->MakeGridRow(tab, rbuf);
         return true;
      });
      fnCallback(res);
   }

   void Get(fon9::StrView strKeyText, fon9::seed::FnPodOp fnCallback) override {
      assert(OmsBrkTree::IsInOmsThread(&this->Tree_));
      if (auto* brk = static_cast<OmsBrkTree*>(&this->Tree_)->GetBrkRec(strKeyText))
         brk->OnPodOp(*static_cast<OmsBrkTree*>(&this->Tree_), std::move(fnCallback));
      else
         fnCallback(fon9::seed::PodOpResult{this->Tree_, fon9::seed::OpResult::not_found_key, strKeyText}, nullptr);
   }
};

void OmsBrkTree::InThr_OnTreeOp(fon9::seed::FnTreeOp fnCallback) {
   TreeOp op{*this};
   fnCallback(fon9::seed::TreeOpResult{this, fon9::seed::OpResult::no_error}, &op);
}

} // namespaces
