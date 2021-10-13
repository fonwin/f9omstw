// \file f9utw/UtwOmsCore.cpp
// \author fonwinz@gmail.com
#include "f9utw/UnitTestCore.hpp"
#include "f9omstw/OmsCoreMgrSeed.hpp"
#include "f9omstw/OmsEventSessionSt.hpp"

#include "f9omstws/OmsTwsSenderStepG1.hpp"
#include "f9omstws/OmsTwsSenderStepLg.hpp"
#include "f9omstws/OmsTwsTradingLineFix.hpp"
#include "f9omstws/OmsTwsTradingLineTmp2019.hpp"
// 
#include "f9omstwf/OmsTwfSenderStepG1.hpp"
#include "f9omstwf/OmsTwfLineTmpFactory.hpp"
#include "f9omstwf/OmsTwfExgMapMgr.hpp"

#include "fon9/seed/SysEnv.hpp"

namespace f9omstw {

using UomsTwsOrderFactory = OmsOrderFactoryT<OmsOrder, OmsTwsOrderRaw, kPoolObjCount>;
using OmsTwsRequestIniFactory = OmsRequestFactoryT<OmsTwsRequestIni, kPoolObjCount>;
using OmsTwsRequestChgFactory = OmsRequestFactoryT<OmsTwsRequestChg, kPoolObjCount>;
//--------------------------------------------------------------------------//
using UomsTwfOrder1Factory = OmsOrderFactoryT<OmsTwfOrder1, OmsTwfOrderRaw1, kPoolObjCount>;
using OmsTwfRequestIni1Factory = OmsRequestFactoryT<OmsTwfRequestIni1, kPoolObjCount>;
using OmsTwfRequestChg1Factory = OmsRequestFactoryT<OmsTwfRequestChg1, kPoolObjCount>;

using UomsTwfOrder7Factory = OmsOrderFactoryT<OmsTwfOrder7, OmsTwfOrderRaw7, 0>;
using OmsTwfRequestIni7Factory = OmsRequestFactoryT<OmsTwfRequestIni7, 0>;

using UomsTwfOrder9Factory = OmsOrderFactoryT<OmsTwfOrder9, OmsTwfOrderRaw9, kPoolObjCount>;
using OmsTwfRequestIni9Factory = OmsRequestFactoryT<OmsTwfRequestIni9, kPoolObjCount>;
//--------------------------------------------------------------------------//
class UtwOmsCoreMgrSeed : public OmsCoreMgrSeed {
   fon9_NON_COPY_NON_MOVE(UtwOmsCoreMgrSeed);
   using base = OmsCoreMgrSeed;
   fon9::CharVector  BrkIdStart_;
   unsigned          BrkCount_;
   f9twf::FcmId      CmId_;
   char              Padding___[2];

   using TwsTradingLineMgrG1SP = std::unique_ptr<TwsTradingLineMgrG1>;
   TwsTradingLineMgrG1SP TwsLineMgrG1_;

   using TwfTradingLineMgrG1SP = std::unique_ptr<TwfTradingLineMgrG1>;
   TwfTradingLineMgrG1SP TwfLineMgrG1_;

   UtwOmsCoreMgrSeed(std::string name, fon9::seed::MaTreeSP owner)
      : base(std::move(name), std::move(owner), &OmsSetRequestLgOut_UseIvac) {
   }

   void InitCoreTables(OmsResource& res) override {
      res.Symbs_.reset(new OmsSymbTree(res.Core_, UtwsSymb::MakeLayout(OmsSymbTree::DefaultTreeFlag()), &UtwsSymb::SymbMaker));

      OmsBrkTree::FnGetBrkIndex fnGetBrkIdx;
      if (this->BrkIdStart_.size() == sizeof(f9twf::BrkId)) {
         fnGetBrkIdx = &OmsBrkTree::TwfBrkIndex;
      }
      else {
         fnGetBrkIdx = &OmsBrkTree::TwsBrkIndex1;
         if (fon9::Alpha2Seq(*(this->BrkIdStart_.end() - 1)) + this->BrkCount_ - 1 >= fon9::kSeq2AlphaSize)
            fnGetBrkIdx = &OmsBrkTree::TwsBrkIndex2;
      }
      res.Brks_.reset(new OmsBrkTree(res.Core_, UtwsBrk::MakeLayout(OmsBrkTree::DefaultTreeFlag()), fnGetBrkIdx));
      res.Brks_->Initialize(&UtwsBrk::BrkMaker, ToStrView(this->BrkIdStart_), this->BrkCount_, &f9omstw_IncStrAlpha);
      for (size_t L = 0; L < this->BrkCount_; ++L)
         if (auto* brk = res.Brks_->GetBrkRec(L)) {
            brk->CmId_ = this->CmId_;
            brk->FcmId_ = this->CmId_; // 當 P06 載入後再調整.
         }
      // 建立委託書號表的關聯.
      res.Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwSEC);
      res.Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwOTC);
      res.Brks_->InitializeTwfOrdNoMap(f9fmkt_TradingMarket_TwFUT);
      res.Brks_->InitializeTwfOrdNoMap(f9fmkt_TradingMarket_TwOPT);
   }

