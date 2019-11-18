// \file f9utw/UtwOmsCore.cpp
// \author fonwinz@gmail.com
#include "f9utw/UnitTestCore.hpp"
#include "f9omstw/OmsCoreMgrSeed.hpp"

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
   f9tws::BrkId   BrkIdStart_;
   unsigned       BrkCount_;

   using TwsTradingLineMgrG1SP = std::unique_ptr<TwsTradingLineMgrG1>;
   TwsTradingLineMgrG1SP TwsLineMgrG1_;

   using TwfTradingLineMgrG1SP = std::unique_ptr<TwfTradingLineMgrG1>;
   TwfTradingLineMgrG1SP TwfLineMgrG1_;

   UtwOmsCoreMgrSeed(std::string name, fon9::seed::MaTreeSP owner)
      : base(std::move(name), std::move(owner), &OmsSetRequestLgOut_UseIvac) {
   }

   void InitCoreTables(OmsResource& res) override {
      res.Symbs_.reset(new OmsSymbTree(res.Core_, UtwsSymb::MakeLayout(OmsSymbTree::DefaultTreeFlag()), &UtwsSymb::SymbMaker));

      OmsBrkTree::FnGetBrkIndex fnGetBrkIdx = &OmsBrkTree::TwsBrkIndex1;
      if (fon9::Alpha2Seq(*(this->BrkIdStart_.end() - 1)) + this->BrkCount_ - 1 >= fon9::kSeq2AlphaSize)
         fnGetBrkIdx = &OmsBrkTree::TwsBrkIndex2;
      res.Brks_.reset(new OmsBrkTree(res.Core_, UtwsBrk::MakeLayout(OmsBrkTree::DefaultTreeFlag()), fnGetBrkIdx));
      res.Brks_->Initialize(&UtwsBrk::BrkMaker, ToStrView(this->BrkIdStart_), this->BrkCount_, &f9omstw_IncStrAlpha);
      // 建立委託書號表的關聯.
      res.Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwSEC);
      res.Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwOTC);
      res.Brks_->InitializeTwfOrdNoMap(f9fmkt_TradingMarket_TwFUT);
      res.Brks_->InitializeTwfOrdNoMap(f9fmkt_TradingMarket_TwOPT);
   }

