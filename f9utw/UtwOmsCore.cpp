﻿// \file f9utw/UtwOmsCore.cpp
// \author fonwinz@gmail.com
#include "f9utw/UnitTestCore.hpp"
#include "f9utw/UtwSpCmdTwf.hpp"
#include "f9utw/UtwSpCmdTws.hpp"
#include "f9omstw/OmsCoreMgrSeed.hpp"
#include "f9omstw/OmsEventSessionSt.hpp"
#include "f9omstw/OmsTradingLineHelper1.hpp"

#include "f9omstws/OmsTwsSenderStepG1.hpp"
#include "f9omstws/OmsTwsSenderStepLgMgr.hpp"
#include "f9omstws/OmsTwsTradingLineFix.hpp"
#include "f9omstws/OmsTwsTradingLineTmp2019.hpp"
// 
#include "f9omstwf/OmsTwfSenderStepG1.hpp"
#include "f9omstwf/OmsTwfSenderStepLgMgr.hpp"
#include "f9omstwf/OmsTwfLineTmpFactory.hpp"
#include "f9omstwf/OmsTwfExgMapMgr.hpp"
#include "f9omstwf/OmsTwfRptFromB50.hpp"

#include "fon9/seed/SysEnv.hpp"

namespace f9omstw {

#ifndef fon9_kCSTR_UtwOmsCoreName
#define fon9_kCSTR_UtwOmsCoreName   "UtwOmsCore"
#endif

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
   TwfExgMapMgrSP    TwfExgMapMgr_;

   TwsTradingLineG1SP TwsLineG1_;
   TwfTradingLineG1SP TwfLineG1_;

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
      // 建立特殊指令(例:特殊刪單)管理員.
      if (res.Core_.Owner_->RequestFactoryPark().GetFactory("TwfNew"))
         res.Sapling_->Add(new SpCmdMgrTwf(&res.Core_, "SpCmdTwf"));
      if (res.Core_.Owner_->RequestFactoryPark().GetFactory("TwsNew"))
         res.Sapling_->Add(new SpCmdMgrTwf(&res.Core_, "SpCmdTws"));
      if (this->TwfExgMapMgr_)
         this->TwfExgMapMgr_->OnAfter_InitCoreTables(res);
   }

