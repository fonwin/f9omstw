// \file f9omstwf/OmsTwfExgSymbBasic.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfExgSymbBasic.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {
using namespace fon9;
using namespace fon9::seed;

TwfContractBase::~TwfContractBase() {
}
//--------------------------------------------------------------------------//
TwfExgContract::~TwfExgContract() {
}
void TwfExgContract::SetMainRef(TwfExgContractSP mainContract) {
   assert(mainContract.get() != nullptr && mainContract.get() != this);
   this->MainId_ = mainContract->ContractId_;
   this->MainRef_ = std::move(mainContract);
   assert(fon9::numofele(this->PsLimit_) == 4);
   this->PsLimit_[0] = this->PsLimit_[1] = this->PsLimit_[2] = this->PsLimit_[3] = f9twf::ContractSize::Null();
}
LayoutSP TwfExgContract::MakeLayout() {
   Fields fields;
   fields.Add(fon9_MakeField (TwfExgContract, TradingMarket_, "Market"));
   fields.Add(fon9_MakeField2(TwfExgContract, TargetId));
   fields.Add(fon9_MakeField2(TwfExgContract, StkShUnit));
   fields.Add(fon9_MakeField2(TwfExgContract, SubType));
   fields.Add(fon9_MakeField2(TwfExgContract, ExpiryType));
   fields.Add(fon9_MakeField2(TwfExgContract, ContractSize));
   fields.Add(fon9_MakeField2(TwfExgContract, TaxRate));
   fields.Add(fon9_MakeField2(TwfExgContract, LastPrice));
   fields.Add(fon9_MakeField2(TwfExgContract, PriDecLoc));
   fields.Add(fon9_MakeField2(TwfExgContract, SpDecLoc));
   fields.Add(fon9_MakeField2(TwfExgContract, DenyReason));
   fields.Add(fon9_MakeField2(TwfExgContract, DenyOpen));
   fields.Add(fon9_MakeField2(TwfExgContract, AcceptQuote));
   fields.Add(fon9_MakeField2(TwfExgContract, CanBlockTrade));
   fields.Add(fon9_MakeField2(TwfExgContract, CurrencyIndex));
   fields.Add(fon9_MakeField2_const(TwfExgContract, ClearingId));
   fields.Add(fon9_MakeField2_const(TwfExgContract, MainId));
   fields.Add(fon9_MakeField2_const(TwfExgContract, TargetFutId));

   fields.Add(fon9_MakeField (TwfExgContract, PsLimit_[0], "PsLimit0"));
   fields.Add(fon9_MakeField (TwfExgContract, PsLimit_[1], "PsLimit1"));
   fields.Add(fon9_MakeField (TwfExgContract, PsLimit_[2], "PsLimit2"));
   fields.Add(fon9_MakeField (TwfExgContract, PsLimit_[3], "PsLimit3"));
   assert(TwfIvacKind_Count == 4);

   fields.Add(fon9_MakeField (TwfExgContract, RiskIni_.RiskA_, "IRiskA"));
   fields.Add(fon9_MakeField (TwfExgContract, RiskIni_.RateA_, "IRateA"));
   fields.Add(fon9_MakeField (TwfExgContract, RiskIni_.RiskB_, "IRiskB"));
   fields.Add(fon9_MakeField (TwfExgContract, RiskIni_.RateB_, "IRateB"));
   fields.Add(fon9_MakeField (TwfExgContract, RiskIni_.RiskC_, "IRiskC"));
   fields.Add(fon9_MakeField (TwfExgContract, RiskIni_.RateC_, "IRateC"));

   fields.Add(fon9_MakeField2(TwfExgContract, LvUpLmt));
   fields.Add(fon9_MakeField2(TwfExgContract, LvDnLmt));
   fields.Add(fon9_MakeField_const(TwfExgContract, LvPriStepsStr_, "TickSize"));
   return new Layout1(fon9_MakeField(TwfExgContract, ContractId_, "Id"),
                      new Tab(Named{"Base"}, std::move(fields), TabFlag::NoSapling_NoSeedCommand_Writable));
}
//--------------------------------------------------------------------------//
class TwfContractBaseTree::PodOp : public PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = PodOpDefault;
public:
   const TwfContractBaseSP  Contract_;
   PodOp(Tree& sender, const StrView& keyText, TwfContractBaseSP&& contract, const Locker& lockedMap)
      : base(sender, OpResult::no_error, keyText)
      , Contract_{std::move(contract)} {
      (void)lockedMap; assert(lockedMap.owns_lock());
   }
   void BeginRead(Tab& tab, FnReadOp fnCallback) override {
      this->BeginRW(tab, std::move(fnCallback), SimpleRawRd{*this->Contract_});
   }
   void BeginWrite(Tab& tab, FnWriteOp fnCallback) override {
      this->BeginRW(tab, std::move(fnCallback), SimpleRawWr{*this->Contract_});
   }
};
class TwfContractBaseTree::TreeOp : public fon9::seed::TreeOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
   using base = fon9::seed::TreeOp;
