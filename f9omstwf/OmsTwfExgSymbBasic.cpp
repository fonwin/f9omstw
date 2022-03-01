// \file f9omstwf/OmsTwfExgSymbBasic.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfExgSymbBasic.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {
using namespace fon9;
using namespace fon9::seed;

class ContractTree::PodOp : public PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = PodOpDefault;
public:
   const TwfExgContractSP  Contract_;
   PodOp(Tree& sender, const StrView& keyText, TwfExgContractSP&& contract, const Locker& lockedMap)
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
class ContractTree::TreeOp : public fon9::seed::TreeOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
   using base = fon9::seed::TreeOp;
public:
   TreeOp(ContractTree& tree) : base(tree) {
   }

   static void MakeRowView(ContractMapImpl::const_iterator ivalue, Tab* tab, RevBuffer& rbuf) {
      if (tab)
         FieldsCellRevPrint(tab->Fields_, SimpleRawRd{*ivalue->second}, rbuf, GridViewResult::kCellSplitter);
      RevPrint(rbuf, ivalue->first);
   }
   void GridView(const GridViewRequest& req, FnGridViewOp fnCallback) override {
      GridViewResult res{this->Tree_, req.Tab_};
      {
         auto  lockedMap{static_cast<ContractTree*>(&this->Tree_)->ContractMap_.ConstLock()};
         MakeGridView(*lockedMap, GetIteratorForGv(*lockedMap, req.OrigKey_), req, res, &MakeRowView);
      } // unlock map.
      fnCallback(res);
   }

   void OnPodOp(const StrView& strKeyText, TwfExgContractSP&& contract, FnPodOp&& fnCallback, const Locker& lockedMap) {
      if (!contract)
         fnCallback(PodOpResult{this->Tree_, OpResult::not_found_key, strKeyText}, nullptr);
      else {
         PodOp op(this->Tree_, strKeyText, std::move(contract), lockedMap);
         fnCallback(op, &op);
      }
   }
   void Get(StrView strKeyText, FnPodOp fnCallback) override {
      Locker lockedMap{static_cast<ContractTree*>(&this->Tree_)->ContractMap_};
      TwfExgContractSP contract = static_cast<ContractTree*>(&this->Tree_)->GetContract(lockedMap, strKeyText);
      this->OnPodOp(strKeyText, std::move(contract), std::move(fnCallback), lockedMap);
   }
   void Add(StrView strKeyText, FnPodOp fnCallback) override {
      if (IsTextBeginOrEnd(strKeyText))
         fnCallback(PodOpResult{this->Tree_, OpResult::not_found_key, strKeyText}, nullptr);
      else {
         Locker lockedMap{static_cast<ContractTree*>(&this->Tree_)->ContractMap_};
         TwfExgContractSP contract = static_cast<ContractTree*>(&this->Tree_)->FetchContract(lockedMap, strKeyText);
         this->OnPodOp(strKeyText, std::move(contract), std::move(fnCallback), lockedMap);
      }
   }
   void Remove(StrView strKeyText, Tab* tab, FnPodRemoved fnCallback) override {
      PodRemoveResult res{this->Tree_, OpResult::not_found_key, strKeyText, tab};
      {
         Locker lockedMap{static_cast<ContractTree*>(&this->Tree_)->ContractMap_};
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
LayoutSP ContractTree::MakeLayout() {
   Fields fields;
   fields.Add(fon9_MakeField (TwfExgContract, StkNo_.Chars_, "StkNo"));
   fields.Add(fon9_MakeField2(TwfExgContract, SubType));
   fields.Add(fon9_MakeField2(TwfExgContract, ExpiryType));
   fields.Add(fon9_MakeField2(TwfExgContract, ContractSize));
   fields.Add(fon9_MakeField2(TwfExgContract, DenyReason));
   fields.Add(fon9_MakeField2(TwfExgContract, AcceptQuote));
   fields.Add(fon9_MakeField2(TwfExgContract, CanBlockTrade));
   return new Layout1(fon9_MakeField(TwfExgContract, ContractId_.Chars_, "Id"),
                      new Tab(Named{"Base"}, std::move(fields), TabFlag::NoSapling_NoSeedCommand_Writable));
}
void ContractTree::OnTreeOp(FnTreeOp fnCallback) {
   TreeOp op{*this};
   fnCallback(TreeOpResult{this, OpResult::no_error}, &op);
}
void ContractTree::OnParentSeedClear() {
   ContractMapImpl impl{std::move(*this->ContractMap_.Lock())};
   // unlock 後, impl 解構時, 自動清除.
}

} // namespaces
