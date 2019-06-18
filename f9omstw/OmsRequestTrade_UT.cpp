// \file f9omstw/OmsRequestTrade_UT.cpp
//
// OmsRequestIni, OmsRequestUpd;
// - 不考慮 OmsCore 的 thread 切換問題.
// - 測試:
//   - 從填妥下單要求之後的 ValidateInUser(), 到 OmsCore 進入下單流程.
//   - 過程中包含相關資源(Symb,Ivac...)的取得.
//   - 包含 Policy 的驗證.
//
// \author fonwinz@gmail.com
#include "apps/f9omstw/UnitTestCore.hpp"
//--------------------------------------------------------------------------//
unsigned gRxHistoryCount = 0;

void CheckExpectedAbandon(f9omstw::OmsRequestBase* req, OmsErrCode abandonErrCode) {
   if (const std::string* abres = req->AbandonReason())
      std::cout << "|msg=" << *abres;
   if (abandonErrCode == OmsErrCode_NoError) {
      if (req->AbandonErrCode() == abandonErrCode && !req->IsAbandoned()) {
      __TEST_OK:
         std::cout << "\r[OK   ]" << std::endl;
         return;
      }
   }
   else if (req->AbandonErrCode() == abandonErrCode && req->IsAbandoned())
      goto __TEST_OK;

   std::cout << "|err=Unexpected abandon"
      << "|req.AbandonErrCode()=" << req->AbandonErrCode()
      << "|req.IsAbandon()= " << req->IsAbandoned();
   std::cout << "\r[ERROR]" << std::endl;
   abort();
}

