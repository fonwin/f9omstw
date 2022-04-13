// \file f9omstwf/OmsTwfExgSymbBasic.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfExgSymbBasic_hpp__
#define __f9omstwf_OmsTwfExgSymbBasic_hpp__
#include "f9omstwf/OmsTwfTypes.hpp"
#include "fon9/ConfigUtils.hpp"
#include "fon9/seed/PodOp.hpp"
#include "fon9/fmkt/FmktTools.hpp"

namespace f9omstw {

using ContractId = f9twf::ContractId;

class TwfExgContract;
using TwfExgContractSP = fon9::intrusive_ptr<TwfExgContract>;

class TwfExgContract : public fon9::intrusive_ref_counter<TwfExgContract> {
   fon9_NON_COPY_NON_MOVE(TwfExgContract);
public:
   TwfExgContract(ContractId id);

   /// 設定 this->MainId_, this->MainRef_ 並清除 PsLimit_(Null);
   void SetMainRef(TwfExgContractSP mainContract);

   const ContractId        ContractId_;
   /// 期約乘數 = 每點價值;
   f9twf::ContractSize     ContractSize_{};
   /// 期貨 or 選擇權.
   f9fmkt_TradingMarket    TradingMarket_{};
   /// 現貨標的=股票代號, 其他標的=自訂代號.
   fon9::CharAry<6>        TargetId_{nullptr};
   f9twf::ExgContractType  SubType_{};
   f9twf::ExgExpiryType    ExpiryType_{};
   fon9::EnabledYN         AcceptQuote_{};
   fon9::EnabledYN         CanBlockTrade_{};

   /// LvUpLmt_ == 0 使用 PriLmts_[0];
   /// - 預告: -1 or -2;
   /// - 實施:  1 or  2;
   int8_t                  LvUpLmt_{0};
   int8_t                  LvDnLmt_{0};

   /// 期交所定義的幣別: 1=NTD, 2=USD, 3=EUR, 4=JPY...
   char                    Currency_{};

   /// 結算代碼.
   fon9::CharAry<8>        ClearingId_{nullptr};
   char                    Padding___[6];

   ContractId              MainId_{nullptr};
   TwfExgContractSP        MainRef_;
   /// 返回值不會是 nullptr; 但可能會是 this;
   TwfExgContract* GetMainRef() {
      return this->MainRef_ ? this->MainRef_.get() : this;
   }
   /// 部位上限, IsNull() 表示沒有設定.
   /// 非股票類:
   ///   P13.position_limit * ContractSize_;
   /// 股票類(股數): 計算方式同 P15;
   ///   P13.position_limit * max(全部相同 [Market + TargetId] 的契約.ContractSize_);
   f9twf::ContractSize     PsLimit_[TwfIvacKind_Count];

   /// 禁止下單原因.
   fon9::CharVector        DenyReason_{};
   /// 禁止新倉原因.
   fon9::CharVector        DenyOpen_{};

   /// 在 P09 匯入時設定.
   const fon9::fmkt::LvPriStep*  LvPriSteps_{};
   fon9::CharVector              LvPriStepsStr_;
};
using TwfExgContractSP = fon9::intrusive_ptr<TwfExgContract>;

static inline uint8_t TwfGetLmtLv(int8_t lvContract, int8_t lvSymb) {
   if (fon9_UNLIKELY(lvContract < 0)) // 預告,尚未實施.
      lvContract = static_cast<int8_t>((-lvContract) - 1); // 所以使用前一檔.
   if (fon9_UNLIKELY(lvSymb < 0))
      lvSymb = static_cast<int8_t>((-lvSymb) - 1);
   return fon9::unsigned_cast(std::max(lvContract, lvSymb));
}
//--------------------------------------------------------------------------//
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
   ~ContractTree();

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
