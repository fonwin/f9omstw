// \file f9utw/UtwfOmsCoreSeed.hpp
//
// 在 #include 之前, 必須先定義:
//    struct UtwfSymb {
//       static fon9::seed::LayoutSP MakeLayout(fon9::seed::TreeFlag flags);
//       static OmsSymbSP SymbMaker(const fon9::StrView& symbid);
//    };
//
//    struct UtwfBrk {
//       static fon9::seed::LayoutSP MakeLayout(fon9::seed::TreeFlag flags);
//       static OmsBrkSP BrkMaker(const fon9::StrView& brkid);
//    };
//
//    struct UomsTwfCfgMgr : public fon9::seed::MaConfigMgr {
//       UomsTwfCfgMgr(OmsCoreMgrSeed& owner);
//       void Initialize(TwfExgMapMgrSP twfExgMapMgr, const std::string& cfgpath);
//       // [換日、重啟] 資料表建立後: 載入風控基本資料.
//       void OnAfter_InitCoreTables(OmsResource& res);
//    };
//
//    // 或 using UomsTwfExgMapMgr = TwfExgMapMgr;
//    class UomsTwfExgMapMgr : public TwfExgMapMgr {
//       fon9_NON_COPY_NON_MOVE(UomsTwfExgMapMgr);
//       using base = TwfExgMapMgr;
//    public:
//       using base::base;
//    };
//
//    #define fon9_kCSTR_UtwOmsCoreName   "xxOmsCore"
//
//    UomsTwfOrder1Factory;
//    UomsTwfRequestIni1Factory;
//    UomsTwfRequestChg1Factory;
//    UomsTwfIni1RiskCheck;
//
//    UomsTwfOrder7Factory;
//    UomsTwfRequestIni7Factory;
//
//    UomsTwfOrder9Factory;
//    UomsTwfRequestIni9Factory;
//    UomsTwfRequestChg9Factory;
//    UomsTwfIni9RiskCheck;
//
// \author fonwinz@gmail.com
#include "f9omstw/OmsCoreMgrSeed.hpp"
#include "f9omstw/OmsEventSessionSt.hpp"

#include "f9omstwf/OmsTwfSenderStepG1.hpp"
#include "f9omstwf/OmsTwfSenderStepLgMgr.hpp"
#include "f9omstwf/OmsTwfLineTmpFactory.hpp"
#include "f9omstwf/OmsTwfRptFromB50.hpp"

#include "f9utw/UtwSpCmdTwf.hpp"

#include "fon9/seed/SysEnv.hpp"
#include "f9twf/TwfTickSize.hpp"

namespace f9omstw {

class UtwfOmsCoreMgrSeed : public OmsCoreMgrSeed {
   fon9_NON_COPY_NON_MOVE(UtwfOmsCoreMgrSeed);
   using base = OmsCoreMgrSeed;

public:
   UomsTwfCfgMgr*       CfgMgr_{};
   UomsTwfExgMapMgr*    TwfExgMapMgr_{};
   fon9::CharVector     BrkIdStart_;
   unsigned             BrkCount_;
   f9twf::FcmId         CmId_;
   char                 Padding___[2];
   TwfTradingLineG1SP   TwfLineG1_;

   using base::base;

   UtwfOmsCoreMgrSeed(std::string name, fon9::seed::MaTreeSP owner,
                      FnSetRequestLgOut fnSetRequestLgOut = &OmsSetRequestLgOut_UseIvac)
      : base(std::move(name), std::move(owner), fnSetRequestLgOut) {
   }

   /// 建立委託書號表的關聯.
   virtual void InitializeTwfOrdNoMap(OmsBrkTree& brks) {
      brks.InitializeTwfOrdNoMap(f9fmkt_TradingMarket_TwFUT);
      brks.InitializeTwfOrdNoMap(f9fmkt_TradingMarket_TwOPT);
      // brk.InitializeTwfOrdNoMapRef(f9fmkt_TradingMarket_TwOPT, f9fmkt_TradingMarket_TwFUT);
   }
   void InitCoreTables(OmsResource& res) override {
      res.Symbs_.reset(new OmsSymbTree(res.Core_, UtwfSymb::MakeLayout(OmsSymbTree::DefaultTreeFlag()), &UtwfSymb::SymbMaker));
      res.Brks_.reset(new OmsBrkTree(res.Core_, UtwfBrk::MakeLayout(OmsBrkTree::DefaultTreeFlag()), &OmsBrkTree::TwfBrkIndex));
      res.Brks_->Initialize(&UtwfBrk::BrkMaker, ToStrView(this->BrkIdStart_), this->BrkCount_, &f9omstw_IncStrAlpha);
      for (size_t L = 0; L < this->BrkCount_; ++L) {
         if (auto* brk = res.Brks_->GetBrkRec(L)) {
            brk->CmId_ = this->CmId_;
            brk->FcmId_ = this->CmId_; // 當 P06 載入後再調整.
         }
      }
      this->InitializeTwfOrdNoMap(*res.Brks_);
      // 載入風控基本資料.
      this->TwfExgMapMgr_->OnAfter_InitCoreTables(res);
      this->CfgMgr_->OnAfter_InitCoreTables(res);
      // 建立特殊指令(例:特殊刪單)管理員.
      if (res.Core_.Owner_->RequestFactoryPark().GetFactory("TwfNew"))
         res.Sapling_->Add(new SpCmdMgrTwf(&res.Core_, "SpCmdTwf"));
   }

