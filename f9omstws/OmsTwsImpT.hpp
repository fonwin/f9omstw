// \file f9omstws/OmsTwsImpT.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsImpT_hpp__
#define __f9omstws_OmsTwsImpT_hpp__
#include "f9omstw/OmsImporter.hpp"
#include "f9omstws/OmsTwsTypes.hpp"

namespace f9omstw {

static_assert(f9tws::StkNo::size() == 6, "f9tws::StkNo::size() != 6");
constexpr auto kStkNoSize = f9tws::StkNo::size();

static_assert(f9tws::BrkId::size() == 4, "f9tws::BrkId::size() != 4");
constexpr auto kBrkIdSize = f9tws::BrkId::size();

//--------------------------------------------------------------------------//
fon9_WARN_DISABLE_PADDING;
struct PriRefSrcOfs {
   unsigned LnSize_, PriRef_, PriUpLmt_, PriDnLmt_, GnDayTrade_;

   // T30: StkNo[6], 6:PriUpLmt[9], 15:PriRef[9], 24:PriDnLmt[9]...
   static constexpr PriRefSrcOfs GetOfsT30TSE() {
      return PriRefSrcOfs{100, 15,  6, 24, 86/*GnDayTrade*/};
   }
   static constexpr PriRefSrcOfs GetOfsT30OTC() {
      return PriRefSrcOfs{100, 15,  6, 24, 87/*GnDayTrade*/};
   }
   // 盤後零股價格檔.
   // O40: StkNo[6], Name[16], 22:PriUpLmt[9], 31:PriDnLmt[9], 40:PriRef[9]...
   static constexpr PriRefSrcOfs GetOfsO40() {
      return PriRefSrcOfs{60, 40, 22, 31, 0};
   }
   // 盤中零股價格檔.
   // O60: StkNo[6], 6:PriUpLmt[9], 15:PriDnLmt[9], 24:PriRef[9], 33:GnDayTrade...
   static constexpr PriRefSrcOfs GetOfsO60() {
      return PriRefSrcOfs{50, 24, 6, 15, 33};
   }
};
//--------------------------------------------------------------------------//
// 'ImpT30': layout of class may have changed from a previous version of the compiler due to better packing of member 'ImpT30::PriRefs_'
fon9_MSC_WARN_DISABLE_NO_PUSH(4371);

/// 匯入漲跌參考價: 整股T30、零股O40,O60.
class ImpT30 : public OmsFileImpSeed {
   fon9_NON_COPY_NON_MOVE(ImpT30);
   using base = OmsFileImpSeed;

public:
   const f9fmkt_TradingMarket Market_;
   const PriRefSrcOfs         Ofs_;
   /// 為沒有漲跌停的商品提供額外的價格限制;
   const OmsTwsPri* const     ExtLmtRate_;
   /// 市價下單時的預設行為;
   const ActMarketPri* const  DefaultActMarketPri_;

   template <class... ArgsT>
   ImpT30(f9fmkt_TradingMarket mkt, const PriRefSrcOfs& ofs, const OmsTwsPri* extLmtRate, const ActMarketPri* defaultActMarketPri, ArgsT&&... args)
      : base(fon9::seed::FileImpMonitorFlag::Reload, std::forward<ArgsT>(args)...)
      , Market_(mkt)
      , Ofs_(ofs)
      , ExtLmtRate_(extLmtRate)
      , DefaultActMarketPri_(defaultActMarketPri) {
   }

protected:
   struct ImpItem {
      f9tws::StkNo   StkNo_;
      OmsTwsPriRefs  PriRefs_;
   };
   struct Loader : public OmsFileImpLoader {
      fon9_NON_COPY_NON_MOVE(Loader);
      const PriRefSrcOfs   Ofs_;
      double               ExtLmtRate_;
      const ActMarketPri   DefaultActMarketPri_;
      std::vector<ImpItem> ImpItems_;
      Loader(OmsCoreSP core, size_t itemCount, ImpT30& owner);
      ~Loader();
      size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override;
      void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override;
   };

   fon9::seed::FileImpLoaderSP OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) override;

   /// 衍生者必須自行覆寫 OnAfterLoad() 將 static_cast<Loader*>(loader.get())->ImpItems_ 轉入 Symbs.
   /// 這裡提供一個匯入的範例.
   template <class SymbT>
   void OnAfterLoad_ImpSymbPriRefs(fon9::seed::FileImpLoaderSP loader, OmsTwsPriRefs SymbT::*pPriRefs, fon9::seed::FileImpMonitorFlag monFlag) {
      assert(dynamic_cast<Loader*>(loader.get()) != nullptr);
      const auto mkt = this->Market_;
      static_cast<Loader*>(loader.get())->Core_->RunCoreTask([loader, mkt, monFlag, pPriRefs](OmsResource& res) {
         OmsSymbTree::Locker symbs{res.Symbs_->SymbMap_};
         if (monFlag != fon9::seed::FileImpMonitorFlag::AddTail) {
            // 轉檔前, 先清除全部商品的 PriRefs.
            for (auto& isymb : *symbs) {
               auto* symb = static_cast<SymbT*>(isymb.second.get());
               if (symb->TradingMarket_ == mkt)
                  (symb->*pPriRefs).ClearPriRefs();
            }
         }
         for (const auto& item : static_cast<Loader*>(loader.get())->ImpItems_) {
            if (auto symb = res.Symbs_->FetchSymb(symbs, ToStrView(item.StkNo_))) {
               auto* usymb = static_cast<SymbT*>(symb.get());
               usymb->TradingMarket_ = mkt;
               (usymb->*pPriRefs).CopyFromPriRefs(item.PriRefs_);
            }
         }
      });
   }
};

