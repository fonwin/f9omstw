// \file f9omstwf/OmsTwfExgMapMgr_C01.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfExgMapMgr.hpp"
#include "f9omstwf/OmsTwfTypes.hpp"

namespace f9omstw {

struct ImpSeedC01 : public fon9::seed::FileImpSeed {
   fon9_NON_COPY_NON_MOVE(ImpSeedC01);
   using base = fon9::seed::FileImpSeed;
   using base::base;

   struct C01Rec {
      char              from_currency_type_; // 換匯前幣別
      char              time_yyyymmdd_[8];   // 時間(年月日)YYYYMMDD
      char              time_hhmmss_[6];     // 時間(時分秒)HHMMSS
      fon9::CharAry<10> exchange_rate_v6_;   // 匯率 9(4)V9(6)
      char              to_currency_type_;   // 換匯後幣別.
      char              filler_[54];         // 空白.
   };
   struct Loader : public fon9::seed::FileImpLoader {
      fon9_NON_COPY_NON_MOVE(Loader);
      ImpSeedC01&          Owner_;
      CurrencyConfig_Tree* CurrencyConfig_;
      using CurrencyMap = std::array<OmsExchangeRateAry, CurrencyIndex_Count>;
      CurrencyMap          CurrencyMap_;

      Loader(ImpSeedC01& owner)
         : Owner_{owner}
         , CurrencyConfig_{static_cast<TwfExgMapMgr*>(&this->Owner_.OwnerTree_.ConfigMgr_)->CurrencyTree_.get()} {
      }
      ~Loader() {
      }
      size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override {
         return this->ParseBlock(pbuf, bufsz, isEOF, sizeof(C01Rec));
      }
      void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override {
         (void)bufsz; (void)isEOF;
         assert(bufsz == sizeof(C01Rec));
         const C01Rec& prec = *reinterpret_cast<const C01Rec*>(pbuf);
         if (auto* currencyFrom = this->CurrencyConfig_->GetCurrencyConfig(prec.from_currency_type_))
         if (auto* currencyTo = this->CurrencyConfig_->GetCurrencyConfig(prec.to_currency_type_))
            this->CurrencyMap_[currencyFrom->CurrencyIndex_][currencyTo->CurrencyIndex_]
                  = OmsExchangeRate::Make<6>(fon9::Pic9StrTo<uint64_t>(prec.exchange_rate_v6_));
      }
   };
   fon9::TimeInterval Reload(ConfigLocker&& lk, std::string fname, bool isClearAddTailRemain) override {
      assert(dynamic_cast<TwfExgMapMgr*>(&this->OwnerTree_.ConfigMgr_) != nullptr
             && static_cast<TwfExgMapMgr*>(&this->OwnerTree_.ConfigMgr_)->CurrencyTree_.get() != nullptr);
      if (static_cast<TwfExgMapMgr*>(&this->OwnerTree_.ConfigMgr_)->IsCurrencyConfigNeedlessOrReady(lk, *this))
         return base::Reload(std::move(lk), std::move(fname), isClearAddTailRemain);
      return fon9::TimeInterval_Second(1);
   }
   fon9::seed::FileImpLoaderSP OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) override {
      (void)rbufDesp; (void)monFlag;
      if ((addSize / sizeof(C01Rec)) > 0)
         return new Loader(*this);
      return nullptr;
   }
   void OnAfterLoad(fon9::RevBuffer& rbufDesp, fon9::seed::FileImpLoaderSP loader, fon9::seed::FileImpMonitorFlag monFlag) override {
      (void)rbufDesp;
      Loader*  ldr = static_cast<Loader*>(loader.get());
      unsigned ifrom, ito;
      // 若 C01 沒提供 A=>B, 則從 B=>A 反算.
      for (ifrom = 0; ifrom < CurrencyIndex_Count; ++ifrom) {
         OmsExchangeRateAry& fromCurrency = ldr->CurrencyMap_[ifrom];
         for (ito = 0; ito < CurrencyIndex_Count; ++ito) {
            if (ifrom == ito)
               continue;
            if (fromCurrency[ito].IsZero()) {
               const auto rRate = ldr->CurrencyMap_[ito][ifrom];
               if (!rRate.IsZero())
                  fromCurrency[ito].AssignRound(1 / rRate.To<double>());
            }
         }
      }
      for (ifrom = 0; ifrom < CurrencyIndex_Count; ++ifrom) {
         OmsExchangeRateAry& fromCurrency = ldr->CurrencyMap_[ifrom];
         for (ito = 0; ito < CurrencyIndex_Count; ++ito) {
            if (ifrom == ito)
               continue;
            auto* upd = ldr->CurrencyConfig_->CurrencyConfigForUpdate(static_cast<CurrencyIndex>(ito));
            if (!upd)
               continue;
            if (!fromCurrency[ito].IsZero())
               upd->SetExchangeRateFrom(static_cast<CurrencyIndex>(ifrom), fromCurrency[ito]);
            else { // 沒有 A <=> B 互換的匯率, 則依 CurrencyIndex: 0..N 的順序轉換, 如果還是不行就放棄.
               for (unsigned idx = 0; idx < CurrencyIndex_Count; ++idx) {
                  if (idx == CurrencyIndex_RMB || idx == ito)
                     continue;
                  const auto rRate1 = fromCurrency[idx];
                  if (rRate1.IsZero())
                     continue;
                  const auto rRate2 = ldr->CurrencyMap_[idx][ito];
                  if (!rRate2.IsZero()) { // ifrom => ito 藉由 ifrom => idx => ito 換匯。
                     upd->SetExchangeRateFrom(static_cast<CurrencyIndex>(ifrom), OmsExchangeRate{rRate1.To<double>() * rRate2.To<double>()});
                     break;
                  }
               }
            }
         }
      }
      fon9::RevBufferList rbuf{128};
      ito = CurrencyIndex_Count;
      do {
         fon9::RevPrint(rbuf, '\n');
         --ito;
         auto* cfg = ldr->CurrencyConfig_->GetCurrencyConfig(static_cast<CurrencyIndex>(ito));
         ifrom = CurrencyIndex_Count;
         for (;;) {
            fon9::RevPrint(rbuf, cfg->FromCurrencyA_[--ifrom]);
            if (ifrom == 0)
               break;
            fon9::RevPrint(rbuf, ',');
         }
         fon9::RevPrint(rbuf, ito, ':');
      } while (ito > 0);
      rbuf.PushFront(MakeOmsImpLog(*this, monFlag, loader->LnCount_));
      static_cast<TwfExgMapMgr*>(&this->OwnerTree_.ConfigMgr_)->CoreLogAppend(std::move(rbuf));
   }
};
void TwfAddC01Importer(TwfExgMapMgr& twfExgMapMgr) {
   auto& configTree = twfExgMapMgr.GetFileImpSapling();
   configTree.Add(new ImpSeedC01(fon9::seed::FileImpMonitorFlag::Reload, configTree, "1_C01", "./tmpsave/C01"));
}

} // namespaces
