// \file f9omstwf/OmsTwfExgMapMgr_P11.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfExgMapMgr.hpp"
#include "f9omstw/OmsImporter.hpp"
#include "f9twf/TwfSymbRef.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9omstw {

struct ImpSeedP11 : public TwfExgMapMgr::ImpSeedForceLoadSesNormal {
   fon9_NON_COPY_NON_MOVE(ImpSeedP11);
   using base = TwfExgMapMgr::ImpSeedForceLoadSesNormal;

   template <class... ArgsT>
   ImpSeedP11(ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...) {
      this->SetDefaultSchCfg();
   }

   fon9_PACK(1);
   using P11Count = fon9::CharAry<2>;
   struct P11Item1 {
      // 階數編號: 01 .. NN
      P11Count          level_;
      // 漲停價格.
      fon9::CharAry<9>  price_;
   };
   using P11PriOrig = uint32_t;
   fon9_PACK_POP;

   struct Loader : public fon9::seed::FileImpLoader {
      fon9_NON_COPY_NON_MOVE(Loader);
      fon9::ByteVector  RecsStr_;

      Loader(size_t addSize) {
         RecsStr_.reserve(addSize);
      }
      ~Loader() {
      }
      void AppendItem(uint8_t cnt, const P11Item1* pitem) {
         this->RecsStr_.push_back(cnt);
         while (cnt > 0) {
            auto pri = fon9::Pic9StrTo<P11PriOrig>(pitem->price_);
            this->RecsStr_.append(&pri, sizeof(pri));
            ++pitem;
            --cnt;
         }
      }
      size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override {
         (void)isEOF;
         for (;;) {
            if (bufsz < sizeof(f9twf::ExgProdIdS) + sizeof(P11Count) * 2)
               return bufsz;
            const auto cntUpLmt = fon9::Pic9StrTo<uint8_t>(*reinterpret_cast<P11Count*>(pbuf + sizeof(f9twf::ExgProdIdS)));
            const auto ofsCntDnLmt = sizeof(f9twf::ExgProdIdS) + sizeof(P11Count) + sizeof(P11Item1) * cntUpLmt;
            if (bufsz < ofsCntDnLmt + sizeof(P11Count))
               return bufsz;
            const auto cntDnLmt = fon9::Pic9StrTo<uint8_t>(*reinterpret_cast<P11Count*>(pbuf + ofsCntDnLmt));
            const auto recSize = ofsCntDnLmt + sizeof(P11Count) + sizeof(P11Item1) * cntDnLmt;
            if (bufsz < recSize)
               return bufsz;
            ++this->LnCount_;
            this->RecsStr_.append(pbuf, sizeof(f9twf::ExgProdIdS));
            this->AppendItem(cntUpLmt, reinterpret_cast<P11Item1*>(pbuf + sizeof(f9twf::ExgProdIdS) + sizeof(P11Count)));
            this->AppendItem(cntDnLmt, reinterpret_cast<P11Item1*>(pbuf + ofsCntDnLmt + sizeof(P11Count)));
            pbuf += recSize;
            bufsz -= recSize;
         }
      }
   };
   fon9::TimeInterval Reload(ConfigLocker&& lk, std::string fname, bool isClearAddTailRemain) override {
      if (this->IsP08Ready(lk)) // 必須等 P08 載入, 才有價格小數位, 才能處理漲跌停價.
         return base::Reload(std::move(lk), std::move(fname), isClearAddTailRemain);
      return fon9::TimeInterval_Second(1);
   }
   fon9::seed::FileImpLoaderSP OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) override {
      (void)rbufDesp; (void)monFlag;
      return new Loader(addSize);
   }
   static void AssignPriLmt(uint8_t cnt, f9twf::TwfSymbRef_Data::PriLmt* lmts, const void* pSrc, unsigned udIdx, int64_t mul) {
      if (cnt > f9twf::TwfSymbRef_Data::kPriLmtCount) {
         cnt = f9twf::TwfSymbRef_Data::kPriLmtCount;
      }
      P11PriOrig src;
      while (cnt > 0) {
         memcpy(&src, pSrc, sizeof(src)); // pSrc is unaligned.
         (&lmts->Up_)[udIdx].SetOrigValue(src * mul);
         ++lmts;
         pSrc = static_cast<const char*>(pSrc) + sizeof(src);
         --cnt;
      }
   }
   void OnAfterLoad(fon9::RevBuffer& rbufDesp, fon9::seed::FileImpLoaderSP loader, fon9::seed::FileImpMonitorFlag monFlag) override {
      (void)rbufDesp;
      auto* cfront = MakeOmsImpLog(*this, monFlag, loader->LnCount_);
      fon9_WARN_DISABLE_PADDING;
      assert(dynamic_cast<TwfExgMapMgr*>(&this->OwnerTree_.ConfigMgr_) != nullptr);
      static_cast<TwfExgMapMgr*>(&this->OwnerTree_.ConfigMgr_)->RunCoreTask(
         nullptr, [loader, monFlag, cfront](OmsResource& res) {
         res.LogAppend(fon9::RevBufferList{128, fon9::BufferList{cfront}});
         auto* tabRef = res.Symbs_->LayoutSP_->GetTab(fon9_kCSTR_TabName_Ref);
         assert(tabRef != nullptr);
         if (tabRef == nullptr)
            return;
         const auto* psrc = static_cast<Loader*>(loader.get())->RecsStr_.begin();
         const auto* pend = static_cast<Loader*>(loader.get())->RecsStr_.end();
         auto symbs = res.Symbs_->SymbMap_.Lock();
         while (psrc != pend) {
            auto symbid = fon9::StrView_eos_or_all(reinterpret_cast<const f9twf::ExgProdIdS*>(psrc)->ProdId_.Chars_, ' ');
            auto symb = res.Symbs_->GetSymb(symbs, symbid);
            psrc += sizeof(f9twf::ExgProdIdS);
            auto cntUpLmt = *psrc++;
            auto* pUpLmt = psrc;
            psrc += sizeof(P11PriOrig) * cntUpLmt;
            auto cntDnLmt = *psrc++;
            auto* pDnLmt = psrc;
            psrc += sizeof(P11PriOrig) * cntDnLmt;
            if (!symb)
               continue;
            assert(dynamic_cast<OmsSymb*>(symb.get()) != nullptr);
            assert(dynamic_cast<f9twf::TwfSymbRef*>(symb->GetSymbData(tabRef->GetIndex())) != nullptr);
            auto* symbRef = static_cast<f9twf::TwfSymbRef*>(symb->GetSymbData(tabRef->GetIndex()));
            memset(symbRef->Data_.PriLmts_, 0, sizeof(symbRef->Data_.PriLmts_));
            int64_t mul = static_cast<OmsSymb*>(symb.get())->PriceOrigDiv_;
            AssignPriLmt(cntUpLmt, symbRef->Data_.PriLmts_, pUpLmt, 0, mul);
            AssignPriLmt(cntDnLmt, symbRef->Data_.PriLmts_, pDnLmt, 1, mul);
         }
      });
      fon9_WARN_POP;
   }
};
void TwfAddP11Importer(TwfExgMapMgr& twfExgMapMgr) {
   auto& configTree = twfExgMapMgr.GetFileImpSapling();
   ExgMapMgr_AddImpSeed_S2FO(configTree, ImpSeedP11, "", "P11");
}

} // namespaces