public:
   TreeOp(TwfContractBaseTree& tree) : base(tree) {
   }

   static void MakeRowView(ContractMapImpl::const_iterator ivalue, Tab* tab, RevBuffer& rbuf) {
      if (tab)
         FieldsCellRevPrint(tab->Fields_, SimpleRawRd{*ivalue->second}, rbuf, GridViewResult::kCellSplitter);
      RevPrint(rbuf, ivalue->first);
   }
   void GridView(const GridViewRequest& req, FnGridViewOp fnCallback) override {
      GridViewResult res{this->Tree_, req.Tab_};
      {
         auto  lockedMap{static_cast<TwfContractBaseTree*>(&this->Tree_)->ContractMap_.ConstLock()};
         MakeGridView(*lockedMap, GetIteratorForGv(*lockedMap, req.OrigKey_), req, res, &MakeRowView);
      } // unlock map.
      fnCallback(res);
   }

   void OnPodOp(const StrView& strKeyText, TwfContractBaseSP&& contract, FnPodOp&& fnCallback, const Locker& lockedMap) {
      if (!contract)
         fnCallback(PodOpResult{this->Tree_, OpResult::not_found_key, strKeyText}, nullptr);
      else {
         PodOp op(this->Tree_, strKeyText, std::move(contract), lockedMap);
         fnCallback(op, &op);
      }
   }
   void Get(StrView strKeyText, FnPodOp fnCallback) override {
      Locker lockedMap{static_cast<TwfContractBaseTree*>(&this->Tree_)->ContractMap_};
      TwfContractBaseSP contract = static_cast<TwfContractBaseTree*>(&this->Tree_)->GetContractBase(lockedMap, strKeyText);
      this->OnPodOp(strKeyText, std::move(contract), std::move(fnCallback), lockedMap);
   }
   void Add(StrView strKeyText, FnPodOp fnCallback) override {
      if (IsTextBeginOrEnd(strKeyText))
         fnCallback(PodOpResult{this->Tree_, OpResult::not_found_key, strKeyText}, nullptr);
      else {
         Locker lockedMap{static_cast<TwfContractBaseTree*>(&this->Tree_)->ContractMap_};
         TwfContractBaseSP contract = static_cast<TwfContractBaseTree*>(&this->Tree_)->FetchContractBase(lockedMap, strKeyText);
         this->OnPodOp(strKeyText, std::move(contract), std::move(fnCallback), lockedMap);
      }
   }
   void Remove(StrView strKeyText, Tab* tab, FnPodRemoved fnCallback) override {
      PodRemoveResult res{this->Tree_, OpResult::not_found_key, strKeyText, tab};
      {
         Locker lockedMap{static_cast<TwfContractBaseTree*>(&this->Tree_)->ContractMap_};
         auto   ifind = lockedMap->find(strKeyText);
         if (ifind != lockedMap->end()) {
            lockedMap->erase(ifind);
            res.OpResult_ = OpResult::removed_pod;
         }
      }
      fnCallback(res);
   }
};
//--------------------------------------------------------------------------//
TwfContractBaseTree::~TwfContractBaseTree() {
}
void TwfContractBaseTree::OnTreeOp(FnTreeOp fnCallback) {
   TreeOp op{*this};
   fnCallback(TreeOpResult{this, OpResult::no_error}, &op);
}
void TwfContractBaseTree::OnParentSeedClear() {
   ContractMapImpl impl{std::move(*this->ContractMap_.Lock())};
   // unlock 後, impl 解構時, 自動清除.
}
//--------------------------------------------------------------------------//
TwfExgSymbBasic::TwfExgSymbBasic() {
}
TwfExgSymbBasic::~TwfExgSymbBasic() {
}

} // namespaces
