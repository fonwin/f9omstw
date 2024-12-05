// \file f9omstws/OmsTwsMdSystem.cpp
//
// Oms Market data from [TwsMd]
//
// \author fonwinz@gmail.com
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsMdEvent.hpp"
#include "fon9/seed/Plugins.hpp"
#include "fon9/seed/SysEnv.hpp"
#include "fon9/framework/IoManagerTree.hpp"
#include "fon9/framework/IoFactory.hpp"
#include "f9tws/ExgMdReceiverFactory.hpp"
#include "f9tws/ExgMdFmt6.hpp"
#include "f9tws/ExgMdFmt23.hpp"

namespace f9omstw {

class OmsTwsMdSystem : public f9tws::ExgMdSystemBase {
   fon9_NON_COPY_NON_MOVE(OmsTwsMdSystem);
   using base = f9tws::ExgMdSystemBase;
   OmsCoreSP                  OmsCore_;
   uint64_t                   IntervalDataCount_{0};
   fon9::fmkt::MdReceiverSt   MdReceiverSt_{fon9::fmkt::MdReceiverSt::DailyClear};
public:
   const f9fmkt_TradingMarket Market_;
   char                       Padding____[6];
   const OmsCoreMgrSP         OmsCoreMgr_;

   static bool StartupPlugins(fon9::seed::PluginsHolder& holder, fon9::StrView args);

   template <class... ArgsT>
   OmsTwsMdSystem(f9fmkt_TradingMarket mkt, OmsCoreMgrSP omsCoreMgr, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , Market_(mkt)
      , OmsCoreMgr_{std::move(omsCoreMgr)} {
   }
   ~OmsTwsMdSystem() {
      this->HbTimer_.DisposeAndWait();
   }
   fon9::TimeInterval OnMdSystem_HbTimer(fon9::TimeStamp now) override {
      (void)now;
      if (this->IntervalDataCount_) {
         this->IntervalDataCount_ = 0;
         return kMdHbInterval;
      }
      switch (this->MdReceiverSt_) {
      case fon9::fmkt::MdReceiverSt::DailyClear:
         this->MdReceiverSt_ = fon9::fmkt::MdReceiverSt::DailyClearNoData;
         break;
      case fon9::fmkt::MdReceiverSt::DataIn:
         this->MdReceiverSt_ = fon9::fmkt::MdReceiverSt::DataBroken;
         break;
      case fon9::fmkt::MdReceiverSt::DailyClearNoData:
      case fon9::fmkt::MdReceiverSt::DataBroken:
         break;
      }
      this->OmsCore_ = this->OmsCoreMgr_->CurrentCore();
      if (this->OmsCore_) {
         this->OmsCore_->RunCoreTask([this](OmsResource& coreResource) {
            auto mkt = this->Market_;
            auto symbs = coreResource.Symbs_->SymbMap_.Lock();
            for (const auto& isymb : *symbs) {
               if (auto* symb = static_cast<OmsSymb*>(isymb.second.get())) {
                  if (symb->TradingMarket_ == mkt) {
                     symb->SetMdReceiverStPtr(&this->MdReceiverSt_);
                     symb->OnMdNoData(coreResource);
                  }
               }
            }
         });
      }
      return kMdHbInterval;
   }
   OmsCoreSP GetOmsCore() {
      if (OmsCoreSP omsCore = this->OmsCore_)
         return omsCore;
      return this->OmsCore_ = this->OmsCoreMgr_->CurrentCore();
   }
   OmsSymbSP FetchOmsSymb(const f9tws::StkNo stkNo, OmsCoreSP& omsCore) {
      omsCore = this->GetOmsCore();
      if (fon9_UNLIKELY(!omsCore)) {
         return nullptr;
      }
      auto symb = omsCore->GetSymbs()->FetchOmsSymb(ToStrView(stkNo));
      if (!symb)
         return nullptr;
      ++this->IntervalDataCount_;
      this->MdReceiverSt_ = fon9::fmkt::MdReceiverSt::DataIn;
      symb->SetMdReceiverStPtr(&this->MdReceiverSt_);
      return symb;
   }
};
//--------------------------------------------------------------------------//
class OmsTwsMdFmtRtHandler : public f9tws::ExgMdHandlerPkCont {
   fon9_NON_COPY_NON_MOVE(OmsTwsMdFmtRtHandler);
   using base = f9tws::ExgMdHandlerPkCont;
protected:
   f9fmkt_TradingSessionSt TradingSessionSt_{};
   char                    Padding_____[7];

