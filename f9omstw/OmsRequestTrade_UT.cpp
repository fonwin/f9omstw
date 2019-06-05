// \file f9omstw/OmsRequestTrade_UT.cpp
//
// OmsRequestIni, OmsRequestUpd;
// - 不考慮 OmsCore 的 thread 切換問題.
// - 測試:
//   - 從填妥下單要求之後的 ValidateInUser(), 到 OmsCore 進入下單流程前的過程.
//   - 過程中包含相關資源(Symb,Ivac...)的取得.
//   - 包含 Policy 的驗證.
//
// \author fonwinz@gmail.com
#include "apps/f9omstw/UnitTestCore.hpp"
//--------------------------------------------------------------------------//
namespace f9omstw {

/// 在 user thread 登入後, 取得權限設定,
/// 然後到 OmsCore 透過 MakePolicyInCore() 建立 OmsRequestPolicySP;
struct OmsRequestPolicyCfg {
   fon9::CharVector  TeamGroupName_;
   fon9::CharVector  TeamGroupCfg_;
   OmsIvList         IvList_;
};

OmsRequestPolicySP MakePolicyInCore(const OmsRequestPolicyCfg& src, OmsResource& res) {
   OmsRequestPolicy* pol = new OmsRequestPolicy{};
   pol->SetOrdTeamGroupCfg(&res.OrdTeamGroupMgr_.SetTeamGroup(ToStrView(src.TeamGroupName_), ToStrView(src.TeamGroupCfg_)));
   for (const auto& item : src.IvList_) {
      auto ec = OmsAddIvRights(*pol, ToStrView(item.first), item.second, *res.Brks_);
      if (ec != OmsIvKind::Unknown)
         fon9_LOG_ERROR("MakePolicyInCore|IvKey=", item.first, "|ec=", ec);
   }
   return pol;
}

} // namespace f9omstw
//--------------------------------------------------------------------------//
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
void TestCase(f9omstw::OmsResource& coreResource, f9omstw::OmsRequestPolicySP pol,
              void (*fnTestRequestInCore)(f9omstw::OmsResource&, f9omstw::OmsRequestRunner&&, OmsErrCode abandonErrCode),
              OmsErrCode abandonErrCode, fon9::StrView reqstr) {
   if (abandonErrCode != OmsErrCode_NoError)
      std::cout << "|expected.ErrCode=" << abandonErrCode;
   auto reqr = MakeOmsRequestRunner(*coreResource.RequestFacPark_, reqstr);
   reqr.Request_->SetPolicy(pol);
   if (!reqr.ValidateInUser()) {
      CheckExpectedAbandon(reqr.Request_.get(), abandonErrCode);
      return;
   }
   fnTestRequestInCore(coreResource, std::move(reqr), abandonErrCode);
}
//--------------------------------------------------------------------------//
unsigned gRxHistoryCount = 0;