public:
   static bool Create(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
      using namespace fon9::seed;

      #define kCSTR_DevFpName    "FpDevice"
      auto devfp = FetchNamedPark<fon9::DeviceFactoryPark>(*holder.Root_, kCSTR_DevFpName);
      if (!devfp) { // 無法取得系統共用的 DeviceFactoryPark.
         holder.SetPluginsSt(fon9::LogLevel::Error,
                             fon9_kCSTR_UtwOmsCoreName ".Create|err=DeviceFactoryPark not found: " kCSTR_DevFpName);
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
      int                  coreCpuId = -1, backendCpuId = -1;
      f9twf::FcmId         cmId{};
      fon9::TimeInterval   backendFlushInterval{fon9::TimeInterval_Millisecond(1)};
      int                  changeDayHHMMSS = -1; // 換日時間, 現貨 00:00 換日, 期權 6:00 換日;
      while (fon9::SbrFetchTagValue(args, tag, value)) {
         fon9::IoManagerArgs* ioargs;
         if (fon9::iequals(tag, "Name"))
            omsName = value;
         else if (fon9::iequals(tag, "IoTse")) {
            ioargs = &ioargsTse;
         __SET_IO_ARGS:;
            if (!ioargs->SetIoServiceCfg(holder, value))
               return false;
         }
         else if (fon9::iequals(tag, "IoOtc")) {
            ioargs = &ioargsOtc;
            goto __SET_IO_ARGS;
         }
         else if (fon9::iequals(tag, "IoFut")) {
            ioargs = &ioargsFutNormal;
            goto __SET_IO_ARGS;
         }
         else if (fon9::iequals(tag, "IoOpt")) {
            ioargs = &ioargsOptNormal;
            goto __SET_IO_ARGS;
         }
         else if (fon9::iequals(tag, "IoFutA")) {
            ioargs = &ioargsFutAfterHour;
            goto __SET_IO_ARGS;
         }
         else if (fon9::iequals(tag, "IoOptA")) {
            ioargs = &ioargsOptAfterHour;
            goto __SET_IO_ARGS;
         }
         else if (fon9::iequals(tag, "BrkId"))
            brkId = value;
         else if (fon9::iequals(tag, "BrkCount"))
            brkCount = fon9::StrTo(value, 0u);
         else if (fon9::iequals(tag, "Wait"))
            howWait = fon9::StrToHowWait(value);
         else if (fon9::iequals(tag, "Cpu"))
            coreCpuId = fon9::StrTo(value, coreCpuId);
         else if (fon9::iequals(tag, "BackendCpu"))
            backendCpuId = fon9::StrTo(value, backendCpuId);
         else if (fon9::iequals(tag, "FlushInterval"))
            backendFlushInterval = fon9::StrTo(value, backendFlushInterval);
         else if (fon9::iequals(tag, "Lg"))
            lgCfgFileName = value;
         else if (fon9::iequals(tag, "CmId"))
            cmId = fon9::StrTo(value, cmId);
         else if (fon9::iequals(tag, "ChgTDay"))
            changeDayHHMMSS = fon9::StrTo(value, changeDayHHMMSS);
         else {
            holder.SetPluginsSt(fon9::LogLevel::Error, fon9_kCSTR_UtwOmsCoreName ".Create|err=Unknown tag: ", tag);
            return false;
         }
      }
      const bool isTwsSys = brkId.size() == sizeof(f9tws::BrkId);
      if (!isTwsSys && brkId.size() != sizeof(f9twf::BrkId)) {
         holder.SetPluginsSt(fon9::LogLevel::Error, fon9_kCSTR_UtwOmsCoreName ".Create|err=Unknown BrkId: ", brkId);
         return false;
      }
      if (changeDayHHMMSS < 0)
         changeDayHHMMSS = (isTwsSys ? 0 : 60000);
      // ------------------------------------------------------------------
      UtwOmsCoreMgrSeed* coreMgrSeed = new UtwOmsCoreMgrSeed(omsName.ToString(), holder.Root_);
      if (!holder.Root_->Add(coreMgrSeed))
         return false;
      coreMgrSeed->BrkIdStart_.assign(brkId);
      coreMgrSeed->BrkCount_             = (brkCount <= 0 ? 1u : brkCount);
      coreMgrSeed->CmId_                 = cmId;
      coreMgrSeed->HowWait_              = howWait;
      coreMgrSeed->CoreCpuId_            = coreCpuId;
      coreMgrSeed->BackendFlushInterval_ = backendFlushInterval;
      coreMgrSeed->BackendCpuId_         = backendCpuId;
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
            coreMgrSeed->TwsLineG1_.reset(new TwsTradingLineG1{coreMgr});
            coreMgrSeed->TwsLineG1_->TseTradingLineMgr_
               = CreateTwsTradingLineMgr(coreMgr, cfgpath, ioargsTse, f9fmkt_TradingMarket_TwSEC);
            // ------------------
            if (fon9::iequals(&ioargsOtc.IoServiceCfgstr_, "IoTse"))
               ioargsOtc.IoServiceSrc_ = coreMgrSeed->TwsLineG1_->TseTradingLineMgr_;
            coreMgrSeed->TwsLineG1_->OtcTradingLineMgr_
               = CreateTwsTradingLineMgr(coreMgr, cfgpath, ioargsOtc, f9fmkt_TradingMarket_TwOTC);
            // ------------------
            twsNewSenderStep.reset(new OmsTwsSenderStepG1{*coreMgrSeed->TwsLineG1_});
            twsChgSenderStep.reset(new OmsTwsSenderStepG1{*coreMgrSeed->TwsLineG1_});
         }
         else { // 在 lg 設定檔裡面設定 ioargs.
            TwsTradingLgMgrSP lgMgr;
            fon9::IoManagerArgs   iocfgs{"LgMgr"};
            iocfgs.CfgFileName_ = lgCfgFileName.ToString(&cfgpath);
            iocfgs.SessionFactoryPark_ = ioargsTse.SessionFactoryPark_;
            iocfgs.DeviceFactoryPark_ = ioargsTse.DeviceFactoryPark_;
            lgMgr = TwsTradingLgMgr::Plant(coreMgr, holder, iocfgs);
            twsNewSenderStep.reset(new OmsTwsSenderStepLgMgr{*lgMgr});
            twsChgSenderStep.reset(new OmsTwsSenderStepLgMgr{*lgMgr});
            OmsLocalHelperMaker1{coreMgr}.MakeTradingLgLocalHelper(*lgMgr);
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
         coreMgrSeed->TwfExgMapMgr_.reset(new TwfExgMapMgr(coreMgr, twfCfgMgr->GetConfigSapling(), "TwfExgImporter"));
         twfCfgMgr->GetConfigSapling().Add(coreMgrSeed->TwfExgMapMgr_);
         twfCfgMgr->BindConfigFile(cfgpath + "TwfExgConfig.f9gv", true);
         coreMgrSeed->TwfExgMapMgr_->BindConfigFile(cfgpath + "TwfExgImporter.f9gv", true);
         coreMgr.Add(twfCfgMgr);
         // ------------------
         #define kCSTR_SesFpName    "FpSession"
         auto sesfp = FetchNamedPark<fon9::SessionFactoryPark>(*holder.Root_, kCSTR_SesFpName);
         if (!sesfp) { // 無法取得系統共用的 SessionFactoryPark.
            holder.SetPluginsSt(fon9::LogLevel::Error,
                                fon9_kCSTR_UtwOmsCoreName ".Create|err=SessionFactoryPark not found: " kCSTR_SesFpName);
            return false;
         }
         sesfp->Add(new OmsRptFromB50_SessionFactory("FromB50", &coreMgr, *twfFil1Factory, coreMgrSeed->TwfExgMapMgr_));
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
         OmsRequestRunStepSP  twfNewSenderStep, twfChgSenderStep, twfNewQRSenderStep, twfNewQSenderStep, twfChgQSenderStep;
         if (lgCfgFileName.IsNullOrEmpty()) {
            MaTreeSP twfG1Mgr{new MaTree{"TwfLineMgr"}};
            coreMgr.Add(new NamedSapling(twfG1Mgr, "TwfLineMgr"));
            coreMgrSeed->TwfLineG1_.reset(new TwfTradingLineG1{});
            // ---
            #define CREATE_TwfTradingLineMgr(type) \
            coreMgrSeed->TwfLineG1_->TradingLineMgr_[ExgSystemTypeToIndex(f9twf::ExgSystemType::type)] \
               = CreateTwfTradingLineMgr(*twfG1Mgr, cfgpath, ioargs##type, coreMgrSeed->TwfExgMapMgr_, f9twf::ExgSystemType::type)
            // ---
            CREATE_TwfTradingLineMgr(OptNormal);
            CREATE_TwfTradingLineMgr(OptAfterHour);
            CREATE_TwfTradingLineMgr(FutNormal);
            CREATE_TwfTradingLineMgr(FutAfterHour);
            for (auto& lmgr : coreMgrSeed->TwfLineG1_->TradingLineMgr_)
               lmgr->StartTimerForOpen(fon9::TimeInterval_Second(1));
            twfNewSenderStep  .reset(new OmsTwfSenderStepG1{*coreMgrSeed->TwfLineG1_});
            twfChgSenderStep  .reset(new OmsTwfSenderStepG1{*coreMgrSeed->TwfLineG1_});
            twfNewQRSenderStep.reset(new OmsTwfSenderStepG1{*coreMgrSeed->TwfLineG1_});
            twfNewQSenderStep .reset(new OmsTwfSenderStepG1{*coreMgrSeed->TwfLineG1_});
            twfChgQSenderStep .reset(new OmsTwfSenderStepG1{*coreMgrSeed->TwfLineG1_});
         }
         else { // Lg 線路群組.
            TwfTradingLgMgrSP    lgMgr;
            fon9::IoManagerArgs  iocfgs{"LgMgr"};
            iocfgs.CfgFileName_ = lgCfgFileName.ToString(&cfgpath);
            iocfgs.SessionFactoryPark_ = ioargsFutNormal.SessionFactoryPark_;
            iocfgs.DeviceFactoryPark_ = ioargsFutNormal.DeviceFactoryPark_;
            lgMgr = TwfTradingLgMgr::Plant(coreMgr, holder, iocfgs, coreMgrSeed->TwfExgMapMgr_);
            twfNewSenderStep  .reset(new OmsTwfSenderStepLgMgr{*lgMgr});
            twfChgSenderStep  .reset(new OmsTwfSenderStepLgMgr{*lgMgr});
            twfNewQRSenderStep.reset(new OmsTwfSenderStepLgMgr{*lgMgr});
            twfNewQSenderStep .reset(new OmsTwfSenderStepLgMgr{*lgMgr});
            twfChgQSenderStep .reset(new OmsTwfSenderStepLgMgr{*lgMgr});
            OmsLocalHelperMaker1{coreMgr}.MakeTradingLgLocalHelper(*lgMgr);
         }
         // ------------------
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
      auto now = fon9::LocalNow();
      int  nowHHMMSS = static_cast<int>(fon9::GetHHMMSS(now));
      now = fon9::TimeStampResetHHMMSS(now);
      if (nowHHMMSS < changeDayHHMMSS)
         now -= fon9::TimeInterval_Day(1);
      coreMgrSeed->AddCore(now);
      // ------------------------------------------------------------------
      return true;
   }
};

} // namespaces
//--------------------------------------------------------------------------//
extern "C" fon9::seed::PluginsDesc f9p_UtwOmsCore;
fon9::seed::PluginsDesc f9p_UtwOmsCore{"", &f9omstw::UtwOmsCoreMgrSeed::Create, nullptr, nullptr,};
static fon9::seed::PluginsPark f9pRegister{fon9_kCSTR_UtwOmsCoreName, &f9p_UtwOmsCore};
