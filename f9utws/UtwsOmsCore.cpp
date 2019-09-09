// \file f9utws/UtwsOmsCore.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCoreMgrSeed.hpp"
#include "f9omstw/OmsReportFactory.hpp"

#include "f9utws/UtwsSymb.hpp"
#include "f9utws/UtwsBrk.hpp"

#include "f9omstws/OmsTwsSenderStepG1.hpp"
#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstws/OmsTwsTradingLineFix.hpp"
#include "f9omstws/OmsTwsTradingLineTmp2019.hpp"
#include "f9omstws/OmsTwsReport.hpp"
#include "f9omstws/OmsTwsFilled.hpp"
#include "f9omstws/OmsTwsTradingLineMgrCfg.hpp"

#include "fon9/seed/SysEnv.hpp"

const unsigned    kPoolObjCount = 1000 * 100;

namespace f9omstw {

using UomsOrderTwsFactory = OmsOrderFactoryT<OmsOrder, OmsTwsOrderRaw, kPoolObjCount>;
using OmsTwsRequestIniFactory = OmsRequestFactoryT<OmsTwsRequestIni, kPoolObjCount>;
using OmsTwsRequestChgFactory = OmsRequestFactoryT<OmsTwsRequestChg, kPoolObjCount>;
//--------------------------------------------------------------------------//
struct UomsTwsIniRiskCheck : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(UomsTwsIniRiskCheck);
   using base = OmsRequestRunStep;
   using base::base;

   void RunRequest(OmsRequestRunnerInCore&& runner) override {
      // TODO: 風控檢查.
      assert(dynamic_cast<const OmsTwsRequestIni*>(runner.OrderRaw_.Order().Initiator()) != nullptr);
      auto& ordraw = *static_cast<OmsTwsOrderRaw*>(&runner.OrderRaw_);
      auto* inireq = static_cast<const OmsTwsRequestIni*>(ordraw.Order().Initiator());
      if (ordraw.OType_ == OmsTwsOType{})
         ordraw.OType_ = inireq->OType_;
      // 風控成功, 設定委託剩餘數量及價格(提供給風控資料計算), 然後執行下一步驟.
      if (&ordraw.Request() == inireq) {
         ordraw.LastPri_ = inireq->Pri_;
         ordraw.LastPriType_ = inireq->PriType_;
         if (inireq->RxKind() == f9fmkt_RxKind_RequestNew)
            ordraw.AfterQty_ = ordraw.LeavesQty_ = inireq->Qty_;
      }
      this->ToNextStep(std::move(runner));
   }
};
//--------------------------------------------------------------------------//
class UtwsOmsCoreMgrSeed : public OmsCoreMgrSeed {
   fon9_NON_COPY_NON_MOVE(UtwsOmsCoreMgrSeed);
   using base = OmsCoreMgrSeed;
   TwsTradingLineMgrG1  ExgLineMgr_;
   f9tws::BrkId         BrkIdStart_;
   unsigned             BrkCount_;

   UtwsOmsCoreMgrSeed(std::string name, fon9::seed::MaTreeSP owner)
      : base(std::move(name), std::move(owner))
      , ExgLineMgr_{*static_cast<OmsCoreMgr*>(Sapling_.get())} {
   }

