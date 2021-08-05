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
enum class TwsMarketMask {
   None = 0x00,
   TwSEC = 0x01,
   TwOTC = 0x02,
   All = TwSEC | TwOTC,
};
fon9_ENABLE_ENUM_BITWISE_OP(TwsMarketMask);

static inline TwsMarketMask GetTwsMarketMask(f9fmkt_TradingMarket mkt) {
   return (mkt == f9fmkt_TradingMarket_TwSEC ? TwsMarketMask::TwSEC : TwsMarketMask::TwOTC);
}

//--------------------------------------------------------------------------//
fon9_WARN_DISABLE_PADDING;
struct TrsRefSrcOfs {
   unsigned LnSize_, PriRef_, PriUpLmt_, PriDnLmt_, GnDayTrade_, DenyOfs_;

   // T30: StkNo[6], 6:PriUpLmt[9], 15:PriRef[9], 24:PriDnLmt[9]...
   static constexpr TrsRefSrcOfs GetOfsT30TSE() {
      return TrsRefSrcOfs{100, 15,  6, 24, 86/*GnDayTrade*/, 41/*DenyOfs=&T30.SETTYPE.MARK-W*/};
   }
   static constexpr TrsRefSrcOfs GetOfsT30OTC() {
      return TrsRefSrcOfs{100, 15,  6, 24, 87/*GnDayTrade*/, 41/*DenyOfs=&T30.SETTYPE.MARK-W*/};
   }
   // 盤後零股價格檔.
   // O40: StkNo[6], Name[16], 22:PriUpLmt[9], 31:PriDnLmt[9], 40:PriRef[9]...
   static constexpr TrsRefSrcOfs GetOfsO40() {
      return TrsRefSrcOfs{60, 40, 22, 31, 0, 0};
   }
   // 盤中零股價格檔.
   // O60: StkNo[6], 6:PriUpLmt[9], 15:PriDnLmt[9], 24:PriRef[9], 33:GnDayTrade...
   static constexpr TrsRefSrcOfs GetOfsO60() {
      return TrsRefSrcOfs{50, 24, 6, 15, 33, 0};
   }
};
//--------------------------------------------------------------------------//
// 'ImpT30': layout of class may have changed from a previous version of the compiler due to better packing of member
fon9_MSC_WARN_DISABLE_NO_PUSH(4371);

/// 匯入漲跌參考價: 整股T30、零股O40,O60.
class ImpT30 : public OmsFileImpSeed {
   fon9_NON_COPY_NON_MOVE(ImpT30);
   using base = OmsFileImpSeed;

public:
   const f9fmkt_TradingMarket Market_;
   const TrsRefSrcOfs         Ofs_;
   /// 為沒有漲跌停的商品提供額外的價格限制;
   const OmsTwsPri* const     ExtLmtRate_;
   /// 市價下單時的預設行為;
   const ActMarketPri* const  DefaultActMarketPri_;

   template <class... ArgsT>
   ImpT30(f9fmkt_TradingMarket mkt, const TrsRefSrcOfs& ofs, const OmsTwsPri* extLmtRate, const ActMarketPri* defaultActMarketPri, ArgsT&&... args)
      : base(FileImpMonitorFlag::Reload, std::forward<ArgsT>(args)...)
      , Market_(mkt)
      , Ofs_(ofs)
      , ExtLmtRate_(extLmtRate)
      , DefaultActMarketPri_(defaultActMarketPri) {
   }

   static TwsMarketMask Ready_;

protected:
   struct ImpItem {
      f9tws::StkNo      StkNo_;
      TrsRefs           Refs_;
      fon9::CharVector  DenyReason_; // "S1"(SETTYPE=1), "S2"(SETTYPE=2), "W1"(MARK-W=1), "W2"(MARK-W=2)...
   };
   struct Loader : public OmsFileImpLoader {
      fon9_NON_COPY_NON_MOVE(Loader);
      const TrsRefSrcOfs   Ofs_;
      double               ExtLmtRate_;
      const ActMarketPri   DefaultActMarketPri_;
      std::vector<ImpItem> ImpItems_;
      Loader(OmsFileImpTree& ownerTree, ImpT30& owner, size_t itemCount);
      ~Loader();
      size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override;
      void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override;
   };

   OmsFileImpLoaderSP MakeLoader(OmsFileImpTree& owner, fon9::RevBuffer& rbufDesp, uint64_t addSize, FileImpMonitorFlag monFlag) override;

   template <class SymbT>
   static void ImpTrsRefs(OmsResource& res, Loader& loader, f9fmkt_TradingMarket mkt, TrsRefs SymbT::*pRefs, FileImpMonitorFlag monFlag) {
      OmsSymbTree::Locker symbs{res.Symbs_->SymbMap_};
      if (monFlag != FileImpMonitorFlag::AddTail) {
         // 轉檔前, 先清除全部商品的 TrsRefs.
         for (auto& isymb : *symbs) {
            auto* symb = static_cast<SymbT*>(isymb.second.get());
            if (symb->TradingMarket_ == mkt) {
               (symb->*pRefs).Clear();
               if (loader.Ofs_.DenyOfs_ != 0)
                  symb->DenyReason_.clear();
            }
         }
      }
      for (const auto& item : loader.ImpItems_) {
         if (auto symb = res.Symbs_->FetchSymb(symbs, ToStrView(item.StkNo_))) {
            auto* usymb = static_cast<SymbT*>(symb.get());
            usymb->TradingMarket_ = mkt;
            (usymb->*pRefs).CopyFrom(item.Refs_);
            if (loader.Ofs_.DenyOfs_ != 0)
               usymb->DenyReason_ = std::move(item.DenyReason_);
         }
      }
      SetReadyFlag(mkt);
   }

