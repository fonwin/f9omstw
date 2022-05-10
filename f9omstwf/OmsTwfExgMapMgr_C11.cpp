// \file f9omstwf/OmsTwfExgMapMgr_C11.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfExgMapMgr.hpp"
#include "f9omstwf/OmsTwfTypes.hpp"
#include "f9omstw/OmsImporter.hpp"
#include "f9twf/TwfSymbRef.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9omstw {

struct ImpSeedC11 : public TwfExgMapMgr::ImpSeedForceLoadSesNormal {
   fon9_NON_COPY_NON_MOVE(ImpSeedC11);
   using base = TwfExgMapMgr::ImpSeedForceLoadSesNormal;
   using base::base;

   struct C11Rec {
      fon9::CharAry<10> kind_id_;
      fon9::CharAry<9>  underlying_price_;   // 現貨價, 參考商品價格欄位小數位數.
      fon9::CharAry<8>  filler_;
      fon9::CharAry<8>  date_YYYYMMDD_;
      fon9::CharAry<29> filler2_;
   };
   struct ItemRec {
      OmsTwfContractId  ContractId_;
      uint32_t          LastPrice_;
   };
   using ItemRecs = std::vector<ItemRec>;

   struct Loader : public fon9::seed::FileImpLoader {
      fon9_NON_COPY_NON_MOVE(Loader);
      ImpSeedC11& Owner_;
      ItemRecs    Items_;

      Loader(ImpSeedC11& owner, size_t itemCount)
         : Owner_{owner} {
         this->Items_.reserve(itemCount);
      }
      ~Loader() {
      }
      size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override {
         return this->ParseBlock(pbuf, bufsz, isEOF, sizeof(C11Rec));
      }
      void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override {
         (void)bufsz; (void)isEOF;
         assert(bufsz == sizeof(C11Rec));
         const C11Rec& prec = *reinterpret_cast<const C11Rec*>(pbuf);
         if (prec.kind_id_.Chars_[3] == ' ') {
            this->Items_.resize(this->Items_.size() + 1);
            ItemRec& item = this->Items_.back();
            item.ContractId_.AssignFrom(prec.kind_id_.Chars_);
            item.LastPrice_ = fon9::Pic9StrTo<uint32_t>(prec.underlying_price_);
         }
      }
   };
   fon9::TimeInterval Reload(ConfigLocker&& lk, std::string fname, bool isClearAddTailRemain) override {
      if (this->IsP09Ready(lk))
         return base::Reload(std::move(lk), std::move(fname), isClearAddTailRemain);
      return fon9::TimeInterval_Second(1);
   }
   fon9::seed::FileImpLoaderSP OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) override {
      (void)rbufDesp; (void)monFlag;
      if ((addSize /= sizeof(C11Rec)) > 0)
         return new Loader(*this, addSize);
      return nullptr;
   }
   void OnAfterLoad(fon9::RevBuffer& rbufDesp, fon9::seed::FileImpLoaderSP loader, fon9::seed::FileImpMonitorFlag monFlag) override {
      (void)rbufDesp;
      auto* cfront = MakeOmsImpLog(*this, monFlag, static_cast<Loader*>(loader.get())->Items_.size());
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
            if (auto mContract = ctree->ContractMap_.GetContract(contracts, item.ContractId_))
               mContract->LastPrice_.Assign(item.LastPrice_, mContract->PriDecLoc_);
         }
      });
      fon9_WARN_POP;
   }
};
void TwfAddC11Importer(TwfExgMapMgr& twfExgMapMgr) {
   auto& configTree = twfExgMapMgr.GetFileImpSapling();
   configTree.Add(new ImpSeedC11(f9twf::ExgSystemType::OptNormal, fon9::seed::FileImpMonitorFlag::Reload, configTree, "1_C11", "./tmpsave/C11"));
}

} // namespaces
