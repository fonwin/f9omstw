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
   void RunRequest(f9omstw::OmsRequestRunnerInCore&& runner) override {
      // TODO: 風控檢查.
      // 風控成功, 執行下一步驟.
      this->ToNextStep(std::move(runner));
   }
};
struct UomsTwsExgSender : public f9omstw::OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(UomsTwsExgSender);
   using base = f9omstw::OmsRequestRunStep;
   using base::base;
   /// 「線路群組」的「櫃號組別Id」, 需透過 OmsResource 取得.
   f9omstw::OmsOrdTeamGroupId TgId_ = 0;
   char  padding_____[4];

   void RunRequest(f9omstw::OmsRequestRunnerInCore&& runner) override {
      // 排隊 or 送單.
      // 最遲在下單要求送出(交易所)前, 必須編製委託書號.
      if (!runner.AllocOrdNo_IniOrTgid(this->TgId_))
         return;
      if (runner.OrderRaw_.Request_->RxKind() == f9fmkt_RxKind_RequestNew)
         runner.OrderRaw_.OrderSt_ = f9fmkt_OrderSt_NewSending;
      runner.OrderRaw_.RequestSt_ = f9fmkt_TradingRequestSt_Sending;
      // TODO: Test 送單狀態, 1 ms 之後 Accepted.
   }
};
//--------------------------------------------------------------------------//
void TestCase(f9omstw::OmsCore& core, f9omstw::OmsRequestPolicySP policy, fon9::StrView reqstr) {
   auto req = MakeOmsRequestRunner(core.RequestFactoryPark(), reqstr);
   req.Request_->SetPolicy(std::move(policy));
   core.MoveToCore(std::move(req));
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
   coreResource.RequestFactoryPark_.reset(new f9omstw::OmsRequestFactoryPark(
      new OmsRequestTwsIniFactory("TwsNew", coreResource.OrderFactoryPark_->GetFactory("TwsOrd"),
                                            f9omstw::OmsRequestRunStepSP{new UomsTwsIniRiskCheck(testCore,
                                             f9omstw::OmsRequestRunStepSP{new UomsTwsExgSender{testCore}})}),
      new OmsRequestTwsChgFactory("TwsChg", f9omstw::OmsRequestRunStepSP{new UomsTwsExgSender{testCore}}),
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
      TestCase(testCore, reqPolicySP,
               "TwsNew|Market=T|SessionId=N|SesName=UT|Src=A|UsrDef=UD123|ClOrdId=C12|OrdNo=A|"
               "IvacNo=10|SubacNo=sa01|BrkId=8610|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");
   }
   stopWatch.PrintResult("ReqNew ", testTimes);
   //---------------------------------------------
   stopWatch.ResetTimer();
   for (unsigned L = 0; L < testTimes; ++L) {
      // 使用 IniSNO 改單, 由於剛剛的 TwsNew 的測試, RxHistory 有一半是 Request, 另一半是 OrderRaw, 
      // 所以這裡有一半的 IniSNO 會遇到 OrderRaw, 因此會造成 Abandon(OmsErrCode_OrderNotFound);
      // 所以這裡只有一半會成功.
      fon9::RevBufferFixedSize<1024> rbuf;
      fon9::RevPrint(rbuf, "TwsChg|IniSNO=", L, "|Market=T|SessionId=N|BrkId=8610|OrdNo="
                     "|Qty=-1000|SesName=UT|Src=B|UsrDef=UD234|ClOrdId=C34");
      TestCase(testCore, reqPolicySP, ToStrView(rbuf));
   }
   stopWatch.PrintResult("ReqChg ", testTimes);
   //---------------------------------------------
   f9omstw::OmsOrdNo ordno{"A0000"};
   stopWatch.ResetTimer();
   for (unsigned L = 0; L < testTimes; ++L) {
      fon9::RevBufferFixedSize<1024> rbuf;
      fon9::RevPrint(rbuf, "TwsChg|Market=T|SessionId=N|SesName=UT|Src=B|UsrDef=UD234|ClOrdId=C34"
                     "|BrkId=8610|OrdNo=", ordno, "|Qty=0");
      TestCase(testCore, reqPolicySP, ToStrView(rbuf));
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
