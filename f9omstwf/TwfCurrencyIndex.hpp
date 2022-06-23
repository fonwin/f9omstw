// \file f9omstwf/TwfCurrencyIndex.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_TwfCurrencyIndex_hpp__
#define __f9omstwf_TwfCurrencyIndex_hpp__
#include "f9omstwf/OmsTwfTypes.hpp"
#include "fon9/seed/FileImpTree.hpp"

namespace f9omstw {

using OmsExchangeRateAry = std::array<OmsExchangeRate, CurrencyIndex_Count>;

struct CurrencyConfig {
   CurrencyIndex        CurrencyIndex_{};
   char                 Padding___[7];
   fon9::CharVector     Alias_;
   OmsTwfMrgn           Round_;
   OmsExchangeRateAry   FromCurrencyA_;
   std::array<double, CurrencyIndex_Count>   FromCurrencyF_;

   OmsTwfMrgn Round(OmsTwfMrgn val) const {
      if (this->Round_.IsZero())
         return val;
      if (fon9_LIKELY(val.GetOrigValue() >= 0))
         val += this->Round_ / 2;
      else
         val -= this->Round_ / 2;
      return val - (val % this->Round_);
   }
   void SetExchangeRateFrom(CurrencyIndex cidx, OmsExchangeRate rate) {
      this->FromCurrencyA_[cidx] = rate;
      this->FromCurrencyF_[cidx] = rate.To<double>();
   }

   ~CurrencyConfig();
};
//--------------------------------------------------------------------------//
class CurrencyConfig_Tree : public fon9::seed::Tree {
   fon9_NON_COPY_NON_MOVE(CurrencyConfig_Tree);
   using base = fon9::seed::Tree;
   class PodOp;
   class TreeOp;

   using CurrencyConfigMap = std::array<CurrencyConfig, CurrencyIndex_Count>;
   CurrencyConfigMap  CurrencyConfigMap_;

   // 期交所幣別 => CurrencyIndex;
   CurrencyIndex  TaiFexToIndex_[0x80];

public:
   using AliasId = fon9::CharAry<3>;
   using AliasMapImpl = fon9::SortedVector<AliasId, CurrencyIndex>;
   using AliasMap = fon9::MustLock<AliasMapImpl>;
   using Locker = AliasMap::Locker;
   using ConstLocker = AliasMap::ConstLocker;
   AliasMap AliasMap_;

   CurrencyConfig_Tree();
   ~CurrencyConfig_Tree();

   static fon9::seed::LayoutSP MakeLayout();

   void OnTreeOp(fon9::seed::FnTreeOp fnCallback) override;

   const CurrencyConfig* GetCurrencyConfig(CurrencyIndex idx) const {
      if (idx < this->CurrencyConfigMap_.size())
         return &this->CurrencyConfigMap_[idx];
      return nullptr;
   }
   CurrencyConfig* CurrencyConfigForUpdate(CurrencyIndex idx) {
      if (idx < this->CurrencyConfigMap_.size())
         return &this->CurrencyConfigMap_[idx];
      return nullptr;
   }

   const CurrencyConfig* GetCurrencyConfig(const ConstLocker& lk, const AliasId id) const {
      auto ifind = lk->find(id);
      if (ifind != lk->end())
         return this->GetCurrencyConfig(ifind->second);
      return nullptr;
   }
   const CurrencyConfig* GetCurrencyConfig(const AliasId id) const {
      return this->GetCurrencyConfig(this->AliasMap_.ConstLock(), id);
   }
   void AddCurrencyAlias(const Locker& lk, const AliasId id, CurrencyIndex idx);

   const CurrencyConfig* GetCurrencyConfig(char chTaiFexCurrency) const {
      if (fon9::unsigned_cast(chTaiFexCurrency) < fon9::numofele(this->TaiFexToIndex_))
         return this->GetCurrencyConfig(this->TaiFexToIndex_[fon9::unsigned_cast(chTaiFexCurrency)]);
      return nullptr;
   }
   void SetTaiFexToIndex(char chTaiFexCurrency, CurrencyIndex idx);
};
using CurrencyConfig_TreeSP = fon9::intrusive_ptr<CurrencyConfig_Tree>;
//--------------------------------------------------------------------------//
class TwfExgMapMgr;

class CurrencyConfig_ImpSeed : public fon9::seed::FileImpSeed {
   fon9_NON_COPY_NON_MOVE(CurrencyConfig_ImpSeed);
   using base = fon9::seed::FileImpSeed;
public:
   TwfExgMapMgr&  TwfExgMapMgr_;

   template <class... ArgsT>
   CurrencyConfig_ImpSeed(TwfExgMapMgr& twfExgMapMgr, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , TwfExgMapMgr_(twfExgMapMgr) {
   }
   ~CurrencyConfig_ImpSeed();

   struct Loader;
   fon9::seed::FileImpLoaderSP OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) override;
   void OnAfterLoad(fon9::RevBuffer& rbufDesp, fon9::seed::FileImpLoaderSP loader, fon9::seed::FileImpMonitorFlag monFlag) override;
};


} // namespaces
#endif//__f9omstwf_TwfCurrencyIndex_hpp__
