// \file utws/UtwsOmsCore.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCoreByThread.hpp"
#include "utws/UtwsSymb.hpp"
#include "utws/UtwsBrk.hpp"
#include "fon9/seed/Plugins.hpp"
#include "fon9/seed/SysEnv.hpp"
#include "fon9/TimedFileName.hpp"

#include "f9omstw/OmsOrderTws.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "fon9/ObjSupplier.hpp"
const unsigned    kPoolObjCount = 1000 * 100;

namespace f9omstw {

class UomsOrderTwsFactory : public OmsOrderFactory {
   fon9_NON_COPY_NON_MOVE(UomsOrderTwsFactory);
   using base = OmsOrderFactory;

   using RawSupplier = fon9::ObjSupplier<OmsOrderTwsRaw, kPoolObjCount>;
   RawSupplier::ThisSP RawSupplier_{RawSupplier::Make()};
   OmsOrderTwsRaw* MakeOrderRawImpl() override {
      return this->RawSupplier_->Alloc();
   }

   using OmsOrderTws = OmsOrder;
   using OrderSupplier = fon9::ObjSupplier<OmsOrderTws, kPoolObjCount>;
   OrderSupplier::ThisSP OrderSupplier_{OrderSupplier::Make()};
   OmsOrderTws* MakeOrderImpl() override {
      return this->OrderSupplier_->Alloc();
   }
public:
   UomsOrderTwsFactory()
      : base(fon9::Named{"TwsOrd"}, MakeFieldsT<OmsOrderTwsRaw>()) {
   }
   ~UomsOrderTwsFactory() {
   }
};
//--------------------------------------------------------------------------//
template <class OmsRequestBaseT>
class UomsRequestFactoryT : public OmsRequestFactory {
   fon9_NON_COPY_NON_MOVE(UomsRequestFactoryT);
   using base = OmsRequestFactory;

   struct OmsRequestT : public OmsRequestBaseT {
      fon9_NON_COPY_NON_MOVE(OmsRequestT);
      using base = OmsRequestBaseT;
      using base::base;
      OmsRequestT() = default;
      void NoReadyLineReject(fon9::StrView) override {
      }
   };
   using RequestSupplier = fon9::ObjSupplier<OmsRequestT, kPoolObjCount>;
   typename RequestSupplier::ThisSP RequestTape_{RequestSupplier::Make()};
   OmsRequestSP MakeRequestImpl() override {
      return this->RequestTape_->Alloc();
   }
public:
   UomsRequestFactoryT(std::string name, OmsRequestRunStepSP runStepList)
      : base(std::move(runStepList), fon9::Named(std::move(name)), MakeFieldsT<OmsRequestT>()) {
   }
   UomsRequestFactoryT(std::string name, OmsOrderFactorySP ordFactory, OmsRequestRunStepSP runStepList)
      : base(std::move(ordFactory), std::move(runStepList), fon9::Named(std::move(name)), MakeFieldsT<OmsRequestT>()) {
   }
   ~UomsRequestFactoryT() {
   }
};
using OmsRequestTwsIniFactory = UomsRequestFactoryT<OmsRequestTwsIni>;
using OmsRequestTwsChgFactory = UomsRequestFactoryT<OmsRequestTwsChg>;
using OmsRequestTwsFilledFactory = UomsRequestFactoryT<OmsRequestTwsFilled>;
//--------------------------------------------------------------------------//
struct UomsTwsIniRiskCheck : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(UomsTwsIniRiskCheck);
   using base = OmsRequestRunStep;
   using base::base;