public:
   static bool Create(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
      #define kCSTR_DevFpName    "FpDevice"
      auto devfp = fon9::seed::FetchNamedPark<fon9::DeviceFactoryPark>(*holder.Root_, kCSTR_DevFpName);
      if (!devfp) { // 無法取得系統共用的 DeviceFactoryPark.
         holder.SetPluginsSt(fon9::LogLevel::Error, "UtwOmsCore.Create|err=DeviceFactoryPark not found: " kCSTR_DevFpName);
         return false;
      }
      // args:
      // - IoTse={上市交易線路 IoService 參數} 可使用 IoTse=/MaIo 表示與 /MaIo 共用 IoService
      // - IoOtc={上櫃交易線路 IoService 參數} 可使用 IoOtc=/MaIo 或 IoTse 表示共用 IoService
      // - 「IoService 設定參數」請參考: IoServiceArgs.hpp
      // - BrkId=起始券商代號
      // - BrkCount=券商數量
      // - Wait=Policy     Busy or Block(default)
      // - Cpu=CpuId
      fon9::IoManagerArgs  ioargsTse{"UtwSEC"}, ioargsOtc{"UtwOTC"};
      fon9::StrView        tag, value, omsName{"omstw"}, brkId, lgCfgFileName;
      unsigned             brkCount = 0;
      fon9::HowWait        howWait{};
      int                  cpuId = -1;
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
         else if (tag == "BrkId")
            brkId = value;
         else if (tag == "BrkCount")
            brkCount = fon9::StrTo(value, 0u);
         else if (tag == "Wait")
            howWait = fon9::StrToHowWait(value);
         else if (tag == "Cpu")
            cpuId = fon9::StrTo(value, cpuId);
         else if (tag == "Lg")
            lgCfgFileName = value;
         else {
            holder.SetPluginsSt(fon9::LogLevel::Error, "UtwOmsCore.Create|err=Unknown tag: ", tag);
            return false;
         }
      }
      if (brkId.size() != sizeof(f9tws::BrkId)) {
         holder.SetPluginsSt(fon9::LogLevel::Error, "UtwOmsCore.Create|err=Unknown BrkId: ", brkId);
         return false;
      }
      ioargsTse.DeviceFactoryPark_ = ioargsOtc.DeviceFactoryPark_ = devfp;
      // ------------------------------------------------------------------
      UtwOmsCoreMgrSeed* coreMgrSeed = new UtwOmsCoreMgrSeed(omsName.ToString(), holder.Root_);
      if (!holder.Root_->Add(coreMgrSeed))
         return false;
      coreMgrSeed->BrkIdStart_.AssignFrom(brkId);
      coreMgrSeed->BrkCount_ = (brkCount <= 0 ? 1u : brkCount);
      coreMgrSeed->CpuId_ = cpuId;
      coreMgrSeed->HowWait_ = howWait;
      // ------------------------------------------------------------------
      OmsCoreMgr&           coreMgr = coreMgrSeed->GetOmsCoreMgr();
      UomsTwsOrderFactory*  twsOrdFactory = new UomsTwsOrderFactory{"TwsOrd"};
      OmsTwsReportFactorySP twsRptFactory = new OmsTwsReportFactory("TwsRpt", twsOrdFactory);
      OmsTwsFilledFactorySP twsFilFactory = new OmsTwsFilledFactory("TwsFil", twsOrdFactory);

      UomsTwfOrder1Factory*  twfOrd1Factory = new UomsTwfOrder1Factory{"TwfOrd"};
      UomsTwfOrder9Factory*  twfOrd9Factory = new UomsTwfOrder9Factory{"TwfOrdQ"};
      OmsTwfReport2FactorySP twfRpt1Factory = new OmsTwfReport2Factory("TwfRpt", twfOrd1Factory);
      OmsTwfFilled1FactorySP twfFil1Factory = new OmsTwfFilled1Factory("TwfFil", twfOrd1Factory, twfOrd9Factory);
      OmsTwfFilled2FactorySP twfFil2Factory = new OmsTwfFilled2Factory("TwfFil2", twfOrd1Factory);
      UomsTwfOrder7Factory*  twfOrd7Factory = new UomsTwfOrder7Factory{"TwfOrdQR"};
      OmsTwfReport8FactorySP twfRpt8Factory = new OmsTwfReport8Factory("TwfRptQR", twfOrd7Factory);
      OmsTwfReport9FactorySP twfRpt9Factory = new OmsTwfReport9Factory("TwfRptQ", twfOrd9Factory);
      // ------------------------------------------------------------------
      ioargsTse.SessionFactoryPark_ = ioargsOtc.SessionFactoryPark_
         = fon9::seed::FetchNamedPark<fon9::SessionFactoryPark>(coreMgr, "FpSession");
      const std::string logpath = fon9::seed::SysEnv_GetLogFileFmtPath(*coreMgrSeed->Root_);
      ioargsTse.SessionFactoryPark_->Add(
         new TwsTradingLineFixFactory(coreMgr, *twsRptFactory, *twsFilFactory,
                                      logpath, fon9::Named{"FIX44"}));
      // 預計新版 TMP 下單, Named 使用 "TMP2"
      ioargsTse.SessionFactoryPark_->Add(
         new TwsTradingLineTmpFactory2019(coreMgr, *twsRptFactory, *twsFilFactory,
                                          logpath, fon9::Named{"TMP"}));
      // ------------------------------------------------------------------
      const std::string     cfgpath = fon9::seed::SysEnv_GetConfigPath(*coreMgrSeed->Root_).ToString();
      TwsTradingLineMgrLgSP lgMgr;
      if (!lgCfgFileName.IsNullOrEmpty()) {
         fon9::IoManagerArgs  iocfgs{"LgMgr"};
         iocfgs.CfgFileName_ = lgCfgFileName.ToString(&cfgpath);
         iocfgs.SessionFactoryPark_ = ioargsTse.SessionFactoryPark_;
         iocfgs.DeviceFactoryPark_ = ioargsTse.DeviceFactoryPark_;
         lgMgr = TwsTradingLineMgrLg::Plant(coreMgr, holder, iocfgs);
      }
      OmsRequestRunStepSP  twsNewSenderStep, twsChgSenderStep;
      if (lgMgr) {
         twsNewSenderStep.reset(new OmsTwsSenderStepLg{*lgMgr});
         twsChgSenderStep.reset(new OmsTwsSenderStepLg{*lgMgr});
      }
      else {
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
      // ------------------------------------------------------------------
      // 測試 TaiFex-TMP 連線.
      fon9::seed::MaTreeSP twfTestTree{new fon9::seed::MaTree{"x"}};
      holder.Root_->Add(new fon9::seed::NamedSapling(twfTestTree, "TaiFex_TEST"));
      // ------------------
      // TaiFex 一般設定: (1)匯入P06,P07; (2)匯入P08; ...
      using namespace fon9::seed;
      fon9::seed::MaConfigMgrSP  twfCfgMgr{new MaConfigMgr(MaConfigSeed::MakeLayout("cfgs", TabFlag::Writable),
                                                           "ExgConfig")};
      f9twf::ExgMapMgrSP         twfMapMgr{new TwfExgMapMgr(coreMgr, twfCfgMgr->GetConfigSapling(), "ExgImporter")};
      twfCfgMgr->GetConfigSapling().Add(twfMapMgr);
      twfCfgMgr->BindConfigFile(cfgpath + "UtwFexCfg.f9gv", true);
      twfMapMgr->BindConfigFile(cfgpath + "UtwFexImp.f9gv", true);
      twfTestTree->Add(twfCfgMgr);
      // ------------------
      fon9::IoManagerArgs ioargsFutNormal{"UtwFutNormal"};
      ioargsFutNormal.DeviceFactoryPark_  = devfp;
      ioargsFutNormal.SessionFactoryPark_ = fon9::seed::FetchNamedPark<fon9::SessionFactoryPark>(*twfTestTree, "FpTwfSession");
      ioargsFutNormal.SessionFactoryPark_->Add(new TwfLineTmpFactory(
         logpath, fon9::Named{"TWF"}, coreMgr,
         *twfRpt1Factory, *twfRpt8Factory, *twfRpt9Factory, *twfFil1Factory, *twfFil2Factory));
      fon9::IoManagerArgs ioargsFutAfterHour{"UtwFutAfterHour"};
      fon9::IoManagerArgs ioargsOptNormal{"UtwOptNormal"};
      fon9::IoManagerArgs ioargsOptAfterHour{"UtwOptAfterHour"};
      ioargsFutAfterHour.DeviceFactoryPark_ = ioargsOptNormal.DeviceFactoryPark_ = ioargsOptAfterHour.DeviceFactoryPark_ = devfp;
      ioargsFutAfterHour.SessionFactoryPark_ = ioargsOptNormal.SessionFactoryPark_ = ioargsOptAfterHour.SessionFactoryPark_ = ioargsFutNormal.SessionFactoryPark_;
      // ------------------
      fon9::seed::MaTreeSP twfG1Mgr{new fon9::seed::MaTree{"TwfLineMgr"}};
      twfTestTree->Add(new fon9::seed::NamedSapling(twfG1Mgr, "LineMgrG1"));
      coreMgrSeed->TwfLineMgrG1_.reset(new TwfTradingLineMgrG1{coreMgr});
      // ---
      #define CREATE_TwfTradingLineMgr(type) \
      coreMgrSeed->TwfLineMgrG1_->TradingLineMgr_[ExgSystemTypeToIndex(f9twf::ExgSystemType::type)] \
         = CreateTwfTradingLineMgr(*twfG1Mgr, cfgpath, ioargs##type, twfMapMgr, f9twf::ExgSystemType::type)
      CREATE_TwfTradingLineMgr(OptNormal);
      CREATE_TwfTradingLineMgr(OptAfterHour);
      CREATE_TwfTradingLineMgr(FutNormal);
      CREATE_TwfTradingLineMgr(FutAfterHour);
      for (auto& lmgr : coreMgrSeed->TwfLineMgrG1_->TradingLineMgr_)
         lmgr->StartTimerForOpen(fon9::TimeInterval_Second(1));
      // ------------------
      OmsRequestRunStepSP  twfNewSenderStep, twfChgSenderStep, twfNewQRSenderStep, twfNewQSenderStep;
      twfNewSenderStep.reset(new OmsTwfSenderStepG1{*coreMgrSeed->TwfLineMgrG1_});
      twfChgSenderStep.reset(new OmsTwfSenderStepG1{*coreMgrSeed->TwfLineMgrG1_});
      twfNewQRSenderStep.reset(new OmsTwfSenderStepG1{*coreMgrSeed->TwfLineMgrG1_});
      twfNewQSenderStep.reset(new OmsTwfSenderStepG1{*coreMgrSeed->TwfLineMgrG1_});
      // ------------------------------------------------------------------
      coreMgr.SetEventFactoryPark(new f9omstw::OmsEventFactoryPark{});
      coreMgr.SetOrderFactoryPark(new OmsOrderFactoryPark{twsOrdFactory, twfOrd1Factory, twfOrd7Factory});
      coreMgr.SetRequestFactoryPark(new f9omstw::OmsRequestFactoryPark(
         new OmsTwsRequestIniFactory("TwsNew", twsOrdFactory,
                                     OmsRequestRunStepSP{new UomsTwsIniRiskCheck(std::move(twsNewSenderStep))}),
         new OmsTwsRequestChgFactory("TwsChg", std::move(twsChgSenderStep)),
         twsRptFactory, twsFilFactory,

         new OmsTwfRequestIni1Factory("TwfNew", twfOrd1Factory,
                                      OmsRequestRunStepSP{new UomsTwfIniRiskCheck(std::move(twfNewSenderStep))}),
         new OmsTwfRequestChg1Factory("TwfChg", std::move(twfChgSenderStep)),
         twfRpt1Factory, twfFil1Factory, twfFil2Factory,

         new OmsTwfRequestIni7Factory("TwfNewQR", twfOrd7Factory, std::move(twfNewQRSenderStep)),
         twfRpt8Factory,

         new OmsTwfRequestIni9Factory("TwfNewQ", twfOrd9Factory, std::move(twfNewQSenderStep)),
         twfRpt9Factory
         ));
      coreMgr.ReloadErrCodeAct(fon9::ToStrView(cfgpath + "UtwErrCodeAct.cfg"));
      coreMgrSeed->AddCore(fon9::UtcNow() + fon9::GetLocalTimeZoneOffset());
      twfMapMgr->SetTDay(coreMgr.CurrentCore()->TDay());
      // ------------------------------------------------------------------
      return true;
   }
};

} // namespaces
//--------------------------------------------------------------------------//
extern "C" fon9::seed::PluginsDesc f9p_UtwOmsCore;
fon9::seed::PluginsDesc f9p_UtwOmsCore{"", &f9omstw::UtwOmsCoreMgrSeed::Create, nullptr, nullptr,};
static fon9::seed::PluginsPark f9pRegister{"UtwOmsCore", &f9p_UtwOmsCore};