public:
   static bool Create(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
      using namespace fon9::seed;

      #define kCSTR_DevFpName    "FpDevice"
      auto devfp = FetchNamedPark<fon9::DeviceFactoryPark>(*holder.Root_, kCSTR_DevFpName);
      if (!devfp) { // 無法取得系統共用的 DeviceFactoryPark.
         holder.SetPluginsSt(fon9::LogLevel::Error, "UtwOmsCore.Create|err=DeviceFactoryPark not found: " kCSTR_DevFpName);
         return false;
      }
      // args:
      // - 使用線路群組:
      //   - Lg=線路群組設定檔.
      // - 不使用線路群組:
      //   - IoTse={上市交易線路 IoService 參數} 可使用 IoTse=/MaIo 表示與 /MaIo 共用 IoService
      //   - IoOtc={上櫃交易線路 IoService 參數} 可使用 IoOtc=/MaIo 或 IoTse 表示共用 IoService
      // - 「IoService 設定參數」請參考: IoServiceArgs.hpp
      // - BrkId=起始券商代號
      // - BrkCount=券商數量
      // - Wait=Policy     Busy or Block(default)
      // - Cpu=CpuId
      fon9::IoManagerArgs  ioargsTse{"UtwSEC"};
      fon9::IoManagerArgs  ioargsOtc{"UtwOTC"};
      fon9::IoManagerArgs  ioargsFutNormal{"UtwFutNormal"};
      fon9::IoManagerArgs  ioargsOptNormal{"UtwOptNormal"};
      fon9::IoManagerArgs  ioargsFutAfterHour{"UtwFutAfterHour"};
      fon9::IoManagerArgs  ioargsOptAfterHour{"UtwOptAfterHour"};
      fon9::StrView        tag, value, omsName{"omstw"}, brkId, lgCfgFileName;
      unsigned             brkCount = 0;
      fon9::HowWait        howWait{};
      int                  cpuId = -1;
      f9twf::FcmId         cmId{};
      fon9::TimeInterval   flushInterval{fon9::TimeInterval_Millisecond(1)};
      while (fon9::SbrFetchTagValue(args, tag, value)) {
         fon9::IoManagerArgs* ioargs;
         if (tag == "Name")
            omsName = value;
         else if (tag == "IoTse") {
            ioargs = &ioargsTse;
         __SET_IO_ARGS:;
            if (!ioargs->SetIoServiceCfg(holder, value))
               return false;
         }
         else if (tag == "IoOtc") {
            ioargs = &ioargsOtc;
            goto __SET_IO_ARGS;
         }
         else if (tag == "IoFut") {
            ioargs = &ioargsFutNormal;
            goto __SET_IO_ARGS;
         }
         else if (tag == "IoOpt") {
            ioargs = &ioargsOptNormal;
            goto __SET_IO_ARGS;
         }
         else if (tag == "IoFutA") {
            ioargs = &ioargsFutAfterHour;
            goto __SET_IO_ARGS;
         }
         else if (tag == "IoOptA") {
            ioargs = &ioargsOptAfterHour;
            goto __SET_IO_ARGS;
         }
         else if (tag == "BrkId")
            brkId = value;
         else if (tag == "BrkCount")
            brkCount = fon9::StrTo(value, 0u);
         else if (tag == "Wait")
            howWait = fon9::StrToHowWait(value);
         else if (tag == "Cpu")
            cpuId = fon9::StrTo(value, cpuId);
         else if (tag == "FlushInterval")
            flushInterval = fon9::StrTo(value, flushInterval);
         else if (tag == "Lg")
            lgCfgFileName = value;
         else if (tag == "CmId")
            cmId = fon9::StrTo(value, cmId);
         else {
            holder.SetPluginsSt(fon9::LogLevel::Error, "UtwOmsCore.Create|err=Unknown tag: ", tag);
            return false;
         }
      }
      const bool isTwsSys = brkId.size() == sizeof(f9tws::BrkId);
      if (!isTwsSys && brkId.size() != sizeof(f9twf::BrkId)) {
         holder.SetPluginsSt(fon9::LogLevel::Error, "UtwOmsCore.Create|err=Unknown BrkId: ", brkId);
         return false;
      }
      // ------------------------------------------------------------------
      UtwOmsCoreMgrSeed* coreMgrSeed = new UtwOmsCoreMgrSeed(omsName.ToString(), holder.Root_);
      if (!holder.Root_->Add(coreMgrSeed))
         return false;
      coreMgrSeed->BrkIdStart_.assign(brkId);
      coreMgrSeed->BrkCount_      = (brkCount <= 0 ? 1u : brkCount);
      coreMgrSeed->CmId_          = cmId;
      coreMgrSeed->CpuId_         = cpuId;
      coreMgrSeed->FlushInterval_ = flushInterval;
      coreMgrSeed->HowWait_       = howWait;
      // ------------------------------------------------------------------
      const std::string     logpath = SysEnv_GetLogFileFmtPath(*coreMgrSeed->Root_);
      const std::string     cfgpath = SysEnv_GetConfigPath(*coreMgrSeed->Root_).ToString();
      OmsCoreMgr&           coreMgr = coreMgrSeed->GetOmsCoreMgr();
      coreMgr.SetEventFactoryPark(new f9omstw::OmsEventFactoryPark(
         new OmsEventInfoFactory(),
         new OmsEventSessionStFactory()
      ));
      // ------------------------------------------------------------------
      if (isTwsSys) {
         UomsTwsOrderFactory*  twsOrdFactory = new UomsTwsOrderFactory{"TwsOrd"};
         OmsTwsReportFactorySP twsRptFactory = new OmsTwsReportFactory("TwsRpt", twsOrdFactory);
         OmsTwsFilledFactorySP twsFilFactory = new OmsTwsFilledFactory("TwsFil", twsOrdFactory);
         ioargsTse.DeviceFactoryPark_ = ioargsOtc.DeviceFactoryPark_ = devfp;
         ioargsTse.SessionFactoryPark_ = ioargsOtc.SessionFactoryPark_
            = FetchNamedPark<fon9::SessionFactoryPark>(coreMgr, "TwsFpSession");
         ioargsTse.SessionFactoryPark_->Add(
            new TwsTradingLineFixFactory(coreMgr, *twsRptFactory, *twsFilFactory,
                                         logpath, fon9::Named{"FIX44"}));
         // 預計新版 TMP 下單, Named 使用 "TMP2"
         // ioargsTse.SessionFactoryPark_->Add(
         //    new TwsTradingLineTmpFactory2019(coreMgr, *twsRptFactory, *twsFilFactory,
         //                                     logpath, fon9::Named{"TMP"}));
         OmsRequestRunStepSP  twsNewSenderStep, twsChgSenderStep;
         if (lgCfgFileName.IsNullOrEmpty()) {
            coreMgrSeed->TwsLineMgrG1_.reset(new TwsTradingLineMgrG1{coreMgr});
            coreMgrSeed->TwsLineMgrG1_->TseTradingLineMgr_
               = CreateTwsTradingLineMgr(coreMgr, cfgpath, ioargsTse, f9fmkt_TradingMarket_TwSEC);
            // ------------------
            if (ioargsOtc.IoServiceCfgstr_ == "IoTse")
               ioargsOtc.IoServiceSrc_ = coreMgrSeed->TwsLineMgrG1_->TseTradingLineMgr_;
            coreMgrSeed->TwsLineMgrG1_->OtcTradingLineMgr_
               = CreateTwsTradingLineMgr(coreMgr, cfgpath, ioargsOtc, f9fmkt_TradingMarket_TwOTC);
            // ------------------
            twsNewSenderStep.reset(new OmsTwsSenderStepG1{*coreMgrSeed->TwsLineMgrG1_});
            twsChgSenderStep.reset(new OmsTwsSenderStepG1{*coreMgrSeed->TwsLineMgrG1_});
         }
         else { // 在 lg 設定檔裡面設定 ioargs.
            TwsTradingLineMgrLgSP lgMgr;
            fon9::IoManagerArgs   iocfgs{"LgMgr"};
            iocfgs.CfgFileName_ = lgCfgFileName.ToString(&cfgpath);
            iocfgs.SessionFactoryPark_ = ioargsTse.SessionFactoryPark_;
            iocfgs.DeviceFactoryPark_ = ioargsTse.DeviceFactoryPark_;
            lgMgr = TwsTradingLineMgrLg::Plant(coreMgr, holder, iocfgs);
            twsNewSenderStep.reset(new OmsTwsSenderStepLg{*lgMgr});
            twsChgSenderStep.reset(new OmsTwsSenderStepLg{*lgMgr});
         }
         coreMgr.SetOrderFactoryPark(new OmsOrderFactoryPark{twsOrdFactory});
         coreMgr.SetRequestFactoryPark(new f9omstw::OmsRequestFactoryPark(
            new OmsTwsRequestIniFactory("TwsNew", twsOrdFactory,
                                        OmsRequestRunStepSP{new UomsTwsIniRiskCheck(std::move(twsNewSenderStep))}),
            new OmsTwsRequestChgFactory("TwsChg", std::move(twsChgSenderStep)),
            twsRptFactory,
            twsFilFactory
         ));
      }
      else {
         UomsTwfOrder1Factory*  twfOrd1Factory = new UomsTwfOrder1Factory{"TwfOrd"};
         UomsTwfOrder9Factory*  twfOrd9Factory = new UomsTwfOrder9Factory{"TwfOrdQ"};
         OmsTwfReport2FactorySP twfRpt1Factory = new OmsTwfReport2Factory("TwfRpt", twfOrd1Factory);
         OmsTwfFilled1FactorySP twfFil1Factory = new OmsTwfFilled1Factory("TwfFil", twfOrd1Factory, twfOrd9Factory);
         OmsTwfFilled2FactorySP twfFil2Factory = new OmsTwfFilled2Factory("TwfFil2", twfOrd1Factory);
         UomsTwfOrder7Factory*  twfOrd7Factory = new UomsTwfOrder7Factory{"TwfOrdQR"};
         OmsTwfReport8FactorySP twfRpt8Factory = new OmsTwfReport8Factory("TwfRptQR", twfOrd7Factory);
         OmsTwfReport9FactorySP twfRpt9Factory = new OmsTwfReport9Factory("TwfRptQ", twfOrd9Factory);
         // ------------------
         // TaiFex 一般設定: (1)匯入P06,P07; (2)匯入P08; ...
         MaConfigMgrSP  twfCfgMgr{new MaConfigMgr(MaConfigSeed::MakeLayout("cfgs", TabFlag::Writable),
                                                  "TwfExgConfig")};
         f9twf::ExgMapMgrSP   twfMapMgr{new TwfExgMapMgr(coreMgr, twfCfgMgr->GetConfigSapling(), "TwfExgImporter")};
         twfCfgMgr->GetConfigSapling().Add(twfMapMgr);
         twfCfgMgr->BindConfigFile(cfgpath + "TwfExgConfig.f9gv", true);
         twfMapMgr->BindConfigFile(cfgpath + "TwfExgImporter.f9gv", true);
         coreMgr.Add(twfCfgMgr);
         coreMgr.TDayChangedEvent_.Subscribe([twfMapMgr](OmsCore& core) {
            twfMapMgr->SetTDay(core.TDay());
         });
         // ------------------
         ioargsFutNormal.DeviceFactoryPark_     = devfp;
         ioargsOptNormal.DeviceFactoryPark_     = devfp;
         ioargsFutAfterHour.DeviceFactoryPark_  = devfp;
         ioargsOptAfterHour.DeviceFactoryPark_  = devfp;
         ioargsFutNormal.SessionFactoryPark_    = FetchNamedPark<fon9::SessionFactoryPark>(coreMgr, "TwfFpSession");
         ioargsOptNormal.SessionFactoryPark_    = ioargsFutNormal.SessionFactoryPark_;
         ioargsFutAfterHour.SessionFactoryPark_ = ioargsFutNormal.SessionFactoryPark_;
         ioargsOptAfterHour.SessionFactoryPark_ = ioargsFutNormal.SessionFactoryPark_;
         ioargsFutNormal.SessionFactoryPark_->Add(new TwfLineTmpFactory(
            logpath, fon9::Named{"TWF"}, coreMgr,
            *twfRpt1Factory, *twfRpt8Factory, *twfRpt9Factory, *twfFil1Factory, *twfFil2Factory));
         // ------------------
         // if (lgCfgFileName.IsNullOrEmpty()) {
            MaTreeSP twfG1Mgr{new MaTree{"TwfLineMgr"}};
            coreMgr.Add(new NamedSapling(twfG1Mgr, "TwfLineMgr"));
            coreMgrSeed->TwfLineMgrG1_.reset(new TwfTradingLineMgrG1{coreMgr});
            // ---
            #define CREATE_TwfTradingLineMgr(type) \
            coreMgrSeed->TwfLineMgrG1_->TradingLineMgr_[ExgSystemTypeToIndex(f9twf::ExgSystemType::type)] \
               = CreateTwfTradingLineMgr(*twfG1Mgr, cfgpath, ioargs##type, twfMapMgr, f9twf::ExgSystemType::type)
            // ---
            CREATE_TwfTradingLineMgr(OptNormal);
            CREATE_TwfTradingLineMgr(OptAfterHour);
            CREATE_TwfTradingLineMgr(FutNormal);
            CREATE_TwfTradingLineMgr(FutAfterHour);
            for (auto& lmgr : coreMgrSeed->TwfLineMgrG1_->TradingLineMgr_)
               lmgr->StartTimerForOpen(fon9::TimeInterval_Second(1));
         // }
         // else {
         //    TODO: Lg 線路群組.
         // }
         // ------------------
         OmsRequestRunStepSP  twfNewSenderStep, twfChgSenderStep, twfNewQRSenderStep, twfNewQSenderStep, twfChgQSenderStep;
         twfNewSenderStep.reset(new OmsTwfSenderStepG1{*coreMgrSeed->TwfLineMgrG1_});
         twfChgSenderStep.reset(new OmsTwfSenderStepG1{*coreMgrSeed->TwfLineMgrG1_});
         twfNewQRSenderStep.reset(new OmsTwfSenderStepG1{*coreMgrSeed->TwfLineMgrG1_});
         twfNewQSenderStep.reset(new OmsTwfSenderStepG1{*coreMgrSeed->TwfLineMgrG1_});
         twfChgQSenderStep.reset(new OmsTwfSenderStepG1{*coreMgrSeed->TwfLineMgrG1_});
         coreMgr.SetOrderFactoryPark(new OmsOrderFactoryPark{twfOrd1Factory, twfOrd7Factory, twfOrd9Factory});
         coreMgr.SetRequestFactoryPark(new f9omstw::OmsRequestFactoryPark(
            new OmsTwfRequestIni1Factory("TwfNew", twfOrd1Factory,
                                         OmsRequestRunStepSP{new UomsTwfIniRiskCheck(std::move(twfNewSenderStep))}),
            new OmsTwfRequestChg1Factory("TwfChg", std::move(twfChgSenderStep)),
            twfRpt1Factory,
            twfFil1Factory,
            twfFil2Factory,
            new OmsTwfRequestIni7Factory("TwfNewQR", twfOrd7Factory, std::move(twfNewQRSenderStep)),
            twfRpt8Factory,
            new OmsTwfRequestIni9Factory("TwfNewQ", twfOrd9Factory,
                                         OmsRequestRunStepSP{new UomsTwfIni9RiskCheck(std::move(twfNewQSenderStep))}),
            new OmsTwfRequestChg9Factory("TwfChgQ", std::move(twfChgQSenderStep)),
            twfRpt9Factory
         ));
      }
      // ------------------------------------------------------------------
      coreMgr.ReloadErrCodeAct(fon9::ToStrView(cfgpath + "OmsErrCodeAct.cfg"));
      coreMgrSeed->AddCore(fon9::UtcNow() + fon9::GetLocalTimeZoneOffset());
      // ------------------------------------------------------------------
      return true;
   }
};

} // namespaces
//--------------------------------------------------------------------------//
extern "C" fon9::seed::PluginsDesc f9p_UtwOmsCore;
fon9::seed::PluginsDesc f9p_UtwOmsCore{"", &f9omstw::UtwOmsCoreMgrSeed::Create, nullptr, nullptr,};
static fon9::seed::PluginsPark f9pRegister{"UtwOmsCore", &f9p_UtwOmsCore};
