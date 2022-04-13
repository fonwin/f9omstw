// \file f9omstwf/TwfIvacScRule.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/TwfIvacScRule.hpp"
#include "fon9/seed/RawRd.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {
using namespace fon9;
using namespace fon9::seed;

class TwfIvacScRule_Tree::PodOp : public PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = PodOpDefault;
public:
   const TwfIvacScRule  IvacScRule_;
   PodOp(Tree& sender, const StrView& keyText, TwfIvacScRule ivacScRule)
      : base(sender, OpResult::no_error, keyText)
      , IvacScRule_(ivacScRule) {
   }
   void BeginRead(Tab& tab, FnReadOp fnCallback) override {
      this->BeginRW(tab, std::move(fnCallback), SimpleRawRd{this->IvacScRule_});
   }
};
class TwfIvacScRule_Tree::TreeOp : public fon9::seed::TreeOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
   using base = fon9::seed::TreeOp;
public:
   TreeOp(TwfIvacScRule_Tree& tree) : base(tree) {
   }
   static void MakeRowView(TwfIvacScRuleMap::const_iterator ivalue, Tab* tab, RevBuffer& rbuf) {
      if (tab)
         FieldsCellRevPrint(tab->Fields_, SimpleRawRd{ivalue->second}, rbuf, GridViewResult::kCellSplitter);
      RevPrint(rbuf, ivalue->first);
   }
   void GridView(const GridViewRequest& req, FnGridViewOp fnCallback) override {
      GridViewResult res{this->Tree_, req.Tab_};
      const auto&    map = static_cast<TwfIvacScRule_Tree*>(&this->Tree_)->TwfIvacScRuleMap_;
      MakeGridView(map, GetIteratorForGv(map, req.OrigKey_), req, res, &MakeRowView);
      fnCallback(res);
   }

   void Get(StrView strKeyText, FnPodOp fnCallback) override {
      TwfIvacScRule ivacScRule;
      if (static_cast<TwfIvacScRule_Tree*>(&this->Tree_)->GetTwfIvacScRule(static_cast<char>(strKeyText.Get1st()), ivacScRule)) {
         PodOp op(this->Tree_, strKeyText, ivacScRule);
         fnCallback(op, &op);
      }
      else {
         fnCallback(PodOpResult{this->Tree_, OpResult::not_found_key, strKeyText}, nullptr);
      }
   }
};
//--------------------------------------------------------------------------//
TwfIvacScRule_Tree::TwfIvacScRule_Tree() : base{MakeLayout()} {
   this->TwfIvacScRuleMap_.reserve(fon9::kSeq2AlphaSize);
   for (uint8_t L = 0; L < fon9::kSeq2AlphaSize; ++L) {
      auto& val = this->TwfIvacScRuleMap_.kfetch(Seq2Alpha(L));
      val.second.IvacNoFlag_.Chars_[0] = val.first;
   }
}
TwfIvacScRule_Tree::~TwfIvacScRule_Tree() {
}
LayoutSP TwfIvacScRule_Tree::MakeLayout() {
   Fields fields;
   fields.Add(fon9_MakeField2_const(TwfIvacScRule, TwfIvacKind));
   fields.Add(fon9_MakeField2_const(TwfIvacScRule, PsUnlimit));
   return new Layout1(fon9_MakeField2(TwfIvacScRule, IvacNoFlag),
                      new Tab(Named{"Base"}, std::move(fields), TabFlag::NoSapling | TabFlag::NoSeedCommand));
}
void TwfIvacScRule_Tree::OnTreeOp(FnTreeOp fnCallback) {
   TreeOp op{*this};
   fnCallback(TreeOpResult{this, OpResult::no_error}, &op);
}
//--------------------------------------------------------------------------//
struct TwfIvacScRule_ImpSeed::Loader : public OmsFileImpLoader {
   fon9_NON_COPY_NON_MOVE(Loader);
   using base = OmsFileImpLoader;
   TwfIvacScRule_Tree& Sapling_;
   Loader(TwfIvacScRule_Tree& sapling, const OmsFileImpTree& owner) : base{owner}, Sapling_(sapling) {
   }
   using base::base;
   void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override {
      (void)isEOF;
      fon9::StrView ln{pbuf, bufsz};
      fon9::StrView ivacNoFlag = fon9::StrTrimSplit(ln, '=');
      if (!fon9::isalnum(ivacNoFlag.Get1st()))
         return;
      TwfIvacScRule ivacScRule;
      ivacScRule.IvacNoFlag_.Chars_[0] = static_cast<char>(ivacNoFlag.Get1st());
      fon9::StrView val = fon9::StrTrimSplit(ln, ',');
      if ((ivacScRule.TwfIvacKind_ = static_cast<TwfIvacKind>(val.Get1st() - '0')) >= TwfIvacKind_Count)
         ivacScRule.TwfIvacKind_ = TwfIvacKind_Natural;
      val = fon9::StrTrimSplit(ln, ',');
      ivacScRule.PsUnlimit_ = (val.Get1st() == 'Y' ? fon9::EnabledYN::Yes : fon9::EnabledYN{});
      this->Sapling_.UpdateIvacScRule(ivacScRule);
   }
};
OmsFileImpLoaderSP TwfIvacScRule_ImpSeed::MakeLoader(OmsFileImpTree& owner, fon9::RevBuffer& rbufDesp, uint64_t addSize, FileImpMonitorFlag monFlag) {
   (void)rbufDesp; (void)addSize; (void)monFlag;
   return new Loader{*this->Sapling_, owner};
}
void TwfIvacScRule_ImpSeed::OnAfterLoad(fon9::RevBuffer& rbufDesp, fon9::seed::FileImpLoaderSP loader, fon9::seed::FileImpMonitorFlag monFlag) {
   (void)rbufDesp;
   auto* cfront = MakeOmsImpLog(*this, monFlag, loader->LnCount_);
   this->TwfExgMapMgr_.CoreLogAppend(fon9::RevBufferList{128, fon9::BufferList{cfront}});
}
TreeSP TwfIvacScRule_ImpSeed::GetSapling() {
   return this->Sapling_;
}

} // namespaces
