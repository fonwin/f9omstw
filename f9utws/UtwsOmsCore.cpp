// \file f9utws/UtwsOmsCore.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCoreByThread.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsReportFactory.hpp"

#include "f9utws/UtwsSymb.hpp"
#include "f9utws/UtwsBrk.hpp"
#include "f9utws/UtwsExgSenderStep.hpp"

#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstws/OmsTwsTradingLineFix.hpp"
#include "f9omstws/OmsTwsReport.hpp"
#include "f9omstws/OmsTwsFilled.hpp"
#include "f9omstws/OmsTwsTradingLineMgrCfg.hpp"

#include "fon9/seed/Plugins.hpp"
#include "fon9/seed/SysEnv.hpp"
const unsigned    kPoolObjCount = 1000 * 100;

namespace f9omstw {

class UomsOrderTwsFactory : public OmsOrderFactory {
   fon9_NON_COPY_NON_MOVE(UomsOrderTwsFactory);
   using base = OmsOrderFactory;

   using RawSupplier = fon9::ObjSupplier<OmsTwsOrderRaw, kPoolObjCount>;
   RawSupplier::ThisSP RawSupplier_{RawSupplier::Make()};
   OmsTwsOrderRaw* MakeOrderRawImpl() override {
      return this->RawSupplier_->Alloc();
   }

   using OmsTwsOrder = OmsOrder;
   using OrderSupplier = fon9::ObjSupplier<OmsTwsOrder, kPoolObjCount>;
   OrderSupplier::ThisSP OrderSupplier_{OrderSupplier::Make()};
   OmsTwsOrder* MakeOrderImpl() override {
      return this->OrderSupplier_->Alloc();
   }
public:
   UomsOrderTwsFactory()
      : base(fon9::Named{"TwsOrd"}, MakeFieldsT<OmsTwsOrderRaw>()) {
   }
   ~UomsOrderTwsFactory() {
   }
};
//--------------------------------------------------------------------------//
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
template <class Root, class NamedSapling>
static void AddNamedSapling(Root& root, fon9::intrusive_ptr<NamedSapling> sapling) {
   root.Add(new fon9::seed::NamedSapling(sapling, sapling->Name_));
}

class UtwsOmsCoreMgrSeed;
struct UtwsOmsCore : public OmsCoreByThread {
   fon9_NON_COPY_NON_MOVE(UtwsOmsCore);
   using base = OmsCoreByThread;
   friend class UtwsOmsCoreMgrSeed;
   UtwsOmsCore(OmsCoreMgrSP owner, std::string seedPath, std::string name)
      : base(std::move(owner), std::move(seedPath), std::move(name)) {
      this->Symbs_.reset(new OmsSymbTree(*this, UtwsSymb::MakeLayout(OmsSymbTree::DefaultTreeFlag()), &UtwsSymb::SymbMaker));
      this->Brks_.reset(new OmsBrkTree(*this, UtwsBrk::MakeLayout(OmsBrkTree::DefaultTreeFlag()), &OmsBrkTree::TwsBrkIndex1));
      this->Brks_->Initialize(&UtwsBrk::BrkMaker, "8610", 5u, &IncStrAlpha);
      // 建立委託書號表的關聯.
      this->Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwSEC);
      this->Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwOTC);
   }
   ~UtwsOmsCore() {
      this->OnBeforeDestroy();
   }
   void OnParentTreeClear(fon9::seed::Tree& tree) override {
      base::OnParentTreeClear(tree);
      this->OnBeforeDestroy();
   }
};
class UtwsOmsCoreMgrSeed : public fon9::seed::NamedSapling {
   fon9_NON_COPY_NON_MOVE(UtwsOmsCoreMgrSeed);
   using base = fon9::seed::NamedSapling;
   const fon9::seed::MaTreeSP Root_;
   UtwsExgTradingLineMgr      ExgLineMgr_;

   UtwsOmsCoreMgrSeed(std::string name, fon9::seed::MaTreeSP owner)
      : base(new OmsCoreMgr{"cores"}, std::move(name))
      , Root_{std::move(owner)}
      , ExgLineMgr_{*static_cast<OmsCoreMgr*>(Sapling_.get())} {
   }

