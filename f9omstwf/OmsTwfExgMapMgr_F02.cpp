// \file f9omstwf/OmsTwfExgMapMgr_F02.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfExgMapMgr.hpp"
#include "f9omstw/OmsImporter.hpp"
#include "f9twf/TwfSymbRef.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9omstw {

struct ImpSeedF02 : public TwfExgMapMgr::ImpSeedForceLoadSesNormal {
   fon9_NON_COPY_NON_MOVE(ImpSeedF02);
   using base = TwfExgMapMgr::ImpSeedForceLoadSesNormal;

   unsigned LnTotal_{};

   template <class... ArgsT>
   ImpSeedF02(f9twf::ExgSystemType sysType, ArgsT&&... args)
      : base(sysType, fon9::seed::FileImpMonitorFlag::AddTail,
             std::forward<ArgsT>(args)...) {
      this->SetDefaultSchCfg();
   }

   fon9_PACK(1);
   using F02FuncCode = fon9::CharAry<4>;

   struct ProdList {
      fon9::CharAry<10> ShortId_;
      fon9::CharAry<20> LongId_;
   };

   // 100: 契約或商品觸及漲跌停價而即將放寬漲跌幅。
   // 101: 放寬漲跌幅階段;
   struct FuncLmt {
      F02FuncCode       FuncCode_;
      fon9::CharAry<6>  InfoTime_;
      fon9::CharAry<2>  ListType_;  // 列表型別註記: 2=契約代號, 3=商品代號;
      fon9::CharAry<2>  Level_;     // 放寬漲跌停價階段.
      fon9::CharAry<2>  ExpandType_;// 放寬型態: 1=僅放寬漲幅, 2=僅放寬跌幅, 3=同時放寬漲幅及跌幅;
      fon9::CharAry<2>  Count_;
      // ProdList       ProdList_[Count_];
   };
   fon9_PACK_POP;

   struct Loader : public fon9::seed::FileImpLoader {
      fon9_NON_COPY_NON_MOVE(Loader);
      ImpSeedF02&       Owner_;
      fon9::CharVector  RecsStr_;

