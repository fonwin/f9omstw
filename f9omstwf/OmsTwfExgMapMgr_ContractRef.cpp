// \file f9omstwf/OmsTwfExgMapMgr_ContractRef.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfExgMapMgr.hpp"
#include "f9omstw/OmsImporter.hpp"
#include "f9twf/TwfSymbRef.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9omstw {

struct ImpSeedContractRef : public fon9::seed::FileImpSeed {
   fon9_NON_COPY_NON_MOVE(ImpSeedContractRef);
   using base = fon9::seed::FileImpSeed;
   using base::base;
   struct ItemRec {
      ContractId  ContractId_;
      ContractId  MainId_;
   };
   using ItemRecs = std::vector<ItemRec>;

   struct Loader : public fon9::seed::FileImpLoader {
      fon9_NON_COPY_NON_MOVE(Loader);
      ImpSeedContractRef&  Owner_;
      ItemRecs             Items_;

      Loader(ImpSeedContractRef& owner, size_t addSize)
         : Owner_{owner} {
         Items_.reserve(addSize / sizeof(ItemRec));
      }
      ~Loader() {
      }
      void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override {
         (void)isEOF;
         fon9::StrView ln{pbuf, bufsz};
         fon9::StrView rId = fon9::StrTrimSplit(ln, '=');
         if (rId.empty() || *rId.begin() == '#')
            return;
         ln = fon9::StrFetchNoTrim(ln, '#'); // 移除註解.
         fon9::StrTrimTail(&ln);
         this->Items_.resize(this->Items_.size() + 1);
         ItemRec& item = this->Items_.back();
         item.ContractId_.AssignFrom(rId);
         item.MainId_.AssignFrom(ln);
      }
   };
   fon9::TimeInterval Reload(ConfigLocker&& lk, std::string fname, bool isClearAddTailRemain) override {
      if (static_cast<TwfExgMapMgr*>(&this->OwnerTree_.ConfigMgr_)->IsP09Ready(lk, *this))
         return base::Reload(std::move(lk), std::move(fname), isClearAddTailRemain);
      return fon9::TimeInterval_Second(1);
   }
   fon9::seed::FileImpLoaderSP OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) override {
      (void)rbufDesp; (void)monFlag;
      return new Loader(*this, addSize);
   }
   void OnAfterLoad(fon9::RevBuffer& rbufDesp, fon9::seed::FileImpLoaderSP loader, fon9::seed::FileImpMonitorFlag monFlag) override {
      RevPrint(rbufDesp, "|Items=", static_cast<Loader*>(loader.get())->Items_.size());
      auto* cfront = MakeOmsImpLog(*this, monFlag, loader->LnCount_);
      fon9_WARN_DISABLE_PADDING;
      assert(dynamic_cast<TwfExgMapMgr*>(&this->OwnerTree_.ConfigMgr_) != nullptr);
      static_cast<TwfExgMapMgr*>(&this->OwnerTree_.ConfigMgr_)->RunCoreTask(
         nullptr, [loader, monFlag, cfront](OmsResource& res) {
         res.LogAppend(fon9::RevBufferList{128, fon9::BufferList{cfront}});
         TwfExgMapMgr&  mapmgr = *static_cast<TwfExgMapMgr*>(&static_cast<Loader*>(loader.get())->Owner_.OwnerTree_.ConfigMgr_);
         ContractTree*  ctree = mapmgr.GetContractTree();
         if (!ctree)
            return;
         ContractTree::Locker lk{ctree->ContractMap_};
         for (const ItemRec item : static_cast<Loader*>(loader.get())->Items_) {
            if (auto mContract = ctree->FetchContract(lk, item.MainId_)) {
               assert(mContract->MainId_.empty1st() && mContract->MainRef_.get() == nullptr);
               if (auto rContract = ctree->GetContract(lk, item.ContractId_))
                  rContract->SetMainRef(std::move(mContract));
            }
         }
         mapmgr.SetContractRefReady();
      });
      fon9_WARN_POP;
   }
};
void TwfAddContractRefImporter(TwfExgMapMgr& twfExgMapMgr) {
   auto& configTree = twfExgMapMgr.GetFileImpSapling();
   configTree.Add(new ImpSeedContractRef(configTree, "ContractRef", "TwfContractRef.cfg"));
}

} // namespaces
