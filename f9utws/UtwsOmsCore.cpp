// \file f9utws/UtwsOmsCore.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCoreByThread.hpp"
#include "f9utws/UtwsSymb.hpp"
#include "f9utws/UtwsBrk.hpp"
#include "f9utws/UtwsExgSenderStep.hpp"

#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstws/OmsTwsTradingLineFix.hpp"
#include "f9omstws/OmsTwsReport.hpp"
#include "f9omstws/OmsTwsFilled.hpp"
#include "f9omstw/OmsReportFactory.hpp"

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

struct UtwsOmsCoreMgr;
struct UtwsOmsCore : public OmsCoreByThread {
   fon9_NON_COPY_NON_MOVE(UtwsOmsCore);
   using base = OmsCoreByThread;
   friend struct UtwsOmsCoreMgr;
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
struct UtwsOmsCoreMgr : public fon9::seed::NamedSapling {
   fon9_NON_COPY_NON_MOVE(UtwsOmsCoreMgr);
   using base = fon9::seed::NamedSapling;
   const fon9::seed::MaTreeSP Root_;
   UtwsExgTradingLineMgr      ExgLineMgr_;

   UtwsOmsCoreMgr(fon9::seed::MaTreeSP owner, fon9::DeviceFactoryParkSP devfp)
      : base(new OmsCoreMgr{"cores"}, "omstws")
      , Root_{std::move(owner)}
      , ExgLineMgr_{*static_cast<OmsCoreMgr*>(Sapling_.get())} {
      OmsCoreMgr* coreMgr = static_cast<OmsCoreMgr*>(this->Sapling_.get());

      UomsOrderTwsFactory* ordfac = new UomsOrderTwsFactory;
      coreMgr->SetOrderFactoryPark(new OmsOrderFactoryPark{ordfac});
      OmsTwsReportFactorySP  rptFactory = new OmsTwsReportFactory("TwsRpt", ordfac);
      OmsTwsFilledFactorySP  filFactory = new OmsTwsFilledFactory("TwsFil", ordfac);

      fon9::IoManagerArgs ioargs;
      ioargs.DeviceFactoryPark_ = devfp;
      ioargs.SessionFactoryPark_ = fon9::seed::FetchNamedPark<fon9::SessionFactoryPark>(*coreMgr, "FpSession");

      const std::string logpath = fon9::seed::SysEnv_GetLogFileFmtPath(*this->Root_);
      ioargs.SessionFactoryPark_->Add(new TwsTradingLineFixFactory(*coreMgr, *rptFactory, *filFactory,
                                                                   logpath,
                                                                   fon9::Named{"FIX44"}));

      const std::string cfgpath = fon9::seed::SysEnv_GetConfigPath(*this->Root_).ToString();
      ioargs.Name_ = "UtwSEC";
      ioargs.CfgFileName_ = cfgpath + ioargs.Name_ + "_io.f9gv";
      this->ExgLineMgr_.TseTradingLineMgr_.reset(new TwsTradingLineMgr{ioargs, f9fmkt_TradingMarket_TwSEC});
      AddNamedSapling(*coreMgr, this->ExgLineMgr_.TseTradingLineMgr_);

      if (0);//櫃號設定, 是否要共用 IoService?
      // ioargs.IoServiceSrc_ = this->ExgLineMgr_.TseTradingLineMgr_;
      // 設定 & 儲存設定?

      ioargs.Name_ = "UtwOTC";
      ioargs.CfgFileName_ = cfgpath + ioargs.Name_ + "_io.f9gv";
      this->ExgLineMgr_.OtcTradingLineMgr_.reset(new TwsTradingLineMgr{ioargs, f9fmkt_TradingMarket_TwOTC});
      AddNamedSapling(*coreMgr, this->ExgLineMgr_.OtcTradingLineMgr_);

      coreMgr->SetRequestFactoryPark(new f9omstw::OmsRequestFactoryPark(
         new OmsTwsRequestIniFactory("TwsNew", ordfac,
                                     OmsRequestRunStepSP{new UomsTwsIniRiskCheck(
                                        OmsRequestRunStepSP{new UtwsExgSenderStep{this->ExgLineMgr_}})}),
         new OmsTwsRequestChgFactory("TwsChg",
                                     OmsRequestRunStepSP{new UtwsExgSenderStep{this->ExgLineMgr_}}),
         rptFactory, filFactory
      ));
      coreMgr->SetEventFactoryPark(new f9omstw::OmsEventFactoryPark{});
   }
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
      (void)args;
      #define kCSTR_DevFpName    "FpDevice"
      auto devfp = fon9::seed::FetchNamedPark<fon9::DeviceFactoryPark>(*holder.Root_, kCSTR_DevFpName);
      if (!devfp) { // 無法取得系統共用的 DeviceFactoryPark.
         holder.SetPluginsSt(fon9::LogLevel::Error, "UtwsOmsCore.Create|err=DeviceFactoryPark not found: " kCSTR_DevFpName);
         return false;
      }
      UtwsOmsCoreMgr* coreMgr;
      if (!holder.Root_->Add(coreMgr = new UtwsOmsCoreMgr(holder.Root_, std::move(devfp))))
         return false;
      coreMgr->AddCore(fon9::UtcNow() + fon9::GetLocalTimeZoneOffset());
      return true;
   }
};

} // namespaces
//--------------------------------------------------------------------------//
extern "C" fon9::seed::PluginsDesc f9p_UtwsOmsCore;
fon9::seed::PluginsDesc f9p_UtwsOmsCore{"", &f9omstw::UtwsOmsCoreMgr::Create, nullptr, nullptr,};
static fon9::seed::PluginsPark f9pRegister{"UtwsOmsCore", &f9p_UtwsOmsCore};