   static bool SetIoManager(fon9::seed::PluginsHolder& holder, fon9::IoManagerArgs& out) {
      if (*out.IoServiceCfgstr_.c_str() == '/') {
         fon9::StrView name{&out.IoServiceCfgstr_};
         name.SetBegin(name.begin() + 1);
         out.IoServiceSrc_ = holder.Root_->GetSapling<fon9::IoManager>(name);
         if (!out.IoServiceSrc_) {
            holder.SetPluginsSt(fon9::LogLevel::Error, "UtwsOmsCore.Create|err=Unknown IoManager: ", out.IoServiceCfgstr_);
            return false;
         }
      }
      return true;
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
      AddNamedSapling(coreMgr, linemgr);
      coreMgr.Add(new TwsTradingLineMgrCfgSeed(*linemgr, cfgpath, name.ToString() + "_cfg"));
   }

public:
   bool AddCore(fon9::TimeStamp tday) {
      fon9::RevBufferFixedSize<128> rbuf;
      fon9::RevPrint(rbuf, this->Name_, '_', fon9::GetYYYYMMDD(tday));
      std::string coreName = rbuf.ToStrT<std::string>();
      fon9::RevPrint(rbuf, this->Name_, '/');
      UtwsOmsCore* core = new UtwsOmsCore(static_cast<OmsCoreMgr*>(this->Sapling_.get()),
                                          rbuf.ToStrT<std::string>(),
                                          std::move(coreName));
      fon9::TimedFileName logfn(fon9::seed::SysEnv_GetLogFileFmtPath(*this->Root_), fon9::TimedFileName::TimeScale::Day);
      // oms log 檔名與 TDay 相關, 與 TimeZone 無關, 所以要扣除 logfn.GetTimeChecker().GetTimeZoneOffset();
      logfn.RebuildFileName(tday - logfn.GetTimeChecker().GetTimeZoneOffset());
      std::string fname = logfn.GetFileName() + this->Name_ + ".log";
      auto res = core->Start(tday, fname);
      core->SetTitle(std::move(fname));
      if (res.IsError())
         core->SetDescription(fon9::RevPrintTo<std::string>(res));
      // 必須在載入完畢後再加入 CoreMgr, 否則可能造成 TDayChanged 事件處理者, 沒有取得完整的資料表.
      return static_cast<OmsCoreMgr*>(this->Sapling_.get())->Add(core);
   }

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
      fon9::IoManagerArgs  ioargsTse, ioargsOtc;
      fon9::StrView        tag, value, omsName{"omstws"};
      while (fon9::SbrFetchTagValue(args, tag, value)) {
         fon9::IoManagerArgs* dst;
         if (tag == "Name") {
            omsName = value;
            continue;
         }
         if (tag == "IoTse")
            dst = &ioargsTse;
         else if (tag == "IoOtc")
            dst = &ioargsOtc;
         else {
            holder.SetPluginsSt(fon9::LogLevel::Error, "UtwsOmsCore.Create|err=Unknown tag: ", tag);
            return false;
         }
         if (!(tag = fon9::SbrFetchInsideNoTrim(value)).IsNull())
            value = tag;
         dst->IoServiceCfgstr_ = fon9::StrTrim(&value).ToString();
         if (!SetIoManager(holder, *dst))
            return false;
      }
      ioargsTse.DeviceFactoryPark_ = ioargsOtc.DeviceFactoryPark_ = devfp;
      // ------------------------------------------------------------------
      UtwsOmsCoreMgrSeed* coreMgrSeed = new UtwsOmsCoreMgrSeed(omsName.ToString(), holder.Root_);
      if (!holder.Root_->Add(coreMgrSeed))
         return false;

      OmsCoreMgr*           coreMgr = static_cast<OmsCoreMgr*>(coreMgrSeed->Sapling_.get());
      UomsOrderTwsFactory*  ordfac = new UomsOrderTwsFactory;
      OmsTwsReportFactorySP rptFactory = new OmsTwsReportFactory("TwsRpt", ordfac);
      OmsTwsFilledFactorySP filFactory = new OmsTwsFilledFactory("TwsFil", ordfac);
      coreMgr->SetOrderFactoryPark(new OmsOrderFactoryPark{ordfac});

      ioargsTse.SessionFactoryPark_ = ioargsOtc.SessionFactoryPark_
         = fon9::seed::FetchNamedPark<fon9::SessionFactoryPark>(*coreMgr, "FpSession");
      const std::string logpath = fon9::seed::SysEnv_GetLogFileFmtPath(*coreMgrSeed->Root_);
      ioargsTse.SessionFactoryPark_->Add(
         new TwsTradingLineFixFactory(
            *coreMgr, *rptFactory, *filFactory,
            logpath,
            fon9::Named{"FIX44"}));
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
                                        OmsRequestRunStepSP{new UtwsExgSenderStep{coreMgrSeed->ExgLineMgr_}})}),
         new OmsTwsRequestChgFactory("TwsChg",
                                     OmsRequestRunStepSP{new UtwsExgSenderStep{coreMgrSeed->ExgLineMgr_}}),
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