void TestCase(f9omstw::OmsCore& core, f9omstw::OmsRequestPolicySP pol,
              OmsErrCode abandonErrCode, fon9::StrView reqstr) {
   if (abandonErrCode != OmsErrCode_NoError)
      std::cout << "|expected.ErrCode=" << abandonErrCode;
   auto runner = MakeOmsRequestRunner(core.Owner_->RequestFactoryPark(), reqstr);
   runner.Request_->SetPolicy(pol);
   if (core.MoveToCore(std::move(runner)))
      ++gRxHistoryCount;
   CheckExpectedAbandon(runner.Request_.get(), abandonErrCode);
}
//--------------------------------------------------------------------------//
struct TwsNewChecker : public f9omstw::OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(TwsNewChecker);
   using base = f9omstw::OmsRequestRunStep;
   using base::base;
   TwsNewChecker() = default;

   /// 「線路群組」的「櫃號組別Id」, 需透過 OmsResource 取得.
   f9omstw::OmsOrdTeamGroupId TgId_ = 0;
   char                       padding_____[4];

   void RunRequest(f9omstw::OmsRequestRunnerInCore&& runner) override {
      ++gRxHistoryCount;
      // 最遲在下單要求送出(交易所)前, 必須編製委託書號.
      if (!runner.AllocOrdNo_IniOrTgid(this->TgId_))
         return;
   }
};
struct TwsChgChecker : public f9omstw::OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(TwsChgChecker);
   using base = f9omstw::OmsRequestRunStep;
   using base::base;
   TwsChgChecker() = default;
   void RunRequest(f9omstw::OmsRequestRunnerInCore&&) override {
      ++gRxHistoryCount;
   }
};
//--------------------------------------------------------------------------//
// 檢查 req->PreCheck_OrdKey(); 是否有補填正確的 Market, SessionId.
void TestMarketSessionId(f9omstw::OmsResource& coreResource, f9fmkt_TradingMarket mkt, f9fmkt_TradingSessionId sesid, fon9::StrView reqstr) {
   std::cout << "[TEST ] auto.Market.SessionId."
             << "|expected.Market=" << static_cast<char>(mkt)
             << "|expected.SessionId=" << static_cast<char>(sesid);

   f9omstw::OmsScResource    scRes;
   f9omstw::OmsRequestRunner runner = MakeOmsRequestRunner(coreResource.Core_.Owner_->RequestFactoryPark(), reqstr);
   static_cast<f9omstw::OmsRequestIni*>(runner.Request_.get())->PreCheck_OrdKey(runner, coreResource, scRes);

   if (runner.Request_->Market() != mkt)
      std::cout << "|err=Invalid Market|request.Market=" << static_cast<char>(runner.Request_->Market());
   else if (runner.Request_->SessionId() != sesid)
      std::cout << "|err=Invalid SessionId|request.SessionId=" << static_cast<char>(runner.Request_->SessionId());
   else {
      CheckExpectedAbandon(runner.Request_.get(), OmsErrCode_NoError);
      return;
   }
   std::cout << "\r[ERROR]" << std::endl;
   abort();
}
//--------------------------------------------------------------------------//
int main(int argc, char* argv[]) {
   (void)argc; (void)argv;
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif
   fon9::AutoPrintTestInfo utinfo{"OmsRequestTrade"};

   TestCore testCore(argc, argv);
   auto&    coreResource = testCore.GetResource();
   testCore.Owner_->SetRequestFactoryPark(new f9omstw::OmsRequestFactoryPark(
      new OmsRequestTwsIniFactory("TwsNew", testCore.Owner_->OrderFactoryPark().GetFactory("TwsOrd"), f9omstw::OmsRequestRunStepSP{new TwsNewChecker}),
      new OmsRequestTwsChgFactory("TwsChg", f9omstw::OmsRequestRunStepSP{new TwsChgChecker})
   ));
   testCore.OpenReload(argc, argv, "OmsRequestTrade_UT.log");
   const auto snoStart = coreResource.Backend_.LastSNO();
   std::this_thread::sleep_for(std::chrono::milliseconds{100});
   //---------------------------------------------
   f9omstw::OmsRequestPolicyCfg  polcfg;
   polcfg.TeamGroupName_.assign("admin");
   polcfg.UserRights_.AllowOrdTeams_.assign("*");
   polcfg.IvList_.kfetch(f9omstw::OmsIvKey{"*"}).second = f9omstw::OmsIvRight::AllowTradingAll;
   f9omstw::OmsRequestPolicySP poAdmin = polcfg.MakePolicy(coreResource);
   //---------------------------------------------
   polcfg.TeamGroupName_.assign("PoUserRights.user"); // = PolicyName.PolicyId
   polcfg.UserRights_.AllowOrdTeams_.assign("B");
   polcfg.IvList_.clear();
   polcfg.IvList_.kfetch(f9omstw::OmsIvKey{"8610-10"}).second = f9omstw::OmsIvRight::DenyTradingNew;
   polcfg.IvList_.kfetch(f9omstw::OmsIvKey{"8610-10-sa01"}).second = f9omstw::OmsIvRight::DenyTradingChgPri;
   f9omstw::OmsRequestPolicySP poUser = polcfg.MakePolicy(coreResource);
   //---------------------------------------------
   auto symb = coreResource.Symbs_->FetchSymb("1101");
   symb->TradingMarket_ = f9fmkt_TradingMarket_TwSEC;
   symb->ShUnit_ = 1000;
   TestMarketSessionId(coreResource, f9fmkt_TradingMarket_TwSEC, f9fmkt_TradingSessionId_FixedPrice,
                       "TwsNew|BrkId=8610|IvacNo=10|Symbol=1101|Qty=8000|Pri=");
   TestMarketSessionId(coreResource, f9fmkt_TradingMarket_TwSEC, f9fmkt_TradingSessionId_Normal,
                       "TwsNew|BrkId=8610|IvacNo=10|Symbol=1101|Qty=8000");
   TestMarketSessionId(coreResource, f9fmkt_TradingMarket_TwSEC, f9fmkt_TradingSessionId_OddLot,
                       "TwsNew|BrkId=8610|IvacNo=10|Symbol=1101|Qty=800");
   symb->ShUnit_ = 200;
   TestMarketSessionId(coreResource, f9fmkt_TradingMarket_TwSEC, f9fmkt_TradingSessionId_Normal,
                       "TwsNew|BrkId=8610|IvacNo=10|Symbol=1101|Qty=200");
   TestMarketSessionId(coreResource, f9fmkt_TradingMarket_TwSEC, f9fmkt_TradingSessionId_OddLot,
                       "TwsNew|BrkId=8610|IvacNo=10|Symbol=1101|Qty=100");
   symb->TradingMarket_ = f9fmkt_TradingMarket_TwOTC;
   symb->ShUnit_ = 0; // symb 沒提供 ShUnit_; 測試預設 1張 = 1000股.
   TestMarketSessionId(coreResource, f9fmkt_TradingMarket_TwOTC, f9fmkt_TradingSessionId_OddLot,
                       "TwsNew|BrkId=8610|IvacNo=10|Symbol=1101|Qty=800");
   TestMarketSessionId(coreResource, f9fmkt_TradingMarket_TwOTC, f9fmkt_TradingSessionId_Normal,
                       "TwsNew|BrkId=8610|IvacNo=10|Symbol=1101|Qty=1000");
   //---------------------------------------------
   std::cout << "[TEST ] admin.RequestNew";
   TestCase(testCore, poAdmin, OmsErrCode_NoError,
            "TwsNew|Market=T|SessionId=N|OrdNo=A|BrkId=8610|IvacNo=10|SubacNo=sa01"
            "|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0|SesName=UT|Src=A|UsrDef=UD123|ClOrdId=C12");
   std::cout << "[TEST ] admin.RequestNew(Bad BrkId)";
   TestCase(testCore, poAdmin, OmsErrCode_Bad_BrkId,
            "TwsNew|Market=T|SessionId=N|OrdNo=A|BrkId=861a|IvacNo=10|SubacNo=sa01");

   std::cout << "[TEST ] admin.RequestIni(Cancel A0000)";
   TestCase(testCore, poAdmin, OmsErrCode_NoError,
            "TwsNew|Kind=C|Market=T|SessionId=N|OrdNo=A0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");
   std::cout << "[TEST ] admin.RequestIni(ChgQty A0000)";
   TestCase(testCore, poAdmin, OmsErrCode_NoError,
            "TwsNew|Kind=Q|Market=T|SessionId=N|OrdNo=A0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");

   std::cout << "[TEST ] admin.RequestIni(ChgQty, No OrdNo)";
   TestCase(testCore, poAdmin, OmsErrCode_Bad_OrdNo,
            "TwsNew|Kind=Q|Market=T|SessionId=N|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");
   std::cout << "[TEST ] admin.RequestIni(ChgQty, No Market)";
   TestCase(testCore, poAdmin, OmsErrCode_Bad_MarketId_SymbNotFound,
            "TwsNew|Kind=Q|SessionId=N|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");

   symb = coreResource.Symbs_->FetchSymb("BADMK");
   symb->TradingMarket_ = f9fmkt_TradingMarket_Unknown;
   std::cout << "[TEST ] admin.RequestIni(ChgQty, Bad Symbol market)";
   TestCase(testCore, poAdmin, OmsErrCode_Bad_SymbMarketId,
            "TwsNew|Kind=Q|SessionId=N|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=BADMK|Qty=8000|Pri=84.3|OType=0");

   std::cout << "[TEST ] admin.RequestIni(ChgQty, Bad IvacNo) ";
   TestCase(testCore, poAdmin, OmsErrCode_FieldNotMatch,
            "TwsNew|Market=T|SessionId=N|Kind=Q|OrdNo=A0000|BrkId=8610|IvacNo=21|SubacNo=sa01|Side=B|Symbol=2317");
   std::cout << "[TEST ] admin.RequestIni(ChgQty, Bad SubacNo)";
   TestCase(testCore, poAdmin, OmsErrCode_FieldNotMatch,
            "TwsNew|Market=T|SessionId=N|Kind=Q|OrdNo=A0000|BrkId=8610|IvacNo=10|SubacNo=01|Side=B|Symbol=2317");
   std::cout << "[TEST ] admin.RequestIni(ChgQty, Bad Side)   ";
   TestCase(testCore, poAdmin, OmsErrCode_FieldNotMatch,
            "TwsNew|Market=T|SessionId=N|Kind=Q|OrdNo=A0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=S|Symbol=2317");
   std::cout << "[TEST ] admin.RequestIni(ChgQty, Bad Symbol) ";
   TestCase(testCore, poAdmin, OmsErrCode_FieldNotMatch,
            "TwsNew|Market=T|SessionId=N|Kind=Q|OrdNo=A0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317A");
   std::cout << "[TEST ] admin.RequestIni(ChgQty, Bad OType)  ";
   TestCase(testCore, poAdmin, OmsErrCode_FieldNotMatch,
            "TwsNew|Market=T|SessionId=N|Kind=Q|OrdNo=A0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|OType=5");
   std::cout << "[TEST ] admin.RequestIni(ChgQty, No OType)";
   TestCase(testCore, poAdmin, OmsErrCode_NoError,
            "TwsNew|Market=T|SessionId=N|Kind=Q|OrdNo=A0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317");
   //---------------------------------------------
   std::cout << "[TEST ] admin.RequestIni(Query Z0000)";
   TestCase(testCore, poAdmin, OmsErrCode_NoError,
            "TwsNew|Kind=u|Market=T|SessionId=N|OrdNo=Z0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");
   std::cout << "[TEST ] user.RequestIni(Query Z0000)";
   TestCase(testCore, poUser, OmsErrCode_NoError,
            "TwsNew|Kind=u|Market=T|SessionId=N|OrdNo=Z0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");

   std::cout << "[TEST ] user.RequestIni(Query Z0001)";
   TestCase(testCore, poUser, OmsErrCode_DenyRequestIni,
            "TwsNew|Kind=u|Market=T|SessionId=N|OrdNo=Z0001|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");

   std::cout << "[TEST ] user.RequestIni(ChgPri Z0000)";
   TestCase(testCore, poUser, OmsErrCode_IvNoPermission,
            "TwsNew|Kind=P|Market=T|SessionId=N|OrdNo=Z0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");
   std::cout << "[TEST ] user.RequestUpd(ChgPri Z0000)";
   TestCase(testCore, poUser, OmsErrCode_IvNoPermission,
            "TwsChg|Kind=P|Market=T|SessionId=N|OrdNo=Z0000|BrkId=8610");
   //---------------------------------------------
   std::cout << "[TEST ] Cancel(IniSNO + No OrdKey)                  ";
   TestCase(testCore, poUser, OmsErrCode_NoError, "TwsChg|IniSNO=1");
   std::cout << "[TEST ] Cancel(IniSNO + OrdKey)                     ";
   TestCase(testCore, poUser, OmsErrCode_NoError,       "TwsChg|Market=T|SessionId=N|BrkId=8610|IniSNO=1|OrdNo=A0000");
   std::cout << "[TEST ] OrderNotFound(Bad IniSNO)                   ";
   TestCase(testCore, poUser, OmsErrCode_OrderNotFound, "TwsChg|Market=T|SessionId=N|BrkId=8610|IniSNO=999999999999999");
   std::cout << "[TEST ] OrderNotFound(No IniSNO + Bad OrdKey)       ";
   TestCase(testCore, poUser, OmsErrCode_OrderNotFound, "TwsChg|Market=T|SessionId=N|BrkId=8610|OrdNo=X0000");
   std::cout << "[TEST ] OrderNotFound(IniSNO + Bad OrdKey:Market)   ";
   TestCase(testCore, poUser, OmsErrCode_OrderNotFound, "TwsChg|Market=O|SessionId=N|BrkId=8610|IniSNO=1|OrdNo=A0000");
   std::cout << "[TEST ] OrderNotFound(IniSNO + Bad OrdKey:SessionId)";
   TestCase(testCore, poUser, OmsErrCode_OrderNotFound, "TwsChg|Market=T|SessionId=F|BrkId=8610|IniSNO=1|OrdNo=A0000");
   std::cout << "[TEST ] OrderNotFound(IniSNO + Bad OrdKey:BrkId)    ";
   TestCase(testCore, poUser, OmsErrCode_OrderNotFound, "TwsChg|Market=T|SessionId=N|BrkId=8611|IniSNO=1|OrdNo=A0000");
   std::cout << "[TEST ] OrderNotFound(IniSNO + Bad OrdKey:OrdNo)    ";
   TestCase(testCore, poUser, OmsErrCode_OrderNotFound, "TwsChg|Market=T|SessionId=N|BrkId=8610|IniSNO=1|OrdNo=B0000");
   //---------------------------------------------
   std::cout << "[TEST ] user.RequestNew";
   TestCase(testCore, poUser, OmsErrCode_NoError, "TwsNew|Market=T|SessionId=N|BrkId=8610|IvacNo=10|SubacNo=sa01");
   std::cout << "[TEST ] user.RequestNew(IvNoPermission)";
   TestCase(testCore, poUser, OmsErrCode_IvNoPermission, "TwsNew|Market=T|SessionId=N|BrkId=8610|IvacNo=10");
   std::cout << "[TEST ] user.RequestNew+OrdNo";
   TestCase(testCore, poUser, OmsErrCode_OrdNoMustEmpty, "TwsNew|Market=T|SessionId=N|BrkId=8610|OrdNo=B");
   //---------------------------------------------
   const auto snoCount = coreResource.Backend_.LastSNO() - snoStart;
   std::cout << "[TEST ] RxHistoryCount|expected=" << gRxHistoryCount;
   if (snoCount != gRxHistoryCount) {
      std::cout << "|coreBackend=" << snoCount << "\r[ERROR]" << std::endl;
      abort();
   }
   std::cout << "\r[OK   ]" << std::endl;
}
