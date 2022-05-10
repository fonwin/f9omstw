// \file f9omstwf/OmsTwfExgMapMgr_P14.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfExgMapMgr.hpp"
#include "f9omstwf/OmsTwfTypes.hpp"
#include "f9omstw/OmsImporter.hpp"
#include "f9twf/TwfSymbRef.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9omstw {

struct ImpSeedP14 : public TwfExgMapMgr::ImpSeedForceLoadSesNormal {
   fon9_NON_COPY_NON_MOVE(ImpSeedP14);
   using base = TwfExgMapMgr::ImpSeedForceLoadSesNormal;

   template <class... ArgsT>
   ImpSeedP14(ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...) {
      this->SetDefaultSchCfg();
   }

   struct P14Rec {
      OmsTwfContractId  kind_id_;
      char              ab_type_;         // 0:非選擇權商品; A,B,C:選擇權商品所屬 ABC 值;
      char              margin_type_;     // 保證金型態 0:表金額。1:表比例(%)
      char              currency_type_;   // 保證金幣別;
      fon9::CharAry<9>  margin_;          // 結算:保證金金額9(9)/保證金適用比例9(6)V999(%)
      fon9::CharAry<9>  margin_maintain_; // 維持:保證金金額9(9)/保證金適用比例9(6)V999(%)
      fon9::CharAry<9>  margin_initial_;  // 原始:保證金金額9(9)/保證金適用比例9(6)V999(%)
   };
   struct ItemRec {
      OmsTwfContractId  ContractId_;
      char              ab_type_;
      fon9::EnabledYN   Rate_;
      char              Padding____[2];
      OmsTwfPri         Risk_;
      void SetMargin(uint32_t margin) {
         if (margin) {
            if (this->Rate_ == fon9::EnabledYN::Yes)
               this->Risk_.Assign<5>(margin);
            else
               this->Risk_.Assign<0>(margin);
         }
         else { // margin == 0 直接用固定值, 計算時不用再[乘上margin]。
            this->Rate_ = fon9::EnabledYN{};
            this->Risk_.Assign0();
         }

      }
   };
   using ItemRecs = std::vector<ItemRec>;

   struct Loader : public fon9::seed::FileImpLoader {
      fon9_NON_COPY_NON_MOVE(Loader);
      ImpSeedP14& Owner_;
      ItemRecs    Items_;

      Loader(ImpSeedP14& owner, size_t itemCount)
         : Owner_{owner} {
         this->Items_.reserve(itemCount);
      }
      ~Loader() {
      }
      size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override {
         return this->ParseBlock(pbuf, bufsz, isEOF, sizeof(P14Rec));
      }
      void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override {
         (void)bufsz; (void)isEOF; assert(bufsz == sizeof(P14Rec));
         const P14Rec& prec = *reinterpret_cast<const P14Rec*>(pbuf);
         this->Items_.resize(this->Items_.size() + 1);
         auto& itemP = this->Items_.back();
         itemP.ContractId_ = prec.kind_id_;
         itemP.ab_type_ = prec.ab_type_;
         itemP.Rate_ = (prec.margin_type_ == '1' ? fon9::EnabledYN::Yes : fon9::EnabledYN{});
         itemP.SetMargin(fon9::Pic9StrTo<uint32_t>(prec.margin_initial_));
         if (prec.ab_type_ == '0') { // 將期貨[結算保證金]匯入 IRiskC_;
            this->Items_.resize(this->Items_.size() + 1);
            auto& itemB = this->Items_.back();
            itemB.ContractId_ = prec.kind_id_;
            itemB.ab_type_ = 'C';
            itemB.Rate_ = itemP.Rate_;
            itemB.SetMargin(fon9::Pic9StrTo<uint32_t>(prec.margin_));
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
      if ((addSize /= sizeof(P14Rec)) > 0) {
         if (ExgSystemTypeToMarket(this->ExgSystemType_) == f9fmkt_TradingMarket_TwFUT) // 期貨要匯入 [結算保證金] 給 [選擇權時間價差] 計算保證金.
            addSize *= 2;                                                               // 所以預計匯入項目會變成 2 倍;
         return new Loader(*this, addSize);
      }
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
            switch (item.ab_type_) {
            case '0': // 期貨, 設定 RiskA 之後, 會用 ab_type_ = 'C' 設定 RiskC, 所以這裡把沒用到的設為 Null;
               mContract->RiskIni_.RiskB_ = OmsTwfPri::Null();
               mContract->RiskIni_.RateB_ = fon9::EnabledYN{};
               /* fall through */
            case 'A':
               mContract->RiskIni_.RiskA_ = item.Risk_;
               mContract->RiskIni_.RateA_ = item.Rate_;
               break;
            case 'B':
               mContract->RiskIni_.RiskB_ = item.Risk_;
               mContract->RiskIni_.RateB_ = item.Rate_;
               break;
            case 'C':
               mContract->RiskIni_.RiskC_ = item.Risk_;
               mContract->RiskIni_.RateC_ = item.Rate_;
               break;
            }
         }
      });
      fon9_WARN_POP;
   }
};
void TwfAddP14Importer(TwfExgMapMgr& twfExgMapMgr) {
   auto& configTree = twfExgMapMgr.GetFileImpSapling();
   ExgMapMgr_AddImpSeed_S2FO(configTree, ImpSeedP14, "1_", "P14");
}

} // namespaces
