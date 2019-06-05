// \file f9omstw/OmsReqOrd_UT.cpp
//
// 測試 OmsRequest/OmsOrder 的設計.
//
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "apps/f9omstw/UnitTestCore.hpp"
//--------------------------------------------------------------------------//
struct UomsTwsIniRiskCheck : public f9omstw::OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(UomsTwsIniRiskCheck);
   using base = f9omstw::OmsRequestRunStep;
   using base::base;
   void RunRequest(f9omstw::OmsRequestRunnerInCore&&) override {
   }
};
struct UomsTwsExgSender : public f9omstw::OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(UomsTwsExgSender);
   using base = f9omstw::OmsRequestRunStep;
   using base::base;
   void RunRequest(f9omstw::OmsRequestRunnerInCore&&) override {
      // Test: 送單狀態, 1 ms 之後 Accepted.
   }
};
//--------------------------------------------------------------------------//
void TestRequestNewInCore(f9omstw::OmsResource& res, f9omstw::OmsOrderFactory& ordFactory, f9omstw::OmsRequestRunner&& reqr) {
   // 開始新單流程:
   // 1. 設定 req 序號.
   res.PutRequestId(*reqr.Request_);

   // 2. OmsRequestIni 基本檢查, 過程中可能需要取得資源: 商品、投資人帳號、投資人商品資料...
   //    - 如果失敗 req.Abandon(), 使用回報機制通知 user(io session), 回報訂閱者需判斷 req.UserId_;
   f9omstw::OmsRequestIni* curReq = static_cast<f9omstw::OmsRequestTwsIni*>(reqr.Request_.get());

   static unsigned gTestCount = 0;
   // abandon request:
   if (++gTestCount % 10 == 0)
      return reqr.RequestAbandon(&res, OmsErrCode_IvNoPermission);

   f9omstw::OmsScResource        scRes;
   const f9omstw::OmsRequestIni* iniReq = curReq->PreCheck_OrdKey(reqr, res, scRes);
   if (iniReq == nullptr)
      return;

   f9omstw::OmsOrderRaw* ordraw;
   if (iniReq == reqr.Request_.get()) {
      if (!iniReq->PreCheck_IvRight(reqr, res, scRes))
         return;
      ordraw = ordFactory.MakeOrder(*curReq, &scRes);
   }
   else {
      f9omstw::OmsOrder* ord = iniReq->LastUpdated()->Order_;
      f9omstw::OmsScResource& ordScRes = ord->ScResource();
      ordScRes.CheckMoveFrom(std::move(scRes));
      if (!iniReq->PreCheck_IvRight(reqr, res, ordScRes))
         return;
      ordraw = ordFactory.MakeOrderRaw(*ord, *reqr.Request_);
   }

   // 3. 建立委託書, 開始執行「下單要求」步驟.
   //    開始 OmsRequestRunnerInCore 之後, req 就不能再變動了!
   //    接下來 req 將被凍結 , 除了 LastUpdated_ 不會再有異動.
   f9omstw::OmsRequestRunnerInCore  runner{res, *ordraw, std::move(reqr.ExLog_), 256};

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

   // 4. 風控檢查.
   // 5. 排隊 or 送單.
   runner.OrderRaw_.OrderSt_ = f9fmkt_OrderSt_NewSending;
   runner.OrderRaw_.RequestSt_ = f9fmkt_TradingRequestSt_Sending;
}
void TestRequestChgInCore(f9omstw::OmsResource& res, f9omstw::OmsOrderFactory& ordFactory, f9omstw::OmsRequestRunner&& reqr) {
   res.PutRequestId(*reqr.Request_);
   f9omstw::OmsRequestUpd*        curReq = static_cast<f9omstw::OmsRequestUpd*>(reqr.Request_.get());
   const f9omstw::OmsRequestBase* iniReq = curReq->PreCheck_GetRequestInitiator(reqr, res);
   if (iniReq == nullptr)
      return;
   f9omstw::OmsOrder* ord = iniReq->LastUpdated()->Order_;
   if (!ord->Initiator_->PreCheck_IvRight(reqr, res))
      return;

   f9omstw::OmsRequestRunnerInCore  runner{res,
      *ordFactory.MakeOrderRaw(*ord, *reqr.Request_),
      std::move(reqr.ExLog_), 256};

   runner.OrderRaw_.RequestSt_ = f9fmkt_TradingRequestSt_Sent;
}
//--------------------------------------------------------------------------//
int main(int argc, char* argv[]) {
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif
   fon9::AutoPrintTestInfo utinfo{"OmsRequest/OmsOrder"};
   fon9::StopWatch         stopWatch;

   const auto  isWait = fon9::GetCmdArg(argc, argv, "w", "wait");
   unsigned    testTimes = fon9::StrTo(fon9::GetCmdArg(argc, argv, "c", "count"), 0u);
   if (testTimes <= 0)
      testTimes = kPoolObjCount * 2;

   TestCore testCore(argc, argv);
   auto&    coreResource = testCore.GetResource();
   coreResource.RequestFacPark_.reset(new f9omstw::OmsRequestFactoryPark(
      new OmsRequestTwsIniFactory("TwsNew",
                                  f9omstw::OmsRequestRunStepSP{new UomsTwsIniRiskCheck(testCore,
                                   f9omstw::OmsRequestRunStepSP{new UomsTwsExgSender{testCore}})}),
      new OmsRequestTwsChgFactory("TwsChg", nullptr),
      new OmsRequestTwsMatchFactory("TwsMat", nullptr)
   ));
   testCore.OpenReload(argc, argv, "OmsReqOrd_UT.log");
   std::this_thread::sleep_for(std::chrono::milliseconds{100});

   // 下單欄位
   // --------
   // UserId, FromIp 由 io-session 填入.
   // SalesNo, Src 各券商的處理方式不盡相同, 可能提供者:
   //    - Client 端 Ap 或 user;
   //    - io-session 根據各種情況: UserId 或 FromIp 或...
   // IvacNoFlag 台灣證交所要求提供的欄位, 其實與投資人帳號關連不大, 應屬於下單來源.

   f9omstw::OmsRequestPolicy*    reqPolicy;
   f9omstw::OmsRequestPolicySP   reqPolicySP{reqPolicy = new f9omstw::OmsRequestPolicy{}};
   reqPolicy->SetOrdTeamGroupCfg(&coreResource.OrdTeamGroupMgr_.SetTeamGroup("admin", "*"));
   reqPolicy->AddIvRights(nullptr, nullptr, f9omstw::OmsIvRight::AllowTradingAll);
   //---------------------------------------------
   stopWatch.ResetTimer();
   for (unsigned L = 0; L < testTimes; ++L) {
      // Create Request & 填入內容;
      auto reqNew = MakeOmsRequestRunner(*coreResource.RequestFacPark_,
                                        "TwsNew|Market=T|SessionId=N|SesName=UT|Src=A|UsrDef=UD123|ClOrdId=C12|OrdNo=A|"
                                        "IvacNo=10|SubacNo=sa01|BrkId=8610|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");
      reqNew.Request_->SetPolicy(reqPolicySP);
      if (!reqNew.ValidateInUser()) {
         fprintf(stderr, "ValidateInUser|err=%s\n", fon9::BufferTo<std::string>(reqNew.ExLog_.MoveOut()).c_str());
         abort();
      }
      // 模擬已將 reqNew 丟給 OmsCore, 已進入 OmsCore 之後的步驟.
      TestRequestNewInCore(coreResource, *coreResource.OrderFacPark_->GetFactory(0), std::move(reqNew));
   }
   stopWatch.PrintResult("ReqNew ", testTimes);
   //---------------------------------------------
   stopWatch.ResetTimer();
   for (unsigned L = 0; L < testTimes; ++L) {
      fon9::RevBufferFixedSize<1024> rbuf;
      fon9::RevPrint(rbuf, "TwsChg|IniSNO=", L, "|Market=T|SessionId=N|SesName=UT|Src=B|UsrDef=UD234|ClOrdId=C34"
                     "|BrkId=8610|OrdNo=|Qty=-1000");
      auto req = MakeOmsRequestRunner(*coreResource.RequestFacPark_, ToStrView(rbuf));
      if (!req.ValidateInUser()) {
         fprintf(stderr, "ValidateInUser|err=%s\n", fon9::BufferTo<std::string>(req.ExLog_.MoveOut()).c_str());
         abort();
      }
      TestRequestChgInCore(coreResource, *coreResource.OrderFacPark_->GetFactory(0), std::move(req));
   }
   stopWatch.PrintResult("ReqChg ", testTimes);
   //---------------------------------------------
   f9omstw::OmsOrdNo ordno{"A0000"};
   stopWatch.ResetTimer();
   for (unsigned L = 0; L < testTimes; ++L) {
      fon9::RevBufferFixedSize<1024> rbuf;
      fon9::RevPrint(rbuf, "TwsChg|Market=T|SessionId=N|SesName=UT|Src=B|UsrDef=UD234|ClOrdId=C34"
                     "|BrkId=8610|OrdNo=", ordno, "|Qty=0");
      auto req = MakeOmsRequestRunner(*coreResource.RequestFacPark_, ToStrView(rbuf));
      if (!req.ValidateInUser()) {
         fprintf(stderr, "ValidateInUser|err=%s\n", fon9::BufferTo<std::string>(req.ExLog_.MoveOut()).c_str());
         abort();
      }
      TestRequestChgInCore(coreResource, *coreResource.OrderFacPark_->GetFactory(0), std::move(req));
      f9omstw::IncStrAlpha(ordno.begin(), ordno.end());
   }
   stopWatch.PrintResult("ReqDel ", testTimes);
   //---------------------------------------------
   std::cout << "LastSNO = " << coreResource.Backend_.LastSNO() << std::endl;
   if (isWait.begin() && (isWait.empty() || fon9::toupper(static_cast<unsigned char>(*isWait.begin())) == 'Y')) {
      // 要查看資源用量(時間、記憶體...), 可透過 `/usr/bin/time` 指令:
      //    /usr/bin/time --verbose ~/devel/output/f9omstw/release/f9omstw/OmsReqOrd_UT
      // 或在結束前先暫停, 在透過其他外部工具查看:
      //    ~/devel/output/f9omstw/release/f9omstw/OmsReqOrd_UT -w
      //    例如: $cat /proc/19420(pid)/status 查看 VmSize
      std::cout << "Press <Enter> to quit.";
      getchar();
   }
}