   static void CreateTradingLine(OmsCoreMgr& coreMgr,
                                 const std::string& cfgpath,
                                 fon9::IoManagerArgs& ioargs,
                                 f9fmkt_TradingMarket mkt,
                                 fon9::StrView name,
                                 TwsTradingLineMgrSP& linemgr) {
      ioargs.Name_ = name.ToString() + "_io";
      ioargs.CfgFileName_ = cfgpath + ioargs.Name_ + ".f9gv";
      linemgr.reset(new TwsTradingLineMgr{ioargs, mkt});
      coreMgr.Add(new fon9::seed::NamedSapling(linemgr, linemgr->Name_));
      coreMgr.Add(new TwsTradingLineMgrCfgSeed(*linemgr, cfgpath, name.ToString() + "_cfg"));
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
   }

public:
   static bool Create(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
      #define kCSTR_DevFpName    "FpDevice"
      auto devfp = fon9::seed::FetchNamedPark<fon9::DeviceFactoryPark>(*holder.Root_, kCSTR_DevFpName);
      if (!devfp) { // 無法取得系統共用的 DeviceFactoryPark.
         holder.SetPluginsSt(fon9::LogLevel::Error, "UtwsOmsCore.Create|err=DeviceFactoryPark not found: " kCSTR_DevFpName);
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
      fon9::IoManagerArgs  ioargsTse, ioargsOtc;
      fon9::StrView        tag, value, omsName{"omstws"}, brkId;
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
            if (!(tag = fon9::SbrFetchInsideNoTrim(value)).IsNull())
               value = tag;
            ioargs->IoServiceCfgstr_ = fon9::StrTrim(&value).ToString();
            if (!SetIoManager(holder, *ioargs))
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
         else {
            holder.SetPluginsSt(fon9::LogLevel::Error, "UtwsOmsCore.Create|err=Unknown tag: ", tag);
            return false;
         }
      }
      if (brkId.size() != sizeof(f9tws::BrkId)) {
         holder.SetPluginsSt(fon9::LogLevel::Error, "UtwsOmsCore.Create|err=Unknown BrkId: ", brkId);
         return false;
      }
      ioargsTse.DeviceFactoryPark_ = ioargsOtc.DeviceFactoryPark_ = devfp;
      // ------------------------------------------------------------------
      UtwsOmsCoreMgrSeed* coreMgrSeed = new UtwsOmsCoreMgrSeed(omsName.ToString(), holder.Root_);
      if (!holder.Root_->Add(coreMgrSeed))
         return false;
      coreMgrSeed->BrkIdStart_.AssignFrom(brkId);
      coreMgrSeed->BrkCount_ = (brkCount <= 0 ? 1u : brkCount);
      coreMgrSeed->CpuId_ = cpuId;
      coreMgrSeed->HowWait_ = howWait;

      OmsCoreMgr*           coreMgr = static_cast<OmsCoreMgr*>(coreMgrSeed->Sapling_.get());
      UomsOrderTwsFactory*  ordfac = new UomsOrderTwsFactory{"TwsOrd"};
      OmsTwsReportFactorySP rptFactory = new OmsTwsReportFactory("TwsRpt", ordfac);
      OmsTwsFilledFactorySP filFactory = new OmsTwsFilledFactory("TwsFil", ordfac);
      coreMgr->SetOrderFactoryPark(new OmsOrderFactoryPark{ordfac});

      ioargsTse.SessionFactoryPark_ = ioargsOtc.SessionFactoryPark_
         = fon9::seed::FetchNamedPark<fon9::SessionFactoryPark>(*coreMgr, "FpSession");
      const std::string logpath = fon9::seed::SysEnv_GetLogFileFmtPath(*coreMgrSeed->Root_);
      ioargsTse.SessionFactoryPark_->Add(new TwsTradingLineFixFactory(
                                                *coreMgr, *rptFactory, *filFactory,
                                                logpath, fon9::Named{"FIX44"}));
      // 預計新版 TMP 下單, Named 使用 "TMP2"
      ioargsTse.SessionFactoryPark_->Add(new TwsTradingLineTmpFactory2019(
                                                *coreMgr, *rptFactory, *filFactory,
                                                logpath, fon9::Named{"TMP"}));
      // ------------------------------------------------------------------
      const std::string cfgpath = fon9::seed::SysEnv_GetConfigPath(*coreMgrSeed->Root_).ToString();
      CreateTradingLine(*coreMgr, cfgpath, ioargsTse, f9fmkt_TradingMarket_TwSEC, "UtwSEC",
                        coreMgrSeed->ExgLineMgr_.TseTradingLineMgr_);
      // ------------------
      if (ioargsOtc.IoServiceCfgstr_ == "IoTse")
         ioargsOtc.IoServiceSrc_ = coreMgrSeed->ExgLineMgr_.TseTradingLineMgr_;
      CreateTradingLine(*coreMgr, cfgpath, ioargsOtc, f9fmkt_TradingMarket_TwOTC, "UtwOTC",
                        coreMgrSeed->ExgLineMgr_.OtcTradingLineMgr_);
      // ------------------------------------------------------------------
      coreMgr->SetRequestFactoryPark(new f9omstw::OmsRequestFactoryPark(
         new OmsTwsRequestIniFactory("TwsNew", ordfac,
                                     OmsRequestRunStepSP{new UomsTwsIniRiskCheck(
                                        OmsRequestRunStepSP{new OmsTwsSenderStepG1{coreMgrSeed->ExgLineMgr_}})}),
         new OmsTwsRequestChgFactory("TwsChg",
                                     OmsRequestRunStepSP{new OmsTwsSenderStepG1{coreMgrSeed->ExgLineMgr_}}),
         rptFactory, filFactory
      ));
      coreMgr->SetEventFactoryPark(new f9omstw::OmsEventFactoryPark{});
      coreMgr->ReloadErrCodeAct(fon9::ToStrView(cfgpath + "UtwErrCode.cfg"));
      coreMgrSeed->AddCore(fon9::UtcNow() + fon9::GetLocalTimeZoneOffset());
      return true;
   }
};

} // namespaces
//--------------------------------------------------------------------------//
extern "C" fon9::seed::PluginsDesc f9p_UtwsOmsCore;
fon9::seed::PluginsDesc f9p_UtwsOmsCore{"", &f9omstw::UtwsOmsCoreMgrSeed::Create, nullptr, nullptr,};
static fon9::seed::PluginsPark f9pRegister{"UtwsOmsCore", &f9p_UtwsOmsCore};
