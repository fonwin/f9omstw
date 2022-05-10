// \file f9omstwf/TwfIvacScRule.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_TwfIvacScRule_hpp__
#define __f9omstwf_TwfIvacScRule_hpp__
#include "f9omstw/OmsImporter.hpp"
#include "f9omstwf/OmsTwfTypes.hpp"

namespace f9omstw {

enum class CurrencyRule : char {
   /// 境外交易人: 只要該約當幣別夠即可下單.
   Foreign = 'F',
   /// 其餘非境外(國內)交易人: 需與期貨商 [約定] 幣別如何互換。
};

struct TwfIvacScRule {
   f9twf::TmpIvacFlag   IvacNoFlag_{};
   TwfIvacKind          TwfIvacKind_{};
   /// 部位上限不控管(例: 避險帳戶);
   fon9::EnabledYN      PsUnlimit_{};
   /// 是否需要收取 [混合部位風險保證金], 底下身分需要:
   /// 自然人: 1,3,7,I,J,U,V;
   /// 一般法人: 0, W;
   fon9::EnabledYN      RiskC_{};
   CurrencyRule         CurrencyRule_;
   char                 Padding___[3];
};
//--------------------------------------------------------------------------//
class TwfIvacScRule_Tree : public fon9::seed::Tree {
   fon9_NON_COPY_NON_MOVE(TwfIvacScRule_Tree);
   using base = fon9::seed::Tree;
   class PodOp;
   class TreeOp;

   struct Cmp {
      bool operator()(char lhs, char rhs) const { return lhs < rhs; }
      bool operator()(char lhs, const fon9::StrView& rhs) const { return lhs < rhs.Get1st(); }
   };
   using TwfIvacScRuleMap = fon9::SortedVector<char, TwfIvacScRule, Cmp>;
   TwfIvacScRuleMap  TwfIvacScRuleMap_;

public:
   TwfIvacScRule_Tree();
   ~TwfIvacScRule_Tree();

   static fon9::seed::LayoutSP MakeLayout();

   void OnTreeOp(fon9::seed::FnTreeOp fnCallback) override;

   bool GetTwfIvacScRule(char ivacNoFlag, TwfIvacScRule& res) const {
      auto ifind = this->TwfIvacScRuleMap_.find(ivacNoFlag);
      if (ifind == this->TwfIvacScRuleMap_.end())
         return false;
      res = ifind->second;
      return true;
   }
   bool UpdateIvacScRule(const TwfIvacScRule& res) {
      auto ifind = this->TwfIvacScRuleMap_.find(res.IvacNoFlag_.Chars_[0]);
      if (ifind == this->TwfIvacScRuleMap_.end())
         return false;
      ifind->second = res;
      return true;
   }
};
using TwfIvacScRule_TreeSP = fon9::intrusive_ptr<TwfIvacScRule_Tree>;
//--------------------------------------------------------------------------//
class TwfExgMapMgr;

class TwfIvacScRule_ImpSeed : public OmsFileImpSeed {
   fon9_NON_COPY_NON_MOVE(TwfIvacScRule_ImpSeed);
   using base = OmsFileImpSeed;
public:
   TwfExgMapMgr&              TwfExgMapMgr_;
   const TwfIvacScRule_TreeSP Sapling_;

   template <class... ArgsT>
   TwfIvacScRule_ImpSeed(TwfExgMapMgr& twfExgMapMgr, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , TwfExgMapMgr_(twfExgMapMgr)
      , Sapling_{new TwfIvacScRule_Tree} {
   }
   ~TwfIvacScRule_ImpSeed();

   fon9::seed::TreeSP GetSapling() override;

   struct Loader;
   OmsFileImpLoaderSP MakeLoader(OmsFileImpTree& owner, fon9::RevBuffer& rbufDesp, uint64_t addSize, FileImpMonitorFlag monFlag) override;
   void OnAfterLoad(fon9::RevBuffer& rbufDesp, fon9::seed::FileImpLoaderSP loader, fon9::seed::FileImpMonitorFlag monFlag) override;
};

} // namespaces
#endif//__f9omstwf_TwfIvacScRule_hpp__
