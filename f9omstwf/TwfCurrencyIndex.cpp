// \file f9omstwf/TwfCurrencyIndex.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/TwfCurrencyIndex.hpp"
#include "f9omstwf/OmsTwfExgMapMgr.hpp"
#include "fon9/seed/RawRd.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {
using namespace fon9;
using namespace fon9::seed;

CurrencyConfig::~CurrencyConfig() {
}
//--------------------------------------------------------------------------//
class CurrencyConfig_Tree::PodOp : public PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = PodOpDefault;
public:
   const CurrencyConfig& CurrencyConfig_;
   PodOp(Tree& sender, const StrView& keyText, const CurrencyConfig& currencyConfig)
      : base(sender, OpResult::no_error, keyText)
      , CurrencyConfig_(currencyConfig) {
   }
   void BeginRead(Tab& tab, FnReadOp fnCallback) override {
      this->BeginRW(tab, std::move(fnCallback), SimpleRawRd{this->CurrencyConfig_});
   }
};
class CurrencyConfig_Tree::TreeOp : public fon9::seed::TreeOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
   using base = fon9::seed::TreeOp;
public:
   TreeOp(CurrencyConfig_Tree& tree) : base(tree) {
   }
   static void MakeRowView(CurrencyConfigMap::const_iterator ivalue, Tab* tab, RevBuffer& rbuf) {
      if (tab)
         FieldsCellRevPrint(tab->Fields_, SimpleRawRd{*ivalue}, rbuf, GridViewResult::kCellSplitter);
      RevPrint(rbuf, ivalue->CurrencyIndex_);
   }
   void GridView(const GridViewRequest& req, FnGridViewOp fnCallback) override {
      GridViewResult res{this->Tree_, req.Tab_};
      const auto*    map = &static_cast<CurrencyConfig_Tree*>(&this->Tree_)->CurrencyConfigMap_;
      res.ContainerSize_ = map->size();
      size_t istart;
      if (IsTextBegin(req.OrigKey_))
         istart = 0;
      else if (IsTextEnd(req.OrigKey_))
         istart = res.ContainerSize_;
      else {
         istart = StrTo(req.OrigKey_, 0u);
         if (istart >= res.ContainerSize_) {
            res.OpResult_ = OpResult::not_found_key;
            fnCallback(res);
            return;
         }
      }
      MakeGridViewArrayRange(istart, res.ContainerSize_, req, res,
         [&res, map](size_t ivalue, Tab* tab, RevBuffer& rbuf) {
         if (ivalue >= res.ContainerSize_)
            return false;
         if (tab)
            FieldsCellRevPrint(tab->Fields_, SimpleRawRd{(*map)[ivalue]}, rbuf);
         RevPrint(rbuf, ivalue);
         return true;
      });
      fnCallback(res);
   }

   void Get(StrView strKeyText, FnPodOp fnCallback) override {
      const CurrencyIndex idx = static_cast<CurrencyIndex>(StrTo(strKeyText, 0u));
      if (auto* currencyConfig = static_cast<CurrencyConfig_Tree*>(&this->Tree_)->GetCurrencyConfig(idx)) {
         PodOp op(this->Tree_, strKeyText, *currencyConfig);
         fnCallback(op, &op);
      }
      else {
         fnCallback(PodOpResult{this->Tree_, OpResult::not_found_key, strKeyText}, nullptr);
      }
   }
};
//--------------------------------------------------------------------------//
LayoutSP CurrencyConfig_Tree::MakeLayout() {
   Fields fields;
   fields.Add(fon9_MakeField2_const(CurrencyConfig, Alias));
   fields.Add(fon9_MakeField2      (CurrencyConfig, Round));
   for (unsigned idx = 0; idx < CurrencyIndex_Count; ++idx) {
      RevBufferFixedSize<128> rbuf;
      RevPrint(rbuf, "ToCurrency", idx);
      fields.Add(fon9_MakeField(CurrencyConfig, ToCurrency_[idx], rbuf.ToStrT<std::string>()));
   }
   return new Layout1(fon9_MakeField2(CurrencyConfig, CurrencyIndex),
                      new Tab(Named{"Info"}, std::move(fields), TabFlag::NoSapling | TabFlag::NoSeedCommand));
}
CurrencyConfig_Tree::CurrencyConfig_Tree() : base{MakeLayout()} {
   unsigned idx = 0;
   for (auto& c : this->CurrencyConfigMap_) {
      c.ToCurrency_[idx].Assign<0>(1);
      c.CurrencyIndex_ = static_cast<CurrencyIndex>(idx++);
   }
   memset(this->TaiFexToIndex_, 0xff, sizeof(this->TaiFexToIndex_));
}
CurrencyConfig_Tree::~CurrencyConfig_Tree() {
}
void CurrencyConfig_Tree::AddCurrencyAlias(const Locker& lk, const AliasId id, CurrencyIndex idx) {
   if (idx >= CurrencyIndex_Count)
      return;
   auto ifind = lk->find(id);
   if (ifind != lk->end()) // 資料已存在, 不變動內容.
      return;
   lk->kfetch(id).second = idx;
   CurrencyConfig& cfg = this->CurrencyConfigMap_[idx];
   if (!cfg.Alias_.empty())
      cfg.Alias_.push_back(',');
   cfg.Alias_.append(ToStrView(id));
}
void CurrencyConfig_Tree::SetTaiFexToIndex(char chTaiFexCurrency, CurrencyIndex idx) {
   if (!fon9::isalnum(chTaiFexCurrency))
      return;
   if (this->TaiFexToIndex_[fon9::unsigned_cast(chTaiFexCurrency)] < CurrencyIndex_Count)
      return;
   this->TaiFexToIndex_[fon9::unsigned_cast(chTaiFexCurrency)] = idx;
   CurrencyConfig& cfg = this->CurrencyConfigMap_[idx];
   if (!cfg.Alias_.empty())
      cfg.Alias_.push_back(',');
   cfg.Alias_.push_back(chTaiFexCurrency);
}
void CurrencyConfig_Tree::OnTreeOp(FnTreeOp fnCallback) {
   TreeOp op{*this};
   fnCallback(TreeOpResult{this, OpResult::no_error}, &op);
}
//--------------------------------------------------------------------------//
struct CurrencyConfig_ImpSeed::Loader : public FileImpLoader {
   fon9_NON_COPY_NON_MOVE(Loader);
   using base = FileImpLoader;
   CurrencyConfig_Tree& Sapling_;
   Loader(CurrencyConfig_Tree& sapling) : Sapling_(sapling) {
   }
   void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override {
      (void)isEOF;
      StrView ln{pbuf, bufsz};
      StrView alias = StrTrimSplit(ln, '=');
      if (alias.Get1st() == '#')
         return;
      switch (alias.size()) {
      case 1: // 期交所幣別代號.
         this->Sapling_.SetTaiFexToIndex(*alias.begin(), static_cast<CurrencyIndex>(NaiveStrToUInt(ln)));
         break;
      case 2: // 幣別名稱: (NT,TW)
      case 3: // 幣別名稱: (NTD,TWD)
         this->Sapling_.AddCurrencyAlias(this->Sapling_.AliasMap_.Lock(), alias, static_cast<CurrencyIndex>(NaiveStrToUInt(ln)));
         break;
      default: // Round.I = 四捨五入位置.
         #define kCSTR_Round  "Round."
         if (alias.size() > sizeof(kCSTR_Round) - 1
          && memcmp(alias.begin(), kCSTR_Round, sizeof(kCSTR_Round) - 1) == 0) {
            auto idx = static_cast<CurrencyIndex>(NaiveStrToUInt(alias.begin() + sizeof(kCSTR_Round) - 1, alias.end()));
            if (auto* cfg = this->Sapling_.CurrencyConfigForUpdate(idx))
               cfg->Round_ = StrTo(ln, cfg->Round_);
         }
         break;
      }
   }
};

CurrencyConfig_ImpSeed::~CurrencyConfig_ImpSeed() {
}
FileImpLoaderSP CurrencyConfig_ImpSeed::OnBeforeLoad(RevBuffer& rbufDesp, uint64_t addSize, FileImpMonitorFlag monFlag) {
   (void)rbufDesp; (void)addSize; (void)monFlag;
   return new Loader{*this->TwfExgMapMgr_.CurrencyTree_};
}
void CurrencyConfig_ImpSeed::OnAfterLoad(RevBuffer& rbufDesp, FileImpLoaderSP loader, FileImpMonitorFlag monFlag) {
   (void)rbufDesp;
   auto* cfront = MakeOmsImpLog(*this, monFlag, loader->LnCount_);
   this->TwfExgMapMgr_.CoreLogAppend(RevBufferList{128, BufferList{cfront}});
   this->TwfExgMapMgr_.SetCurrencyConfigReady();
}

} // namespaces