   /// 衍生者必須自行覆寫 OnAfterLoad() 將 static_cast<Loader*>(loader.get())->ImpItems_ 轉入 Symbs.
   /// 這裡提供一個匯入的範例.
   template <class SymbT>
   static void OnAfterLoad_ImpTrsRefs(fon9::seed::FileImpLoaderSP loader, f9fmkt_TradingMarket mkt, TrsRefs SymbT::*pRefs, FileImpMonitorFlag monFlag) {
      assert(dynamic_cast<Loader*>(loader.get()) != nullptr);
      static_cast<Loader*>(loader.get())->RunCoreTask([loader, mkt, monFlag, pRefs](OmsResource& res) {
         ImpTrsRefs(res, *static_cast<Loader*>(loader.get()), mkt, pRefs, monFlag);
      });
   }
   static void SetReadyFlag(f9fmkt_TradingMarket mkt) {
      Ready_ |= GetTwsMarketMask(mkt);
   }
};

//--------------------------------------------------------------------------//
/// 匯入整張交易單位 T32.TradeUnit.
class ImpT32 : public OmsFileImpSeed {
   fon9_NON_COPY_NON_MOVE(ImpT32);
   using base = OmsFileImpSeed;

public:
   const f9fmkt_TradingMarket Market_;
   const unsigned             LnSize_;

   template <class... ArgsT>
   ImpT32(f9fmkt_TradingMarket mkt, unsigned lnSize, ArgsT&&... args)
      : base(FileImpMonitorFlag::Reload, std::forward<ArgsT>(args)...)
      , Market_(mkt)
      , LnSize_(lnSize) {
   }

   static TwsMarketMask Ready_;

protected:
   struct ImpItem {
      f9tws::StkNo   StkNo_;
      OmsTwsQty      ShUnit_;
   };
   struct Loader : public OmsFileImpLoader {
      fon9_NON_COPY_NON_MOVE(Loader);
      const unsigned       LnSize_;
      std::vector<ImpItem> ImpItems_;
      Loader(OmsFileImpTree& owner, size_t itemCount, unsigned lnsz);
      ~Loader();
      size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override;
      void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override;
   };

   OmsFileImpLoaderSP MakeLoader(OmsFileImpTree& owner, fon9::RevBuffer& rbufDesp, uint64_t addSize, FileImpMonitorFlag monFlag) override;

   /// 如果 T32 為空, 仍須設定 Ready_ 旗標;
   void OnLoadEmptyFile() override;

   /// 衍生者必須自行覆寫 OnAfterLoad() 將 static_cast<Loader*>(loader.get())->ImpItems_ 轉入 Symbs.
   /// 這裡提供一個匯入的範例.
   template <class SymbT>
   void OnAfterLoad_ImpSymbShUnit(fon9::seed::FileImpLoaderSP loader, FileImpMonitorFlag monFlag) {
      assert(dynamic_cast<Loader*>(loader.get()) != nullptr);
      const auto mkt = this->Market_;
      auto* cfront = MakeOmsImpLog(*this, monFlag, static_cast<Loader*>(loader.get())->ImpItems_.size());
      static_cast<Loader*>(loader.get())->RunCoreTask([mkt, loader, cfront](OmsResource& res) {
         res.LogAppend(fon9::RevBufferList{128, fon9::BufferList{cfront}});
         OmsSymbTree::Locker symbs{res.Symbs_->SymbMap_};
         for (const auto& item : static_cast<Loader*>(loader.get())->ImpItems_) {
            if (auto symb = res.Symbs_->FetchSymb(symbs, ToStrView(item.StkNo_))) {
               auto* usymb = static_cast<SymbT*>(symb.get());
               usymb->ShUnit_ = item.ShUnit_;
            }
         }
         SetReadyFlag(mkt);
      });
   }
   static void SetReadyFlag(f9fmkt_TradingMarket mkt) {
      Ready_ |= GetTwsMarketMask(mkt);
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
      : base(FileImpMonitorFlag::Reload, std::forward<ArgsT>(args)...)
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
      Loader(OmsFileImpTree& owner, size_t itemCount, unsigned lnsz);
      ~Loader();
      size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override;
      void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override;
   };

   OmsFileImpLoaderSP MakeLoader(OmsFileImpTree& owner, fon9::RevBuffer& rbufDesp, uint64_t addSize, FileImpMonitorFlag monFlag) override;

   /// 若一般(日盤)尚未收盤, 則在載入定盤交易價時, 表示日盤已收盤 => 觸發收盤事件.
   /// 所以T33匯入應設定排程時間, 避免有昨日的T33造成誤判.
   /// EvName 使用 f9omstw_kCSTR_OmsEventSessionStFactory_Name;
   void FireEvent_SessionNormalClosed(OmsCore& core) const;

   template <class SymbT>
   void OnAfterLoad_ImpSymbPriFixedInCore(fon9::seed::FileImpLoaderSP loader, OmsResource& res, FileImpMonitorFlag monFlag) {
      OmsSymbTree::Locker symbs{res.Symbs_->SymbMap_};
      if (monFlag != FileImpMonitorFlag::AddTail) {
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
   void OnAfterLoad_ImpSymbPriFixed(fon9::seed::FileImpLoaderSP loader, FileImpMonitorFlag monFlag) {
      fon9::intrusive_ptr<ImpT33> pthis(this);
      static_cast<Loader*>(loader.get())->RunCoreTask([pthis, loader, monFlag](OmsResource& res) {
         pthis->OnAfterLoad_ImpSymbPriFixedInCore<SymbT>(loader, res, monFlag);
      });
   }
};
fon9_WARN_POP;

} // namespaces
#endif//__f9omstws_OmsTwsImpT_hpp__
