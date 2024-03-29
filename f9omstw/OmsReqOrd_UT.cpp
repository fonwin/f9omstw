﻿// \file f9omstw/OmsReqOrd_UT.cpp
//
// 測試 OmsRequest/OmsOrder 的設計.
//
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9utw/UnitTestCore.hpp"
//--------------------------------------------------------------------------//
void TestCase(f9omstw::OmsCore& core, f9omstw::OmsRequestPolicySP policy, fon9::StrView reqstr) {
   auto req = MakeOmsRequestRunner(core.Owner_->RequestFactoryPark(), reqstr);
   static_cast<f9omstw::OmsRequestTrade*>(req.Request_.get())->SetPolicy(std::move(policy));
   core.MoveToCore(std::move(req));
}
//--------------------------------------------------------------------------//
int main(int argc, const char* argv[]) {
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif
   fon9::AutoPrintTestInfo utinfo{"OmsRequest/OmsOrder"};
   fon9::StopWatch         stopWatch;

   TestCore testCore(argc, argv);
   auto&    coreResource = testCore.GetResource();
   testCore.Owner_->SetRequestFactoryPark(new f9omstw::OmsRequestFactoryPark(
      new OmsTwsRequestIniFactory("TwsNew", testCore.Owner_->OrderFactoryPark().GetFactory("TwsOrd"),
                                            f9omstw::OmsRequestRunStepSP{new UomsTwsIniRiskCheck(
                                             f9omstw::OmsRequestRunStepSP{new UomsTwsExgSender})}),
      new OmsTwsRequestChgFactory("TwsChg", f9omstw::OmsRequestRunStepSP{new UomsTwsExgSender})
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
   reqPolicy->SetOrdTeamGroupCfg(coreResource.OrdTeamGroupMgr_.SetTeamGroup("admin", "*"));
   reqPolicy->AddIvConfig(nullptr, nullptr, f9omstw::OmsIvConfig{f9omstw::OmsIvRight::AllowAll});
   //---------------------------------------------
   stopWatch.ResetTimer();
   for (unsigned L = 0; L < testCore.TestCount_; ++L) {
      TestCase(testCore, reqPolicySP,
               "TwsNew|Market=T|SessionId=N|SesName=UT|Src=A|UsrDef=UD123|ClOrdId=C12|OrdNo=A|"
               "IvacNo=10|SubacNo=sa01|BrkId=8610|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");
   }
   stopWatch.PrintResult("ReqNew ", testCore.TestCount_);
   //---------------------------------------------
   stopWatch.ResetTimer();
   for (unsigned L = 0; L < testCore.TestCount_; ++L) {
      // 使用 IniSNO 改單, 由於剛剛的 TwsNew 的測試, RxHistory 有一半是 Request, 另一半是 OrderRaw, 
      // 所以這裡有一半的 IniSNO 會遇到 OrderRaw, 因此會造成 Abandon(OmsErrCode_OrderNotFound);
      // 所以這裡只有一半會成功.
      fon9::RevBufferFixedSize<1024> rbuf;
      fon9::RevPrint(rbuf, "TwsChg|IniSNO=", L, "|Market=T|SessionId=N|BrkId=8610|OrdNo="
                     "|Qty=-1000|SesName=UT|Src=B|UsrDef=UD234|ClOrdId=C34");
      TestCase(testCore, reqPolicySP, ToStrView(rbuf));
   }
   stopWatch.PrintResult("ReqChg ", testCore.TestCount_);
   //---------------------------------------------
   f9omstw::OmsOrdNo ordno{"A0000"};
   stopWatch.ResetTimer();
   for (unsigned L = 0; L < testCore.TestCount_; ++L) {
      fon9::RevBufferFixedSize<1024> rbuf;
      fon9::RevPrint(rbuf, "TwsChg|Market=T|SessionId=N|SesName=UT|Src=B|UsrDef=UD234|ClOrdId=C34"
                     "|BrkId=8610|OrdNo=", ordno, "|Qty=0");
      TestCase(testCore, reqPolicySP, ToStrView(rbuf));
      f9omstw_IncStrAlpha(ordno.begin(), ordno.end());
   }
   stopWatch.PrintResult("ReqDel ", testCore.TestCount_);
}