   void RunRequest(OmsRequestRunnerInCore&& runner) override {
      // TODO: 風控檢查.
      assert(dynamic_cast<const OmsRequestTwsIni*>(runner.OrderRaw_.Order_->Initiator_) != nullptr);
      const auto* inidat = static_cast<const OmsRequestTwsIni*>(runner.OrderRaw_.Order_->Initiator_);
      if (static_cast<OmsOrderTwsRaw*>(&runner.OrderRaw_)->OType_ == TwsOType{})
         static_cast<OmsOrderTwsRaw*>(&runner.OrderRaw_)->OType_ = inidat->OType_;
      // 風控成功, 執行下一步驟.
      this->ToNextStep(std::move(runner));
   }
};
struct UomsTwsExgSender : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(UomsTwsExgSender);
   using base = OmsRequestRunStep;
   using base::base;
   UomsTwsExgSender() = default;

   /// 「線路群組」的「櫃號組別Id」, 需透過 OmsResource 取得.
   OmsOrdTeamGroupId TgId_ = 0;
   char              padding_____[4];

   void RunRequest(OmsRequestRunnerInCore&& runner) override {
      // TODO: 排隊 or 送單.
      // 最遲在下單要求送出(交易所)前, 必須編製委託書號.
      if (!runner.AllocOrdNo_IniOrTgid(this->TgId_))
         return;
      if (runner.OrderRaw_.Request_->RxKind() == f9fmkt_RxKind_RequestNew)
         runner.OrderRaw_.OrderSt_ = f9fmkt_OrderSt_NewSending;
      runner.OrderRaw_.RequestSt_ = f9fmkt_TradingRequestSt_Sending;
   }
};
//--------------------------------------------------------------------------//
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
      this->WaitForEndNow();
   }
};
struct UtwsOmsCoreMgr : public fon9::seed::NamedSapling {
   fon9_NON_COPY_NON_MOVE(UtwsOmsCoreMgr);
   using base = fon9::seed::NamedSapling;
   const fon9::seed::MaTreeSP Root_;
   UtwsOmsCoreMgr(fon9::seed::MaTreeSP owner)
      : base(new OmsCoreMgr{"cores"}, "omstws")
      , Root_{std::move(owner)} {
      OmsCoreMgr* coreMgr = static_cast<OmsCoreMgr*>(this->Sapling_.get());
      UomsOrderTwsFactory* ordfac = new UomsOrderTwsFactory;
      coreMgr->SetOrderFactoryPark(new OmsOrderFactoryPark{ordfac});
      coreMgr->SetRequestFactoryPark(new f9omstw::OmsRequestFactoryPark(
         new OmsRequestTwsIniFactory("TwsNew", ordfac,
                                     OmsRequestRunStepSP{new UomsTwsIniRiskCheck(
                                        OmsRequestRunStepSP{new UomsTwsExgSender})}),
         new OmsRequestTwsChgFactory("TwsChg", OmsRequestRunStepSP{new UomsTwsExgSender}),
         new OmsRequestTwsFilledFactory("TwsFilled", nullptr)
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
      if (!static_cast<OmsCoreMgr*>(this->Sapling_.get())->Add(core))
         return false;
      fon9::TimedFileName logfn(fon9::seed::SysEnv_GetLogFileFmtPath(*this->Root_), fon9::TimedFileName::TimeScale::Day);
      logfn.RebuildFileName(tday);
      std::string fname = logfn.GetFileName() + this->Name_ + ".log";
      auto res = core->Start(tday, fname);
      core->SetTitle(std::move(fname));
      if (res.IsError())
         core->SetDescription(fon9::RevPrintTo<std::string>(res));
      return true;
   }

   static bool Create(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
      (void)args;
      UtwsOmsCoreMgr* coreMgr;
      if (!holder.Root_->Add(coreMgr = new UtwsOmsCoreMgr{holder.Root_}))
         return false;
      coreMgr->AddCore(fon9::UtcNow() + fon9::GetLocalTimeZoneOffset());
      return true;
   }
};

} // namespaces
//--------------------------------------------------------------------------//
static fon9::seed::PluginsDesc f9p_UtwsOmsCore{"", &f9omstw::UtwsOmsCoreMgr::Create, nullptr, nullptr,};
static fon9::seed::PluginsPark f9pRegister{"UtwsOmsCore", &f9p_UtwsOmsCore};