   /// - kTradingSessionId == f9fmkt_TradingSessionId_Normal; MdFmt = Fmt6:股票; Fmt17:權證;  symbs = *handler.MdSys_.Symbs_;
   /// - kTradingSessionId == f9fmkt_TradingSessionId_OddLot; MdFmt = Fmt23:零股;             symbs = *handler.MdSys_.SymbsOdd_;
   template <f9fmkt_TradingSessionId kTradingSessionId, class MdFmt>
   void OnMdFmtRt_Received(const MdFmt& mdfmt, unsigned pksz, SeqT seq) {
      (void)pksz;
      this->CheckLogLost(&mdfmt, seq);
      if (fon9_UNLIKELY(mdfmt.IsLastDealSent())) {
         // 最後行情已送出: 已收盤.
         if (this->TradingSessionSt_ != f9fmkt_TradingSessionSt_Closed) {
            this->OnTradingSessionClosed(kTradingSessionId);
         }
         return;
      }
      assert(dynamic_cast<OmsTwsMdSystem*>(&this->PkSys_) != nullptr);
      OmsTwsMdSystem* mdsys = static_cast<OmsTwsMdSystem*>(&this->PkSys_);
      OmsCoreSP       omsCore;
      OmsSymbSP       omssymb = mdsys->FetchOmsSymb(mdfmt.StkNo_, omsCore);
      if (!omssymb)
         return;

      SetMatchingMethod(&omssymb->TwsFlags_,
                        ((mdfmt.StatusMask_ & 0x10)
                        ? fon9::fmkt::TwsBaseFlag::MatchingMethod_ContinuousMarket
                        : fon9::fmkt::TwsBaseFlag::MatchingMethod_AggregateAuction));
      switch (mdfmt.StatusMask_ & (0x80 | 0x40 | 0x20 | 0x08 | 0x04)) {
      case 0x80 | 0x40: omssymb->TradingSessionSt_ = f9fmkt_TradingSessionSt_DelayOpen;  break;
      case 0x80 | 0x20: omssymb->TradingSessionSt_ = f9fmkt_TradingSessionSt_DelayClose; break;
      case 0x08:        omssymb->TradingSessionSt_ = f9fmkt_TradingSessionSt_Open;       break;
      case 0x04:        omssymb->TradingSessionSt_ = f9fmkt_TradingSessionSt_Closed;     break;
      }

      //const auto  dealTime = mdfmt.Time_.ToDayTime();
      //const bool  isCalc = ((mdfmt.StatusMask_ & 0x80) != 0); // 試算階段?
      const bool  hasDeal = ((mdfmt.ItemMask_ & 0x80) != 0);
      const auto* mdPQs = mdfmt.PQs_;
      if (hasDeal) {
         if (auto* symbmd = (kTradingSessionId == f9fmkt_TradingSessionId_OddLot ? omssymb->GetMdLastPriceEv_OddLot() : omssymb->GetMdLastPriceEv())) {
            omssymb->LockMd();
            const OmsMdLastPrice bfmd = *symbmd;
            symbmd->TotalQty_ = fon9::PackBcdTo<fon9::fmkt::Qty>(mdfmt.TotalQty_);
            symbmd->LastQty_ = fon9::PackBcdTo<fon9::fmkt::Qty>(mdPQs->Qty_);
            symbmd->LastPrice_.Assign<4>(fon9::PackBcdTo<uint64_t>(mdPQs->PriV4_));
            const bool isNeedEv = symbmd->IsNeedsOnMdLastPriceEv();
            omssymb->UnlockMd();
            if (isNeedEv)
               symbmd->OnMdLastPriceEv(bfmd, *omsCore);
         }
         ++mdPQs;
      }

      if (mdfmt.HasBS()) {
         if (auto* symbmd = (kTradingSessionId == f9fmkt_TradingSessionId_OddLot ? omssymb->GetMdBSEv_OddLot() : omssymb->GetMdBSEv())) {
            omssymb->LockMd();
            const OmsMdBS bfmd = *symbmd;
            // AssignBS() 在 ExgMdFmt6.hpp
            mdPQs = f9tws::AssignBS(symbmd->Buys_, mdPQs, static_cast<unsigned>((mdfmt.ItemMask_ & 0x70) >> 4));
            if (symbmd->Buy_.Qty_ && symbmd->Buy_.Pri_.IsZero())
               symbmd->Buy_.Pri_ = kMdPriBuyMarket;
            //-----
            mdPQs = f9tws::AssignBS(symbmd->Sells_, mdPQs, static_cast<unsigned>((mdfmt.ItemMask_ & 0x0e) >> 1));
            if (symbmd->Sell_.Qty_ && symbmd->Sell_.Pri_.IsZero())
               symbmd->Sell_.Pri_ = kMdPriSellMarket;
            //-----
            const bool isNeedEv = symbmd->IsNeedsOnMdBSEv();
            omssymb->UnlockMd();
            if (isNeedEv)
               symbmd->OnMdBSEv(bfmd, *omsCore);
         }
      }
   }
   void OnTradingSessionClosed(const f9fmkt_TradingSessionId kTradingSessionId) {
      (void)kTradingSessionId;
      this->TradingSessionSt_ = f9fmkt_TradingSessionSt_Closed;
      auto  core = static_cast<OmsTwsMdSystem*>(&this->PkSys_)->OmsCoreMgr_->CurrentCore();
      if (!core)
         return;
      core->RunCoreTask([this](OmsResource& coreResource) {
         auto mkt = static_cast<OmsTwsMdSystem*>(&this->PkSys_)->Market_;
         auto symbs = coreResource.Symbs_->SymbMap_.Lock();
         for (const auto& isymb : *symbs) {
            if (auto* symb = static_cast<OmsSymb*>(isymb.second.get())) {
               if (symb->TradingMarket_ == mkt) {
                  symb->TradingSessionSt_ = f9fmkt_TradingSessionSt_Closed;
                  symb->OnTradingSessionClosed(coreResource);
               }
            }
         }
      });
   }
public:
   using base::base;
   virtual ~OmsTwsMdFmtRtHandler() {
   }
};
class OmsTwsMdFmt6v4Handler : public OmsTwsMdFmtRtHandler {
   fon9_NON_COPY_NON_MOVE(OmsTwsMdFmt6v4Handler);
   using base = OmsTwsMdFmtRtHandler;
   void PkContOnReceived(const void* pkptr, unsigned pksz, SeqT seq) override {
      this->OnMdFmtRt_Received<f9fmkt_TradingSessionId_Normal>(*static_cast<const f9tws::ExgMdFmt6v4*>(pkptr), pksz, seq);
   }
public:
   using base::base;
};
class OmsTwsMdFmt23v1Handler : public OmsTwsMdFmtRtHandler {
   fon9_NON_COPY_NON_MOVE(OmsTwsMdFmt23v1Handler);
   using base = OmsTwsMdFmtRtHandler;
   void PkContOnReceived(const void* pkptr, unsigned pksz, SeqT seq) override {
      this->OnMdFmtRt_Received<f9fmkt_TradingSessionId_OddLot>(*static_cast<const f9tws::ExgMdFmt23v1*>(pkptr), pksz, seq);
   }
public:
   using base::base;
};
//--------------------------------------------------------------------------//
// OmsCore=omstws|Io="..."|Add=Market,DataInSch
// Market: T=上市; O=櫃買;
// 例: OmsCore=omstws|Io=/MaIo|Add=T,064500-180000|Add=O,064500-180000
//     Io="ThreadCount=2|Capacity=10|Cpus=1"
//     Io 設定必須在 Add 之前; 若 Add [之間] 沒有設定 Io, 則[後面]的 Add 使用[前面]的 IoServiceSrc_;
bool OmsTwsMdSystem::StartupPlugins(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
   fon9::IoManagerArgs  iomArgs;
   iomArgs.DeviceFactoryPark_ = fon9::seed::FetchNamedPark<fon9::DeviceFactoryPark>(*holder.Root_, "FpDevice");
   
   OmsCoreMgrSP   omsCoreMgr;
   fon9::StrView  tag, value;
   while (SbrFetchTagValue(args, tag, value)) {
      if (fon9::iequals(tag, "OmsCore")) {
         if ((omsCoreMgr = holder.Root_->GetSapling<OmsCoreMgr>(value)).get() == nullptr) {
            holder.SetPluginsSt(fon9::LogLevel::Error, "Not found OmsCore: ", value);
            return false;
         }
      }
      else if (fon9::iequals(tag, "Io")) {
         iomArgs.SetIoServiceCfg(holder, value);
      }
      else if (fon9::iequals(tag, "Add")) {
         if (!omsCoreMgr) {
            holder.SetPluginsSt(fon9::LogLevel::Error, "Unknwon OmsCore");
            return false;
         }
         f9fmkt_TradingMarket txMarketId{static_cast<f9fmkt_TradingMarket>(value.Get1st())};
         if ((txMarketId != f9fmkt_TradingMarket_TwSEC) && (txMarketId != f9fmkt_TradingMarket_TwOTC)) {
            holder.SetPluginsSt(fon9::LogLevel::Error, "Unknwon Add=T or O: T=TSEC, O=OTC.");
            return false;
         }
         std::string name{"OmsTwsMdSystem_"};
         name.push_back(*value.begin()); // "OmsTwsMdSystem_T" or "OmsTwsMdSystem_O"
         OmsTwsMdSystem* mdsys = new OmsTwsMdSystem(txMarketId, omsCoreMgr, holder.Root_, std::move(name));
         if (!omsCoreMgr->Add(mdsys)) {
            holder.SetPluginsSt(fon9::LogLevel::Error, "Cannot Add=", value);
            return false;
         }
         fon9::StrFetchTrim(value, ',');
         value = fon9::StrTrimRemoveQuotes(value);
         mdsys->SetNoDataEventSch(value);

         mdsys->RegMdMessageHandler<f9tws::ExgMdMarket_TwTSE,  6, 4>(f9tws::ExgMdHandlerSP{new OmsTwsMdFmt6v4Handler{*mdsys}});
         mdsys->RegMdMessageHandler<f9tws::ExgMdMarket_TwOTC,  6, 4>(f9tws::ExgMdHandlerSP{new OmsTwsMdFmt6v4Handler{*mdsys}});
         mdsys->RegMdMessageHandler<f9tws::ExgMdMarket_TwTSE, 17, 4>(f9tws::ExgMdHandlerSP{new OmsTwsMdFmt6v4Handler{*mdsys}});
         mdsys->RegMdMessageHandler<f9tws::ExgMdMarket_TwOTC, 17, 4>(f9tws::ExgMdHandlerSP{new OmsTwsMdFmt6v4Handler{*mdsys}});
         mdsys->RegMdMessageHandler<f9tws::ExgMdMarket_TwTSE, 23, 4>(f9tws::ExgMdHandlerSP{new OmsTwsMdFmt23v1Handler{*mdsys}});
         mdsys->RegMdMessageHandler<f9tws::ExgMdMarket_TwOTC, 23, 4>(f9tws::ExgMdHandlerSP{new OmsTwsMdFmt23v1Handler{*mdsys}});

         iomArgs.SessionFactoryPark_ = fon9::seed::FetchNamedPark<fon9::SessionFactoryPark>(*mdsys->Sapling_, "FpSession");
         iomArgs.SessionFactoryPark_->Add(new f9tws::ExgMdReceiverFactory(mdsys, "MdReceiver"));
         iomArgs.Name_ = mdsys->Name_ + "_io";
         iomArgs.CfgFileName_ = fon9::RevPrintTo<std::string>(SysEnv_GetConfigPath(*holder.Root_), iomArgs.Name_, ".f9gv");
         fon9::IoManagerTree::Plant(*mdsys->Sapling_, iomArgs);
         mdsys->StartupMdSystem();
      }
      else {
         holder.SetPluginsSt(fon9::LogLevel::Error, "Unknown tag:", tag);
         return false;
      }
   }
   return true;
}

} // namespaces
//--------------------------------------------------------------------------//
extern "C" fon9::seed::PluginsDesc  f9p_OmsTwsMdSystem;
static fon9::seed::PluginsPark      f9pRegister{"OmsTwsMdSystem", &f9p_OmsTwsMdSystem};
fon9::seed::PluginsDesc             f9p_OmsTwsMdSystem{"",
   &f9omstw::OmsTwsMdSystem::StartupPlugins,
   nullptr,
   nullptr,
};
