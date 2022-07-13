// \file f9omstwf/OmsTwfExgSymbBasic.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfExgSymbBasic_hpp__
#define __f9omstwf_OmsTwfExgSymbBasic_hpp__
#include "f9omstwf/OmsTwfTypes.hpp"
#include "f9omstw/OmsMdEvent.hpp"
#include "fon9/ConfigUtils.hpp"
#include "fon9/seed/PodOp.hpp"
#include "fon9/fmkt/FmktTools.hpp"

namespace f9omstw {

class TwfContractBase : public fon9::intrusive_ref_counter<TwfContractBase> {
   fon9_NON_COPY_NON_MOVE(TwfContractBase);
public:
   const OmsTwfContractId  ContractId_;

   TwfContractBase(OmsTwfContractId id) : ContractId_{id} {
   }
   virtual ~TwfContractBase();
};
using TwfContractBaseSP = fon9::intrusive_ptr<TwfContractBase>;
//--------------------------------------------------------------------------//
class TwfContractBaseTree : public fon9::seed::Tree {
   fon9_NON_COPY_NON_MOVE(TwfContractBaseTree);
   using base = fon9::seed::Tree;
   class PodOp;
   class TreeOp;

public:
   using ContractMapImpl = fon9::SortedVector<OmsTwfContractId, TwfContractBaseSP>;
   using ContractMap = fon9::MustLock<ContractMapImpl, std::mutex>;
   using Locker = typename ContractMap::Locker;
   ContractMap& ContractMap_;

   template <class... ArgsT>
   TwfContractBaseTree(ContractMap& contracts, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , ContractMap_(contracts) {
   }
   ~TwfContractBaseTree();

   void OnTreeOp(fon9::seed::FnTreeOp fnCallback) override;
   void OnParentSeedClear() override;

   virtual TwfContractBaseSP FetchContractBase(const Locker& contracts, const OmsTwfContractId contractId) = 0;

   static TwfContractBaseSP GetContractBase(const Locker& contracts, const OmsTwfContractId contractId) {
      auto ifind = contracts->find(contractId);
      return ifind == contracts->end() ? nullptr : ifind->second;
   }
};

template <class ContractT>
class TwfContractMap {
   fon9_NON_COPY_NON_MOVE(TwfContractMap);
public:
   class ContractTree : public TwfContractBaseTree {
      fon9_NON_COPY_NON_MOVE(ContractTree);
      using base = TwfContractBaseTree;
   public:
      template <class... ArgsT>
      ContractTree(TwfContractMap& map, ArgsT&&... args)
         : base(*reinterpret_cast<ContractMap*>(&map.Contracts_), std::forward<ArgsT>(args)...) {
      }
      ContractTree(TwfContractMap& map) : base(*reinterpret_cast<ContractMap*>(&map.Contracts_), ContractT::MakeLayout()) {
      }
      TwfContractBaseSP FetchContractBase(const Locker& contracts, const OmsTwfContractId contractId) override {
         return FetchContract(contracts, contractId);
      }
   };
   class ContainsMapTree : public ContractTree {
      fon9_NON_COPY_NON_MOVE(ContainsMapTree);
      using base = ContractTree;
   public:
      TwfContractMap ContractMap_;
      template <class... ArgsT>
      ContainsMapTree(ArgsT&&... args) : base(ContractMap_, std::forward<ArgsT>(args)...) {
      }
   };


   using ContractSP = fon9::intrusive_ptr<ContractT>;
   using ContractMapImpl = fon9::SortedVector<OmsTwfContractId, ContractSP>;
   using ContractMap = fon9::MustLock<ContractMapImpl, std::mutex>;
   using Locker = typename ContractMap::Locker;
   ContractMap Contracts_;

   TwfContractMap() = default;

   template <class... ArgsT>
   fon9::intrusive_ptr<ContractTree> MakeTree(ArgsT&&... args) {
      return new ContractTree{*this, std::forward<ArgsT>(args)...};
   }

   template <class Locker>
   static ContractSP FetchContract(const Locker& contracts, const OmsTwfContractId contractId) {
      auto& val = contracts->kfetch(contractId);
      if (!val.second)
         val.second.reset(new ContractT{contractId});
      return fon9::static_pointer_cast<ContractT>(val.second);
   }