   static bool Create(fon9::seed::PluginsHolder& holder, fon9::StrView args,
                      UtwfOmsCoreMgrSeed* (*fnCreator)(fon9::StrView omsName, fon9::seed::PluginsHolder& holder)) {
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
      //   - IoFut={上市交易線路 IoService 參數} 可使用 IoFut=/MaIo 表示與 /MaIo 共用 IoService
      //   - IoFutA, IoOpt, IoOptA
      // - 「IoService 設定參數」請參考: IoServiceArgs.hpp
      // - BrkId=起始券商代號
      // - BrkCount=券商數量
      // - Wait=Policy     Busy or Block(default)
      // - Cpu=CpuId
      fon9::IoManagerArgs  ioargsFutNormal{"UtwFutNormal"};
      fon9::IoManagerArgs  ioargsOptNormal{"UtwOptNormal"};
      fon9::IoManagerArgs  ioargsFutAfterHour{"UtwFutAfterHour"};
      fon9::IoManagerArgs  ioargsOptAfterHour{"UtwOptAfterHour"};
      fon9::StrView        tag, value, omsName{"omstw"}, brkId, lgCfgFileName;
      unsigned             brkCount = 0;
      fon9::HowWait        howWait{};
      int                  cpuId = -1;
      f9twf::FcmId         cmId{};
      fon9::TimeInterval   flushInterval{fon9::TimeInterval_Millisecond(1)}; // backend flush interval
      int                  changeDayHHMMSS = -1; // 換日時間, 現貨 00:00 換日, 期權 6:00 換日;
      while (fon9::SbrFetchTagValue(args, tag, value)) {
         fon9::IoManagerArgs* ioargs;
         if (tag == "Name")
            omsName = value;
         else if (tag == "IoFut") {
            ioargs = &ioargsFutNormal;
         __SET_IO_ARGS:;
            if (!ioargs->SetIoServiceCfg(holder, value))
               return false;
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
         else if (tag == "ChgTDay")
            changeDayHHMMSS = fon9::StrTo(value, changeDayHHMMSS);
         else {
            holder.SetPluginsSt(fon9::LogLevel::Error, fon9_kCSTR_UtwOmsCoreName ".Create|err=Unknown tag: ", tag);
            return false;
         }
      }
      if (changeDayHHMMSS < 0)
         changeDayHHMMSS = 60000;
      // ------------------------------------------------------------------
      UtwfOmsCoreMgrSeed* coreMgrSeed = fnCreator(omsName, holder);
      if (!holder.Root_->Add(coreMgrSeed))
         return false;
      coreMgrSeed->BrkIdStart_.assign(brkId);
      coreMgrSeed->BrkCount_ = (brkCount <= 0 ? 1u : brkCount);
      coreMgrSeed->CmId_ = cmId;
      coreMgrSeed->CpuId_ = cpuId;
      coreMgrSeed->FlushInterval_ = flushInterval;
      coreMgrSeed->HowWait_ = howWait;
      // ------------------------------------------------------------------
      const std::string     logpath = SysEnv_GetLogFileFmtPath(*coreMgrSeed->Root_);
      const std::string     cfgpath = SysEnv_GetConfigPath(*coreMgrSeed->Root_).ToString();
      OmsCoreMgr&           coreMgr = coreMgrSeed->GetOmsCoreMgr();
      coreMgr.SetEventFactoryPark(new f9omstw::OmsEventFactoryPark(
         new OmsEventInfoFactory(),
         new OmsEventSessionStFactory()
      ));
      // ------------------------------------------------------------------
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
      f9twf::Load_TwfTickSize(&cfgpath);
      MaConfigMgrSP                          cfgMgr{coreMgrSeed->CfgMgr_ = new UomsTwfCfgMgr{*coreMgrSeed}};
      fon9::intrusive_ptr<UomsTwfExgMapMgr>  twfExgMap{coreMgrSeed->TwfExgMapMgr_ = new UomsTwfExgMapMgr(coreMgr, cfgMgr->GetConfigSapling(), "TwfExgImporter")};
      coreMgrSeed->CfgMgr_->Initialize(*twfExgMap, cfgpath);
      cfgMgr->GetConfigSapling().Add(twfExgMap);
      cfgMgr->BindConfigFile(cfgpath + cfgMgr->Name_ + ".f9gv", true);
      twfExgMap->BindConfigFile(cfgpath + "TwfExgImporter.f9gv", true);
      twfExgMap->StopAndWait_SchTask();
      coreMgr.Add(cfgMgr);
      // ------------------
      #define kCSTR_SesFpName    "FpSession"
      auto sesfp = FetchNamedPark<fon9::SessionFactoryPark>(*holder.Root_, kCSTR_SesFpName);
      if (!sesfp) { // 無法取得系統共用的 SessionFactoryPark.
         holder.SetPluginsSt(fon9::LogLevel::Error,
                             fon9_kCSTR_UtwOmsCoreName ".Create|err=SessionFactoryPark not found: " kCSTR_SesFpName);
         return false;
      }
      sesfp->Add(new OmsRptFromB50_SessionFactory("FromB50", &coreMgr, *twfFil1Factory, twfExgMap));
      // ------------------
      ioargsFutNormal.DeviceFactoryPark_ = devfp;
      ioargsOptNormal.DeviceFactoryPark_ = devfp;
      ioargsFutAfterHour.DeviceFactoryPark_ = devfp;
      ioargsOptAfterHour.DeviceFactoryPark_ = devfp;
      ioargsFutNormal.SessionFactoryPark_ = FetchNamedPark<fon9::SessionFactoryPark>(coreMgr, "TwfFpSession");
      ioargsOptNormal.SessionFactoryPark_ = ioargsFutNormal.SessionFactoryPark_;
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
         coreMgrSeed->TwfLineG1_.reset(new TwfTradingLineG1{coreMgr});
         // ---
         #define CREATE_TwfTradingLineMgr(type) \
            coreMgrSeed->TwfLineG1_->TradingLineMgr_[ExgSystemTypeToIndex(f9twf::ExgSystemType::type)] \
               = CreateTwfTradingLineMgr(*twfG1Mgr, cfgpath, ioargs##type, twfExgMap, f9twf::ExgSystemType::type)
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
      }
      // ------------------
      coreMgr.SetOrderFactoryPark(new OmsOrderFactoryPark{twfOrd1Factory, twfOrd7Factory, twfOrd9Factory});
      coreMgr.SetRequestFactoryPark(new f9omstw::OmsRequestFactoryPark(
         new UomsTwfRequestIni1Factory("TwfNew", twfOrd1Factory,
                                       OmsRequestRunStepSP{new UomsTwfIni1RiskCheck(std::move(twfNewSenderStep))}),
         new UomsTwfRequestChg1Factory("TwfChg", std::move(twfChgSenderStep)),
         twfRpt1Factory,
         twfFil1Factory,
         twfFil2Factory,
         new UomsTwfRequestIni7Factory("TwfNewQR", twfOrd7Factory, std::move(twfNewQRSenderStep)),
         twfRpt8Factory,
         new UomsTwfRequestIni9Factory("TwfNewQ", twfOrd9Factory,
                                      OmsRequestRunStepSP{new UomsTwfIni9RiskCheck(std::move(twfNewQSenderStep))}),
         new UomsTwfRequestChg9Factory("TwfChgQ", std::move(twfChgQSenderStep)),
         twfRpt9Factory
      ));
      // ------------------------------------------------------------------
      coreMgr.ReloadErrCodeAct(fon9::ToStrView(cfgpath + "OmsErrCodeAct.cfg"));
      auto now = fon9::LocalNow();
      int  nowHHMMSS = static_cast<int>(fon9::GetHHMMSS(now));
      now = fon9::TimeStampResetHHMMSS(now);
      if (nowHHMMSS < changeDayHHMMSS)
         now -= fon9::TimeInterval_Day(1);
      coreMgrSeed->AddCore(now);
      twfExgMap->Restart_SchTask(fon9::TimeInterval{});
      // ------------------------------------------------------------------
      return true;
   }
};

} // namespaces
