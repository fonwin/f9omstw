// \file f9omstwf/OmsTwfExgMapMgr_N09.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfExgMapMgr.hpp"
#include "f9omstw/OmsImporter.hpp"
#include "f9twf/TwfSymbRef.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9omstw {

struct ImpSeedN09 : public TwfExgMapMgr::ImpSeedForceLoadSesNormal {
   fon9_NON_COPY_NON_MOVE(ImpSeedN09);
   using base = TwfExgMapMgr::ImpSeedForceLoadSesNormal;

   unsigned LnTotal_{};

   template <class... ArgsT>
   ImpSeedN09(f9twf::ExgSystemType sysType, ArgsT&&... args)
      : base(sysType, fon9::seed::FileImpMonitorFlag::AddTail,
             std::forward<ArgsT>(args)...) {
      this->SetDefaultSchCfg();
   }

   fon9_PACK(1);
   struct N09Rec {
      fon9::CharAry<6>  stock_id_;
      fon9::CharAry<1>  status_code_; // 1:限制, 0:解除;
   };
   fon9_PACK_POP;

   struct Loader : public fon9::seed::FileImpLoader {
      fon9_NON_COPY_NON_MOVE(Loader);
      ImpSeedN09&       Owner_;
      fon9::CharVector  RecsStr_;

      Loader(ImpSeedN09& owner, size_t addItems)
         : Owner_{owner} {
         RecsStr_.reserve(addItems * sizeof(N09Rec));
      }
      ~Loader() {
      }
      size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override {
         return this->ParseBlock(pbuf, bufsz, isEOF, sizeof(N09Rec));
      }
      void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override {
         (void)isEOF; (void)bufsz; assert(bufsz == sizeof(N09Rec));
         N09Rec n09;
         n09.stock_id_.AssignFrom(ToStrView(*reinterpret_cast<const f9twf::StkNo*>(pbuf)));
         n09.status_code_ = reinterpret_cast<const N09Rec*>(pbuf)->status_code_;
         RecsStr_.append(&n09, sizeof(n09));
      }
   };
   fon9::TimeInterval Reload(ConfigLocker&& lk, std::string fname, bool isClearAddTailRemain) override {
      if (this->IsP09Ready(lk))
         return base::Reload(std::move(lk), std::move(fname), isClearAddTailRemain);
      return fon9::TimeInterval_Second(1);
   }
   fon9::seed::FileImpLoaderSP OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) override {
      (void)rbufDesp; (void)monFlag;
      if ((addSize /= sizeof(N09Rec)) > 0)
         return new Loader(*this, addSize);
      return nullptr;
   }
   void OnAfterLoad(fon9::RevBuffer& rbufDesp, fon9::seed::FileImpLoaderSP loader, fon9::seed::FileImpMonitorFlag monFlag) override {
      (void)rbufDesp;
      if (static_cast<Loader*>(loader.get())->RecsStr_.empty())
         return;
      auto  mkt = ExgSystemTypeToMarket(this->ExgSystemType_);
      auto* cfront = MakeOmsImpLog(*this, monFlag, loader->LnCount_);
      fon9_WARN_DISABLE_PADDING;
      assert(dynamic_cast<TwfExgMapMgr*>(&this->OwnerTree_.ConfigMgr_) != nullptr);
      static_cast<TwfExgMapMgr*>(&this->OwnerTree_.ConfigMgr_)->RunCoreTask(
         nullptr, [loader, monFlag, mkt, cfront](OmsResource& res) {
         res.LogAppend(fon9::RevBufferList{128, fon9::BufferList{cfront}});
         TwfExgMapMgr&  mapmgr = *static_cast<TwfExgMapMgr*>(&static_cast<Loader*>(loader.get())->Owner_.OwnerTree_.ConfigMgr_);
         auto*          ctree = mapmgr.GetContractTree();
         if (!ctree)
            return;
         const char* pbeg = static_cast<Loader*>(loader.get())->RecsStr_.begin();
         const void* pend = static_cast<Loader*>(loader.get())->RecsStr_.end();
         auto contracts{ctree->ContractMap_.Lock()};
         while (pbeg + sizeof(N09Rec) <= pend) {
            const N09Rec* n09 = reinterpret_cast<const N09Rec*>(pbeg);
            for (const auto& icontract : *contracts) {
               auto contract = static_cast<TwfExgContract*>(icontract.second.get());
               if (mkt == contract->TradingMarket_ && contract->TargetId_ == n09->stock_id_) {
                  switch (n09->status_code_.Chars_[0]) {
                  case '1':
                     if (contract->DenyOpen_.empty())
                        contract->DenyOpen_.assign("9");
                     break;
                  case '0':
                     if (contract->DenyOpen_.size() == 1 && *contract->DenyOpen_.begin() == '9')
                        contract->DenyOpen_.clear();
                     break;
                  }
               }
            }
            pbeg += sizeof(N09Rec);
         }
      });
      fon9_WARN_POP;
   }
};
void TwfAddN09Importer(TwfExgMapMgr& twfExgMapMgr) {
   auto& configTree = twfExgMapMgr.GetFileImpSapling();
   ExgMapMgr_AddImpSeed_S2FO(configTree, ImpSeedN09, "", "N09");
}

} // namespaces