   Locker Lock() {
      return this->Contracts_.Lock();
   }
   ContractSP FetchContract(const OmsTwfContractId contractId) {
      return this->FetchContract(this->Contracts_.Lock(), contractId);
   }
   static ContractSP GetContract(const Locker& contracts, const OmsTwfContractId contractId) {
      auto ifind = contracts->find(contractId);
      return ifind == contracts->end() ? nullptr : fon9::static_pointer_cast<ContractT>(ifind->second);
   }
   ContractSP GetContract(const OmsTwfContractId contractId) {
      return this->GetContract(this->Contracts_.Lock(), contractId);
   }
};
//--------------------------------------------------------------------------//
class TwfExgContract;
using TwfExgContractSP = fon9::intrusive_ptr<TwfExgContract>;

struct RiskV {
   OmsTwfMrgn        RiskA_;
   OmsTwfMrgn        RiskB_;
   OmsTwfMrgn        RiskC_;
   fon9::EnabledYN   RateA_{};
   fon9::EnabledYN   RateB_{};
   fon9::EnabledYN   RateC_{};
   char              Padding____[5];
};

class TwfExgContract : public TwfContractBase {
   fon9_NON_COPY_NON_MOVE(TwfExgContract);
   using base = TwfContractBase;
public:
   using base::base;
   ~TwfExgContract();

   static fon9::seed::LayoutSP MakeLayout();

   /// 設定 this->MainId_, this->MainRef_ 並清除 PsLimit_(Null);
   void SetMainRef(TwfExgContractSP mainContract);

   /// 結算代碼.
   fon9::CharAry<8>        ClearingId_{nullptr};
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
   CurrencyIndex           CurrencyIndex_{CurrencyIndex_Unsupport};
   /// 股票類商品, 每張的股數.
   uint16_t                StkShUnit_{};
   /// 價格小數位數.
   uint8_t                 PriDecLoc_{};
   /// 履約價小數位數.
   uint8_t                 SpDecLoc_{};

   /// LvUpLmt_ == 0 使用 PriLmts_[0];
   /// - 預告: -1 or -2;
   /// - 實施:  1 or  2;
   int8_t                  LvUpLmt_{0};
   int8_t                  LvDnLmt_{0};

   char                    Padding____[2];
   /// 選擇權參照的 [標的期貨].
   OmsTwfContractId        TargetFutId_{nullptr};
   TwfExgContractSP        TargetFut_;

   /// 原始保證金風險值。
   RiskV RiskIni_;
   /// P14沒提供[週商品], 應自行建立參照.
   /// 期貨的 RiskIni_.xxxxC = 結算保證金. [選擇權時間價差] 計算保證金會用到: [標的期貨的結算保證金]。
   OmsTwfMrgn      GetFutClearingMarginRisk() const { return this->RiskIni_.RiskC_; }
   fon9::EnabledYN GetFutClearingMarginRate() const { return this->RiskIni_.RateC_; }

   /// 標的(契約)參考價,  初值來自 C11, 後續來自行情 I060;
   OmsTwfPri               LastPrice_;
   /// 交易稅率.
   OmsTaxRate              TaxRate_;

   OmsTwfContractId        MainId_{nullptr};
   TwfExgContractSP        MainRef_;
   /// 返回值不會是 nullptr; 但可能會是 this;
   TwfExgContract* GetMainRef() {
      return this->MainRef_ ? this->MainRef_.get() : this;
   }
   /// 部位上限, IsZero()=禁止下單(P13沒設定), IsNull()=表示 P13 設定的是無上限.
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
static inline uint8_t TwfGetLmtLv(int8_t lvContract, int8_t lvSymb) {
   if (fon9_UNLIKELY(lvContract < 0)) // 預告,尚未實施.
      lvContract = static_cast<int8_t>((-lvContract) - 1); // 所以使用前一檔.
   if (fon9_UNLIKELY(lvSymb < 0))
      lvSymb = static_cast<int8_t>((-lvSymb) - 1);
   return fon9::unsigned_cast(std::max(lvContract, lvSymb));
}

using TwfExgContractMap = TwfContractMap<TwfExgContract>;
using TwfExgContractTree = TwfExgContractMap::ContainsMapTree;
//--------------------------------------------------------------------------//
/// TwfExgMapMgr 會將 P08 的內容填入此處.
/// OmsMdLastPrice.LastPrice_: 商品最後成交價 = 初始值來自P08.premium, 後續用即時行情I020;
class TwfExgSymbBasic : public OmsMdLastPriceEv {
   fon9_NON_COPY_NON_MOVE(TwfExgSymbBasic);
public:
   TwfExgSymbBasic();
   ~TwfExgSymbBasic();

   /// 期交所PA8的商品Id;
   /// this->SymbId_ = P08的商品Id;
   /// P08/PA8 共用同一個 UtwsSymb;
   fon9::CharVector  LongId_;
   fon9::TimeStamp   P08UpdatedTime_;

   /// 在 TwfExgMapMgr::OnP08Updated() 會設定此值.
   TwfExgContractSP  Contract_;
};

} // namespaces
#endif//__f9omstwf_OmsTwfExgSymbBasic_hpp__