void TestRequestNewInCore(f9omstw::OmsResource& res, f9omstw::OmsRequestRunner&& reqr, OmsErrCode abandonErrCode) {
   ++gRxHistoryCount;
   // 開始新單流程:
   // 1. 設定 req 序號.
   res.PutRequestId(*reqr.Request_);

   // 2. OmsRequestIni 基本檢查, 過程中可能需要取得資源: 商品、投資人帳號、投資人商品資料...
   //    - 如果失敗 req.Abandon(), 使用回報機制通知 user(io session), 回報訂閱者需判斷 req.UserId_;
   f9omstw::OmsRequestIni* curReq = static_cast<f9omstw::OmsRequestTwsIni*>(reqr.Request_.get());
   f9omstw::OmsScResource  scRes;

   const f9omstw::OmsRequestIni* iniReq = curReq->PreCheck_OrdKey(reqr, res, scRes);
   if (iniReq == nullptr) {
      CheckExpectedAbandon(curReq, abandonErrCode);
      return;
   }
   f9omstw::OmsOrderRaw*      ordraw;
   f9omstw::OmsOrderFactory&  ordFactory = *res.OrderFacPark_->GetFactory(0);
   if (iniReq == reqr.Request_.get()) {
      if (!iniReq->PreCheck_IvRight(reqr, res, scRes)) {
         CheckExpectedAbandon(curReq, abandonErrCode);
         return;
      }
      CheckExpectedAbandon(curReq, abandonErrCode);
      ordraw = ordFactory.MakeOrder(*curReq, &scRes);
   }
   else {
      f9omstw::OmsOrder* ord = iniReq->LastUpdated()->Order_;
      f9omstw::OmsScResource& ordScRes = ord->ScResource();
      ordScRes.CheckMoveFrom(std::move(scRes));
      if (!iniReq->PreCheck_IvRight(reqr, res, ordScRes)) {
         CheckExpectedAbandon(curReq, abandonErrCode);
         return;
      }
      CheckExpectedAbandon(curReq, abandonErrCode);
      ordraw = ordFactory.MakeOrderRaw(*ord, *reqr.Request_);
   }

   // 3. 建立委託書, 開始執行「下單要求」步驟.
   //    開始 OmsRequestRunnerInCore 之後, req 就不能再變動了!
   //    接下來 req 將被凍結 , 除了 LastUpdated_ 不會再有異動.
   f9omstw::OmsRequestRunnerInCore  runner{res, *ordraw, std::move(reqr.ExLog_), 256};
   ++gRxHistoryCount;

   // 分配委託書號.
   f9omstw::OmsBrk* brk = runner.OrderRaw_.Order_->GetBrk(runner.Resource_);  assert(brk);
   if (f9omstw::OmsOrdNoMap* ordNoMap = brk->GetOrdNoMap(*curReq)) {
      if (!ordNoMap->AllocOrdNo(runner, curReq->OrdNo_))
         return;
   }
   else {
      f9omstw::OmsOrdNoMap::Reject(runner, OmsErrCode_OrdNoMapNotFound);
      return;
   }
}
void TestRequestChgInCore(f9omstw::OmsResource& res, f9omstw::OmsRequestRunner&& reqr, OmsErrCode abandonErrCode) {
   ++gRxHistoryCount;
   res.PutRequestId(*reqr.Request_);
   f9omstw::OmsRequestUpd*        curReq = static_cast<f9omstw::OmsRequestUpd*>(reqr.Request_.get());
   const f9omstw::OmsRequestBase* iniReq = curReq->PreCheck_GetRequestInitiator(reqr, res);
   if (iniReq == nullptr) {
      CheckExpectedAbandon(curReq, abandonErrCode);
      return;
   }
   f9omstw::OmsOrder* ord = iniReq->LastUpdated()->Order_;
   if (!ord->Initiator_->PreCheck_IvRight(reqr, res)) {
      CheckExpectedAbandon(curReq, abandonErrCode);
      return;
   }
   CheckExpectedAbandon(curReq, abandonErrCode);

   f9omstw::OmsOrderFactory&        ordFactory = *res.OrderFacPark_->GetFactory(0);
   f9omstw::OmsRequestRunnerInCore  runner{res,
      *ordFactory.MakeOrderRaw(*ord, *reqr.Request_),
      std::move(reqr.ExLog_), 256};
   ++gRxHistoryCount;
}
//--------------------------------------------------------------------------//
f9fmkt_TradingMarket    gExpectedMarket;
f9fmkt_TradingSessionId gExpectedSessionId;
void TestRequestMarketSessionId(f9omstw::OmsResource& res, f9omstw::OmsRequestRunner&& reqr, OmsErrCode abandonErrCode) {
   std::cout << "|expected.Market=" << static_cast<char>(gExpectedMarket)
             << "|expected.SessionId=" << static_cast<char>(gExpectedSessionId);

   f9omstw::OmsScResource  scRes;
   static_cast<f9omstw::OmsRequestIni*>(reqr.Request_.get())->PreCheck_OrdKey(reqr, res, scRes);

   if (reqr.Request_->Market() != gExpectedMarket)
      std::cout << "|err=Invalid Market|request.Market=" << static_cast<char>(reqr.Request_->Market());
   else if (reqr.Request_->SessionId() != gExpectedSessionId)
      std::cout << "|err=Invalid SessionId|request.SessionId=" << static_cast<char>(reqr.Request_->SessionId());
   else {
      CheckExpectedAbandon(reqr.Request_.get(), abandonErrCode);
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
   coreResource.RequestFacPark_.reset(new f9omstw::OmsRequestFactoryPark(
      new OmsRequestTwsIniFactory("TwsNew", nullptr),
      new OmsRequestTwsChgFactory("TwsChg", nullptr)
   ));
   testCore.OpenReload(argc, argv, "ReqTradeUT.log");
   const auto snoStart = coreResource.Backend_.LastSNO();
   std::this_thread::sleep_for(std::chrono::milliseconds{100});
   //---------------------------------------------
   f9omstw::OmsRequestPolicyCfg  poCfg;
   poCfg.TeamGroupName_.assign("admin");
   poCfg.TeamGroupCfg_.assign("*");
   poCfg.IvList_.kfetch(f9omstw::OmsIvKey{"*"}).second = f9omstw::OmsIvRight::AllowTradingAll;
   f9omstw::OmsRequestPolicySP   poAdmin{f9omstw::MakePolicyInCore(poCfg, coreResource)};
   //---------------------------------------------
   poCfg.TeamGroupName_.assign("PoUserRights.user"); // = PolicyName.PolicyId
   poCfg.TeamGroupCfg_.assign("B");
   poCfg.IvList_.clear();
   poCfg.IvList_.kfetch(f9omstw::OmsIvKey{"8610-10"}).second = f9omstw::OmsIvRight::DenyTradingNew;
   poCfg.IvList_.kfetch(f9omstw::OmsIvKey{"8610-10-sa01"}).second = f9omstw::OmsIvRight::DenyTradingChgPri;
   f9omstw::OmsRequestPolicySP   poUser{f9omstw::MakePolicyInCore(poCfg, coreResource)};
   //---------------------------------------------
   auto symb = coreResource.Symbs_->FetchSymb("1101");
   symb->TradingMarket_ = gExpectedMarket = f9fmkt_TradingMarket_TwSEC;
   symb->ShUnit_ = 1000;
   gExpectedSessionId = f9fmkt_TradingSessionId_FixedPrice;
   std::cout << "[TEST ] auto.Market.SessionId.";
   TestCase(coreResource, poAdmin, &TestRequestMarketSessionId, OmsErrCode_NoError, "TwsNew|BrkId=8610|IvacNo=10|Symbol=1101|Qty=8000|Pri=");
   gExpectedSessionId = f9fmkt_TradingSessionId_Normal;
   std::cout << "[TEST ] auto.Market.SessionId.";
   TestCase(coreResource, poAdmin, &TestRequestMarketSessionId, OmsErrCode_NoError, "TwsNew|BrkId=8610|IvacNo=10|Symbol=1101|Qty=8000");
   gExpectedSessionId = f9fmkt_TradingSessionId_OddLot;
   std::cout << "[TEST ] auto.Market.SessionId.";
   TestCase(coreResource, poAdmin, &TestRequestMarketSessionId, OmsErrCode_NoError, "TwsNew|BrkId=8610|IvacNo=10|Symbol=1101|Qty=800");

   symb->ShUnit_ = 200;
   gExpectedSessionId = f9fmkt_TradingSessionId_Normal;
   std::cout << "[TEST ] auto.Market.SessionId.";
   TestCase(coreResource, poAdmin, &TestRequestMarketSessionId, OmsErrCode_NoError, "TwsNew|BrkId=8610|IvacNo=10|Symbol=1101|Qty=200");
   gExpectedSessionId = f9fmkt_TradingSessionId_OddLot;
   std::cout << "[TEST ] auto.Market.SessionId.";
   TestCase(coreResource, poAdmin, &TestRequestMarketSessionId, OmsErrCode_NoError, "TwsNew|BrkId=8610|IvacNo=10|Symbol=1101|Qty=100");

   symb->TradingMarket_ = gExpectedMarket = f9fmkt_TradingMarket_TwOTC;
   symb->ShUnit_ = 0; // symb 沒提供 ShUnit_; 測試預設 1張 = 1000股.
   gExpectedSessionId = f9fmkt_TradingSessionId_OddLot;
   std::cout << "[TEST ] auto.Market.SessionId.";
   TestCase(coreResource, poAdmin, &TestRequestMarketSessionId, OmsErrCode_NoError, "TwsNew|BrkId=8610|IvacNo=10|Symbol=1101|Qty=800");
   gExpectedSessionId = f9fmkt_TradingSessionId_Normal;
   std::cout << "[TEST ] auto.Market.SessionId.";
   TestCase(coreResource, poAdmin, &TestRequestMarketSessionId, OmsErrCode_NoError, "TwsNew|BrkId=8610|IvacNo=10|Symbol=1101|Qty=1000");

   //---------------------------------------------
   std::cout << "[TEST ] admin.RequestNew";
   TestCase(coreResource, poAdmin, &TestRequestNewInCore, OmsErrCode_NoError,
            "TwsNew|Market=T|SessionId=N|OrdNo=A|BrkId=8610|IvacNo=10|SubacNo=sa01"
            "|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0|SesName=UT|Src=A|UsrDef=UD123|ClOrdId=C12");
   std::cout << "[TEST ] admin.RequestNew(Bad BrkId)";
   TestCase(coreResource, poAdmin, &TestRequestNewInCore, OmsErrCode_Bad_BrkId,
            "TwsNew|Market=T|SessionId=N|OrdNo=A|BrkId=861a|IvacNo=10|SubacNo=sa01");

   std::cout << "[TEST ] admin.RequestIni(Cancel A0000)";
   TestCase(coreResource, poAdmin, &TestRequestNewInCore, OmsErrCode_NoError,
            "TwsNew|Kind=C|Market=T|SessionId=N|OrdNo=A0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");
   std::cout << "[TEST ] admin.RequestIni(ChgQty A0000)";
   TestCase(coreResource, poAdmin, &TestRequestNewInCore, OmsErrCode_NoError,
            "TwsNew|Kind=Q|Market=T|SessionId=N|OrdNo=A0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");

   std::cout << "[TEST ] admin.RequestIni(ChgQty, No OrdNo)";
   TestCase(coreResource, poAdmin, &TestRequestNewInCore, OmsErrCode_Bad_OrdNo,
            "TwsNew|Kind=Q|Market=T|SessionId=N|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");
   std::cout << "[TEST ] admin.RequestIni(ChgQty, No Market)";
   TestCase(coreResource, poAdmin, &TestRequestNewInCore, OmsErrCode_Bad_MarketId_SymbNotFound,
            "TwsNew|Kind=Q|SessionId=N|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");

   symb = coreResource.Symbs_->FetchSymb("BADMK");
   symb->TradingMarket_ = f9fmkt_TradingMarket_Unknown;
   std::cout << "[TEST ] admin.RequestIni(ChgQty, Bad Symbol market)";
   TestCase(coreResource, poAdmin, &TestRequestNewInCore, OmsErrCode_Bad_SymbMarketId,
            "TwsNew|Kind=Q|SessionId=N|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=BADMK|Qty=8000|Pri=84.3|OType=0");

   std::cout << "[TEST ] admin.RequestIni(ChgQty, Bad IvacNo) ";
   TestCase(coreResource, poAdmin, &TestRequestNewInCore, OmsErrCode_FieldNotMatch,
            "TwsNew|Market=T|SessionId=N|Kind=Q|OrdNo=A0000|BrkId=8610|IvacNo=21|SubacNo=sa01|Side=B|Symbol=2317");
   std::cout << "[TEST ] admin.RequestIni(ChgQty, Bad SubacNo)";
   TestCase(coreResource, poAdmin, &TestRequestNewInCore, OmsErrCode_FieldNotMatch,
            "TwsNew|Market=T|SessionId=N|Kind=Q|OrdNo=A0000|BrkId=8610|IvacNo=10|SubacNo=01|Side=B|Symbol=2317");
   std::cout << "[TEST ] admin.RequestIni(ChgQty, Bad Side)   ";
   TestCase(coreResource, poAdmin, &TestRequestNewInCore, OmsErrCode_FieldNotMatch,
            "TwsNew|Market=T|SessionId=N|Kind=Q|OrdNo=A0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=S|Symbol=2317");
   std::cout << "[TEST ] admin.RequestIni(ChgQty, Bad Symbol) ";
   TestCase(coreResource, poAdmin, &TestRequestNewInCore, OmsErrCode_FieldNotMatch,
            "TwsNew|Market=T|SessionId=N|Kind=Q|OrdNo=A0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317A");
   std::cout << "[TEST ] admin.RequestIni(ChgQty, Bad OType)  ";
   TestCase(coreResource, poAdmin, &TestRequestNewInCore, OmsErrCode_FieldNotMatch,
            "TwsNew|Market=T|SessionId=N|Kind=Q|OrdNo=A0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|OType=5");
   std::cout << "[TEST ] admin.RequestIni(ChgQty, No OType)";
   TestCase(coreResource, poAdmin, &TestRequestNewInCore, OmsErrCode_NoError,
            "TwsNew|Market=T|SessionId=N|Kind=Q|OrdNo=A0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317");
   //---------------------------------------------
   std::cout << "[TEST ] admin.RequestIni(Query Z0000)";
   TestCase(coreResource, poAdmin, &TestRequestNewInCore, OmsErrCode_NoError,
            "TwsNew|Kind=u|Market=T|SessionId=N|OrdNo=Z0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");
   std::cout << "[TEST ] user.RequestIni(Query Z0000)";
   TestCase(coreResource, poUser, &TestRequestNewInCore, OmsErrCode_NoError,
            "TwsNew|Kind=u|Market=T|SessionId=N|OrdNo=Z0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");

   std::cout << "[TEST ] user.RequestIni(Query Z0001)";
   TestCase(coreResource, poUser, &TestRequestNewInCore, OmsErrCode_DenyRequestIni,
            "TwsNew|Kind=u|Market=T|SessionId=N|OrdNo=Z0001|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");

   std::cout << "[TEST ] user.RequestIni(ChgPri Z0000)";
   TestCase(coreResource, poUser, &TestRequestNewInCore, OmsErrCode_IvNoPermission,
            "TwsNew|Kind=P|Market=T|SessionId=N|OrdNo=Z0000|BrkId=8610|IvacNo=10|SubacNo=sa01|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");
   std::cout << "[TEST ] user.RequestUpd(ChgPri Z0000)";
   TestCase(coreResource, poUser, &TestRequestChgInCore, OmsErrCode_IvNoPermission,
            "TwsChg|Kind=P|Market=T|SessionId=N|OrdNo=Z0000|BrkId=8610");
   //---------------------------------------------
   std::cout << "[TEST ] Cancel(IniSNO + No OrdKey)                  ";
   TestCase(coreResource, poUser, &TestRequestChgInCore, OmsErrCode_NoError, "TwsChg|IniSNO=1");
   std::cout << "[TEST ] Cancel(IniSNO + OrdKey)                     ";
   TestCase(coreResource, poUser, &TestRequestChgInCore, OmsErrCode_NoError,       "TwsChg|Market=T|SessionId=N|BrkId=8610|IniSNO=1|OrdNo=A0000");
   std::cout << "[TEST ] OrderNotFound(Bad IniSNO)                   ";
   TestCase(coreResource, poUser, &TestRequestChgInCore, OmsErrCode_OrderNotFound, "TwsChg|Market=T|SessionId=N|BrkId=8610|IniSNO=999999999999999");
   std::cout << "[TEST ] OrderNotFound(No IniSNO + Bad OrdKey)       ";
   TestCase(coreResource, poUser, &TestRequestChgInCore, OmsErrCode_OrderNotFound, "TwsChg|Market=T|SessionId=N|BrkId=8610|OrdNo=X0000");
   std::cout << "[TEST ] OrderNotFound(IniSNO + Bad OrdKey:Market)   ";
   TestCase(coreResource, poUser, &TestRequestChgInCore, OmsErrCode_OrderNotFound, "TwsChg|Market=O|SessionId=N|BrkId=8610|IniSNO=1|OrdNo=A0000");
   std::cout << "[TEST ] OrderNotFound(IniSNO + Bad OrdKey:SessionId)";
   TestCase(coreResource, poUser, &TestRequestChgInCore, OmsErrCode_OrderNotFound, "TwsChg|Market=T|SessionId=F|BrkId=8610|IniSNO=1|OrdNo=A0000");
   std::cout << "[TEST ] OrderNotFound(IniSNO + Bad OrdKey:BrkId)    ";
   TestCase(coreResource, poUser, &TestRequestChgInCore, OmsErrCode_OrderNotFound, "TwsChg|Market=T|SessionId=N|BrkId=8611|IniSNO=1|OrdNo=A0000");
   std::cout << "[TEST ] OrderNotFound(IniSNO + Bad OrdKey:OrdNo)    ";
   TestCase(coreResource, poUser, &TestRequestChgInCore, OmsErrCode_OrderNotFound, "TwsChg|Market=T|SessionId=N|BrkId=8610|IniSNO=1|OrdNo=B0000");
   //---------------------------------------------
   std::cout << "[TEST ] user.RequestNew";
   TestCase(coreResource, poUser, &TestRequestNewInCore, OmsErrCode_NoError, "TwsNew|Market=T|SessionId=N|BrkId=8610|IvacNo=10|SubacNo=sa01");
   std::cout << "[TEST ] user.RequestNew(IvNoPermission)";
   TestCase(coreResource, poUser, &TestRequestNewInCore, OmsErrCode_IvNoPermission, "TwsNew|Market=T|SessionId=N|BrkId=8610|IvacNo=10");
   std::cout << "[TEST ] user.RequestNew+OrdNo";
   TestCase(coreResource, poUser, &TestRequestNewInCore, OmsErrCode_OrdNoMustEmpty, "TwsNew|Market=T|SessionId=N|BrkId=8610|OrdNo=B");
   //---------------------------------------------
   const auto snoCount = coreResource.Backend_.LastSNO() - snoStart;
   std::cout << "[TEST ] RxHistoryCount|expected=" << gRxHistoryCount;
   if (snoCount != gRxHistoryCount) {
      std::cout << "|coreBackend=" << snoCount << "\r[ERROR]" << std::endl;
      abort();
   }
   std::cout << "\r[OK   ]" << std::endl;
}