//--------------------------------------------------------------------------//

/// 匯入整張交易單位 T32.TradeUnit.
class ImpT32 : public OmsFileImpSeed {
   fon9_NON_COPY_NON_MOVE(ImpT32);
   using base = OmsFileImpSeed;

public:
   const unsigned LnSize_;

   template <class... ArgsT>
   ImpT32(unsigned lnSize, ArgsT&&... args)
      : base(fon9::seed::FileImpMonitorFlag::Reload, std::forward<ArgsT>(args)...)
      , LnSize_(lnSize) {
   }

protected:
   struct ImpItem {
      f9tws::StkNo   StkNo_;
      OmsTwsQty      ShUnit_;
   };
   struct Loader : public OmsFileImpLoader {
      fon9_NON_COPY_NON_MOVE(Loader);
      const unsigned       LnSize_;
      std::vector<ImpItem> ImpItems_;
      Loader(OmsCoreSP core, size_t itemCount, unsigned lnsz);
      ~Loader();
      size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override;
      void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override;
   };

   fon9::seed::FileImpLoaderSP OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) override;

   /// 衍生者必須自行覆寫 OnAfterLoad() 將 static_cast<Loader*>(loader.get())->ImpItems_ 轉入 Symbs.
   /// 這裡提供一個匯入的範例.
   template <class SymbT>
   void OnAfterLoad_ImpSymbShUnit(fon9::seed::FileImpLoaderSP loader) {
      assert(dynamic_cast<Loader*>(loader.get()) != nullptr);
      static_cast<Loader*>(loader.get())->Core_->RunCoreTask([loader](OmsResource& res) {
         OmsSymbTree::Locker symbs{res.Symbs_->SymbMap_};
         for (const auto& item : static_cast<Loader*>(loader.get())->ImpItems_) {
            if (auto symb = res.Symbs_->FetchSymb(symbs, ToStrView(item.StkNo_))) {
               auto* usymb = static_cast<SymbT*>(symb.get());
               usymb->ShUnit_ = item.ShUnit_;
            }
         }
      });
   }
};

//--------------------------------------------------------------------------//

/// 匯入定盤交易價: T33.PriFixed;
class ImpT33 : public OmsFileImpSeed {
   fon9_NON_COPY_NON_MOVE(ImpT33);
   using base = OmsFileImpSeed;

public:
   const f9fmkt_TradingMarket Market_;
   const unsigned             LnSize_;

   template <class... ArgsT>
   ImpT33(f9fmkt_TradingMarket mkt, unsigned lnSize, ArgsT&&... args)
      : base(fon9::seed::FileImpMonitorFlag::Reload, std::forward<ArgsT>(args)...)
      , Market_(mkt)
      , LnSize_(lnSize) {
   }

protected:
   struct ImpItem {
      f9tws::StkNo   StkNo_;
      OmsTwsPri      PriFixed_;
   };
   struct Loader : public OmsFileImpLoader {
      fon9_NON_COPY_NON_MOVE(Loader);
      const unsigned       LnSize_;
      std::vector<ImpItem> ImpItems_;
      Loader(OmsCoreSP core, size_t itemCount, unsigned lnsz);
      ~Loader();
      size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override;
      void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override;
   };

   fon9::seed::FileImpLoaderSP OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) override;

   /// 若一般(日盤)尚未收盤, 則在載入定盤交易價時, 表示日盤已收盤 => 觸發收盤事件.
   /// 所以T33匯入應設定排程時間, 避免有昨日的T33造成誤判.
   /// EvName 使用 f9omstw_kCSTR_OmsEventSessionStFactory_Name;
   void FireEvent_SessionNormalClosed(OmsCore& core) const;

   template <class SymbT>
   void OnAfterLoad_ImpSymbPriFixedInCore(fon9::seed::FileImpLoaderSP loader, OmsResource& res, fon9::seed::FileImpMonitorFlag monFlag) {
      OmsSymbTree::Locker symbs{res.Symbs_->SymbMap_};
      if (monFlag != fon9::seed::FileImpMonitorFlag::AddTail) {
         // 轉檔前, 先清除全部商品的 PriFixed_
         for (auto& isymb : *symbs) {
            auto* symb = static_cast<SymbT*>(isymb.second.get());
            if (symb->TradingMarket_ == this->Market_)
               symb->PriFixed_.AssignNull();
         }
      }
      for (const auto& item : static_cast<Loader*>(loader.get())->ImpItems_) {
         if (auto symb = res.Symbs_->FetchSymb(symbs, ToStrView(item.StkNo_))) {
            auto* usymb = static_cast<SymbT*>(symb.get());
            usymb->PriFixed_ = item.PriFixed_;
         }
      }
   }
      
   template <class SymbT>
   void OnAfterLoad_ImpSymbPriFixed(fon9::seed::FileImpLoaderSP loader, fon9::seed::FileImpMonitorFlag monFlag) {
      fon9::intrusive_ptr<ImpT33> pthis(this);
      static_cast<Loader*>(loader.get())->Core_->RunCoreTask([pthis, loader, monFlag](OmsResource& res) {
         pthis->OnAfterLoad_ImpSymbPriFixedInCore<SymbT>(loader, res, monFlag);
      });
   }
};
fon9_WARN_POP;

} // namespaces
#endif//__f9omstws_OmsTwsImpT_hpp__
