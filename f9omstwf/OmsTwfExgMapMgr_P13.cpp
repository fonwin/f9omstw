// \file f9omstwf/OmsTwfExgMapMgr_P13.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfExgMapMgr.hpp"
#include "f9omstwf/OmsTwfTypes.hpp"
#include "f9omstw/OmsImporter.hpp"
#include "f9twf/TwfSymbRef.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9omstw {

struct ImpSeedP13 : public TwfExgMapMgr::ImpSeedForceLoadSesNormal {
   fon9_NON_COPY_NON_MOVE(ImpSeedP13);
   using base = TwfExgMapMgr::ImpSeedForceLoadSesNormal;

   template <class... ArgsT>
   ImpSeedP13(ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...) {
      this->SetDefaultSchCfg();
   }

   struct P13Rec {
      OmsTwfContractId  kind_id_;
      char              investor_flag_;         // = IvacKind(0:自然人, 1:法人, 2:期貨自營商, 3:造市者).
      fon9::CharAry<8>  position_limit_;        // 各月份合計部位限制.
      fon9::CharAry<8>  position_limit_month_;  // 單一月份部位限制.     99999999=無限制;
      fon9::CharAry<8>  position_limit_last_;   // 最近到期月份部位限制. 99999999=無限制;
   };
   struct ItemRec {
      OmsTwfContractId  ContractId_;
      char              Padding____[4];
      uint32_t          PosLimit_[TwfIvacKind_Count];
      ItemRec() {
         fon9::ForceZeroNonTrivial(this);
      }
      bool operator<(const ItemRec& rhs) const {
         return this->ContractId_ < rhs.ContractId_;
      }
   };
   using ItemRecs = fon9::SortedVectorSet<ItemRec>;

   struct Loader : public fon9::seed::FileImpLoader {
      fon9_NON_COPY_NON_MOVE(Loader);
      ImpSeedP13& Owner_;
      ItemRecs    Items_;
      ItemRec     KeyItem_;

      Loader(ImpSeedP13& owner, size_t itemCount)
         : Owner_{owner} {
         this->Items_.reserve(itemCount);
      }
      ~Loader() {
      }
      size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override {
         return this->ParseBlock(pbuf, bufsz, isEOF, sizeof(P13Rec));
      }
      void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override {
         (void)bufsz; (void)isEOF;
         assert(bufsz == sizeof(P13Rec));
         const P13Rec& prec = *reinterpret_cast<const P13Rec*>(pbuf);
         TwfIvacKind   ivacKind = static_cast<TwfIvacKind>(prec.investor_flag_ - '0');
         if (fon9::unsigned_cast(ivacKind) >= TwfIvacKind_Count)
            return;
         this->KeyItem_.ContractId_ = prec.kind_id_;
         ItemRec& item = this->Items_.kfetch(this->KeyItem_);
         item.PosLimit_[ivacKind] = fon9::Pic9StrTo<uint32_t>(prec.position_limit_);
      }
   };
   fon9::TimeInterval Reload(ConfigLocker&& lk, std::string fname, bool isClearAddTailRemain) override {
      if (this->IsP09Ready(lk))
         return base::Reload(std::move(lk), std::move(fname), isClearAddTailRemain);
      return fon9::TimeInterval_Second(1);
   }
   fon9::seed::FileImpLoaderSP OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) override {
      (void)rbufDesp; (void)monFlag;
      if ((addSize /= sizeof(P13Rec)) > 0)
         return new Loader(*this, addSize);
      return nullptr;
   }
   void OnAfterLoad(fon9::RevBuffer& rbufDesp, fon9::seed::FileImpLoaderSP loader, fon9::seed::FileImpMonitorFlag monFlag) override {
      (void)rbufDesp;
      auto* cfront = MakeOmsImpLog(*this, monFlag, loader->LnCount_);
      fon9_WARN_DISABLE_PADDING;
      assert(dynamic_cast<TwfExgMapMgr*>(&this->OwnerTree_.ConfigMgr_) != nullptr);
      static_cast<TwfExgMapMgr*>(&this->OwnerTree_.ConfigMgr_)->RunCoreTask(
         nullptr, [loader, monFlag, cfront](OmsResource& res) {
         res.LogAppend(fon9::RevBufferList{128, fon9::BufferList{cfront}});
         TwfExgMapMgr&  mapmgr = *static_cast<TwfExgMapMgr*>(&static_cast<Loader*>(loader.get())->Owner_.OwnerTree_.ConfigMgr_);
         TwfExgContractTree*  ctree = mapmgr.GetContractTree();
         if (!ctree)
            return;
         auto contracts{ctree->ContractMap_.Lock()};
         for (auto& item : static_cast<Loader*>(loader.get())->Items_) {
            auto mContract = ctree->ContractMap_.GetContract(contracts, item.ContractId_);
            if (!mContract)
               continue;
            assert(mContract->MainRef_.get() == nullptr  && mContract->MainId_.empty1st());
            auto psSize = mContract->ContractSize_;
            if (!mContract->TargetId_.empty1st()) {
               // 股票類: 尋找 max ContractSize;
               for (auto& pContract : *contracts) {
                  auto& rContract = *static_cast<TwfExgContract*>(pContract.second.get());
                  if (&rContract != mContract.get()
                      && rContract.TradingMarket_ == mContract->TradingMarket_
                      && rContract.TargetId_ == mContract->TargetId_) {
                     rContract.SetMainRef(mContract);
                     if (psSize < rContract.ContractSize_)
                        psSize = rContract.ContractSize_;
                  }
               }
            }
            const auto* plmt = item.PosLimit_;
            for (unsigned L = 0; L < TwfIvacKind_Count; ++L) {
               mContract->PsLimit_[L] = (*plmt == 99999999 ? f9twf::ContractSize::Null()
                                         : (psSize * fon9::unsigned_cast(*plmt)));
               ++plmt;
            }
         }
         mapmgr.SetP13Ready(static_cast<Loader*>(loader.get())->Owner_.ExgSystemType_);
      });
      fon9_WARN_POP;
   }
};
void TwfAddP13Importer(TwfExgMapMgr& twfExgMapMgr) {
   auto& configTree = twfExgMapMgr.GetFileImpSapling();
   ExgMapMgr_AddImpSeed_S2FO(configTree, ImpSeedP13, "1_", "P13");
}

} // namespaces