      Loader(ImpSeedF02& owner, size_t addSize)
         : Owner_{owner} {
         RecsStr_.reserve(addSize);
      }
      ~Loader() {
      }
      // F02 使用換行字元, 所以使用預設的 OnLoadBlock() 轉呼叫 ParseLine(), 然後會來到此處.
      void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override {
         (void)isEOF;
         if (++this->Owner_.LnTotal_ == 1) // Ln.1 為 Header.
            return;
         if (bufsz < sizeof(F02FuncCode))
            return;
         auto funcCode = fon9::Pic9StrTo<uint16_t>(*reinterpret_cast<const F02FuncCode*>(pbuf));
         if (funcCode == 101) { // 契約或商品放寬漲跌幅至某一階段時以此訊息通知。
            this->RecsStr_.append(pbuf, bufsz);
         }
      }
   };
   void ClearReload(ConfigLocker&& lk) override {
      this->LnTotal_ = 0;
      base::ClearReload(std::move(lk));
   }
   fon9::TimeInterval Reload(ConfigLocker&& lk, std::string fname, bool isClearAddTailRemain) override {
      if (this->IsP08Ready(lk))
         return base::Reload(std::move(lk), std::move(fname), isClearAddTailRemain);
      return fon9::TimeInterval_Second(1);
   }
   fon9::seed::FileImpLoaderSP OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) override {
      (void)rbufDesp; (void)monFlag;
      return new Loader(*this, addSize);
   }
   void OnAfterLoad(fon9::RevBuffer& rbufDesp, fon9::seed::FileImpLoaderSP loader, fon9::seed::FileImpMonitorFlag monFlag) override {
      fon9::RevPrint(rbufDesp, "|LnTotal=", this->LnTotal_);
      if (static_cast<Loader*>(loader.get())->RecsStr_.empty())
         return;
      fon9::RevBufferList rbuf{128};
      fon9::RevPrint(rbuf, fon9::LocalNow(),
                     '|', this->Name_, "|mon=", monFlag,
                     "|ftime=", this->LastFileTime(), fon9::kFmtYMD_HH_MM_SS_us_L,
                     "|items=\n", static_cast<Loader*>(loader.get())->RecsStr_);
      auto* cfront = rbuf.MoveOut().ReleaseList();
      fon9_WARN_DISABLE_PADDING;
      assert(dynamic_cast<TwfExgMapMgr*>(&this->OwnerTree_.ConfigMgr_) != nullptr);
      static_cast<TwfExgMapMgr*>(&this->OwnerTree_.ConfigMgr_)->RunCoreTask(
         nullptr, [loader, monFlag, cfront](OmsResource& res) {
         res.LogAppend(fon9::RevBufferList{128, fon9::BufferList{cfront}});
         auto*          tabRef = res.Symbs_->LayoutSP_->GetTab(fon9_kCSTR_TabName_Ref);
         TwfExgMapMgr&  mapmgr = *static_cast<TwfExgMapMgr*>(&static_cast<Loader*>(loader.get())->Owner_.OwnerTree_.ConfigMgr_);
         auto*          ctree = mapmgr.GetContractTree();
         fon9::StrView  srcstr = ToStrView(static_cast<Loader*>(loader.get())->RecsStr_);
         auto           symbs = res.Symbs_->SymbMap_.Lock();
         while(!srcstr.empty()) {
            fon9::StrView  lnstr = fon9::StrFetchNoTrim(srcstr, '\n');
            assert(lnstr.size() >= sizeof(F02FuncCode));
            if (lnstr.size() < sizeof(F02FuncCode))
               continue;
            auto funcCode = fon9::Pic9StrTo<uint16_t>(*reinterpret_cast<const F02FuncCode*>(lnstr.begin()));
            if (funcCode == 101) {
               if (lnstr.size() <= sizeof(FuncLmt))
                  continue;
               const FuncLmt& flmt = *reinterpret_cast<const FuncLmt*>(lnstr.begin());
               const int8_t   level = static_cast<int8_t>(fon9::Pic9StrTo<uint8_t>(flmt.Level_) - 1);
               if (level <= 0)
                  continue;
               auto  count = fon9::Pic9StrTo<uint8_t>(flmt.Count_);
               if (lnstr.size() < sizeof(FuncLmt) + count * sizeof(ProdList))
                  continue;
               const auto      listType   = fon9::Pic9StrTo<uint8_t>(flmt.ListType_);
               const auto      expandType = fon9::Pic9StrTo<uint8_t>(flmt.ExpandType_);
               const ProdList* prodList   = reinterpret_cast<const ProdList*>(lnstr.begin() + sizeof(FuncLmt));
               while (count > 0) {
                  const fon9::StrView shortId = fon9::StrView_eos_or_all(prodList->ShortId_.Chars_, ' ');
                  int8_t* pLevel = nullptr;
                  switch (listType) { // 列表型別註記: 2=契約代號, 3=商品代號;
                  case 2:
                     if (ctree) {
                        if (auto contract = ctree->ContractMap_.FetchContract(shortId)) {
                           pLevel = &contract->LvUpLmt_;
                           assert(pLevel + 1 == &contract->LvDnLmt_);
                        }
                     }
                     break;
                  case 3:
                     if (tabRef) {
                        if (auto symb = res.Symbs_->GetSymb(symbs, shortId)) {
                           assert(dynamic_cast<OmsSymb*>(symb.get()) != nullptr);
                           assert(dynamic_cast<f9twf::TwfSymbRef*>(symb->GetSymbData(tabRef->GetIndex())) != nullptr);
                           auto* symbRef = static_cast<f9twf::TwfSymbRef*>(symb->GetSymbData(tabRef->GetIndex()));
                           pLevel = &symbRef->Data_.LvUpLmt_;
                           assert(pLevel + 1 == &symbRef->Data_.LvDnLmt_);
                        }
                     }
                     break;
                  }
                  if (pLevel) {
                     // 放寬型態: 1=僅放寬漲幅, 2=僅放寬跌幅, 3=同時放寬漲幅及跌幅;
                     switch (expandType) {
                     case 1:  pLevel[0] = level;               break;
                     case 2:  pLevel[1] = level;               break;
                     case 3:  pLevel[0] = pLevel[1] = level;   break;
                     }
                  }
                  ++prodList;
                  --count;
               }
            }
         }
      });
      fon9_WARN_POP;
   }
};
void TwfAddF02Importer(TwfExgMapMgr& twfExgMapMgr) {
   auto& configTree = twfExgMapMgr.GetFileImpSapling();
   ExgMapMgr_AddImpSeed_S2FO(configTree, ImpSeedF02, "", "F02");
}

} // namespaces
