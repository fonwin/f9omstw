// \file f9omstwf/OmsTwfExgSymbBasic.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfExgSymbBasic_hpp__
#define __f9omstwf_OmsTwfExgSymbBasic_hpp__
#include "f9twf/ExgTypes.hpp"
#include "fon9/ConfigUtils.hpp"
#include "fon9/TimeStamp.hpp"
#include "fon9/seed/PodOp.hpp"

namespace f9omstw {

using ContractId = f9twf::ContractId;

class TwfExgContract : public fon9::intrusive_ref_counter<TwfExgContract> {
   fon9_NON_COPY_NON_MOVE(TwfExgContract);
public:
   TwfExgContract(ContractId id) : ContractId_{id} {
   }

   const ContractId        ContractId_;
   /// 現貨標的.
   f9twf::StkNo            StkNo_{nullptr};
   f9twf::ExgContractType  SubType_{};
   f9twf::ExgExpiryType    ExpiryType_{};
   f9twf::ContractSize     ContractSize_{};
   /// 禁止下單原因.
   fon9::CharVector        DenyReason_{};
   fon9::EnabledYN         AcceptQuote_{};
   fon9::EnabledYN         CanBlockTrade_{};
   char                    Padding___[6];
};
using TwfExgContractSP = fon9::intrusive_ptr<TwfExgContract>;

/// 這裡提供一個 Twf 商品基本資料.
/// TwfExgMapMgr 會將 P08 的內容填入此處.
class TwfExgSymbBasic {
public:
   /// 期交所PA8的商品Id;
   /// this->SymbId_ = P08的商品Id;
   /// P08/PA8 共用同一個 UtwsSymb;
   fon9::CharVector  LongId_;
   fon9::TimeStamp   P08UpdatedTime_;

   /// 在 TwfExgMapMgr::OnP08Updated() 會設定此值.
   TwfExgContractSP  Contract_;
};

//--------------------------------------------------------------------------//

class ContractTree : public fon9::seed::Tree {
   fon9_NON_COPY_NON_MOVE(ContractTree);
   using base = fon9::seed::Tree;
   class PodOp;
   class TreeOp;

public:
   using ContractMapImpl = fon9::SortedVector<ContractId, TwfExgContractSP>;
   using iterator = typename ContractMapImpl::iterator;
   using ContractMap = fon9::MustLock<ContractMapImpl, std::mutex>;
   using Locker = typename ContractMap::Locker;
   ContractMap  ContractMap_;

   ContractTree() : base{MakeLayout()} {
   }

   static fon9::seed::LayoutSP MakeLayout();

   void OnTreeOp(fon9::seed::FnTreeOp fnCallback) override;
   void OnParentSeedClear() override;

   static TwfExgContractSP FetchContract(const Locker& contracts, const ContractId contractId) {
      auto& val = contracts->kfetch(contractId);
      if (!val.second)
         val.second.reset(new TwfExgContract{contractId});
      return val.second;
   }
   TwfExgContractSP FetchContract(const ContractId contractId) {
      return this->FetchContract(this->ContractMap_.Lock(), contractId);
   }

   static TwfExgContractSP GetContract(const Locker& contracts, const ContractId contractId) {
      auto ifind = contracts->find(contractId);
      return ifind == contracts->end() ? TwfExgContractSP{nullptr} : ifind->second;
   }
   TwfExgContractSP GetContract(const ContractId contractId) {
      return this->GetContract(this->ContractMap_.Lock(), contractId);
   }
};

} // namespaces
#endif//__f9omstwf_OmsTwfExgSymbBasic_hpp__
