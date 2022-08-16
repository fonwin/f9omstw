// \file f9omstw/OmsOrdNoMap_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9utw/UnitTestCore.hpp"
#include "fon9/ThreadId.hpp"
#include "fon9/TestTools.hpp"
//--------------------------------------------------------------------------//
void VerifyAllocError(f9omstw::OmsOrderRaw& ordraw, bool allocResult, OmsErrCode errc) {
   std::cout << "|TeamGroupId=" << ordraw.Order().Initiator()->Policy()->OrdTeamGroupId()
             << "|expected.ErrCode=" << errc;
   if (allocResult == true) {
      std::cout << "|err=expect AllocOrdNo() fail, but it success."
         "|expected.ErrCode=" << errc
         << "\r[ERROR]" << std::endl;
      abort();
   }
   if (ordraw.ErrCode_ == errc
       && ordraw.RequestSt_ == f9fmkt_TradingRequestSt_OrdNoRejected
       && ordraw.UpdateOrderSt_ == f9fmkt_OrderSt_NewOrdNoRejected) {
      ordraw.ErrCode_ = OmsErrCode_NoError;
      ordraw.RequestSt_ = f9fmkt_TradingRequestSt_Initialing;
      ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_NewStarting;
      std::cout << "\r[OK   ]" << std::endl;
      return;
   }
   std::cout
      << "|ErrCode=" << ordraw.ErrCode_
      << std::hex
      << "|reqSt=" << ordraw.RequestSt_
      << "|ordSt=" << ordraw.UpdateOrderSt_
      << "\r[ERROR]" << std::endl;
   abort();
}
void VerifyAllocOK(f9omstw::OmsOrderRaw& ordraw, bool allocResult, f9omstw::OmsOrdNo expected) {
   std::cout << "|TeamGroupId=" << ordraw.Order().Initiator()->Policy()->OrdTeamGroupId();
   if (allocResult == false) {
      std::cout << "|err=expect AllocOrdNo() OK, but it fail."
         "|ErrCode=" << ordraw.ErrCode_
         << std::hex
         << "|reqSt=" << ordraw.RequestSt_
         << "|ordSt=" << ordraw.UpdateOrderSt_
         << "\r[ERROR]" << std::endl;
      abort();
   }
   std::cout << "|ordNo=" << std::string{ordraw.OrdNo_.begin(), ordraw.OrdNo_.end()};
   if (ordraw.OrdNo_ != expected) {
      std::cout << "|expected=" << std::string{expected.begin(), expected.end()}
         << "\r[ERROR]" << std::endl;
      abort();
   }
   std::cout << "\r[OK   ]" << std::endl;
}
//--------------------------------------------------------------------------//
int main(int argc, char* argv[]) {
   (void)argc; (void)argv;
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif
   fon9::AutoPrintTestInfo utinfo{"OmsOrdNoMap"};

   struct OrderFactory : public f9omstw::OmsOrderFactory {
      fon9_NON_COPY_NON_MOVE(OrderFactory);
      using base = f9omstw::OmsOrderFactory;

      f9omstw::OmsOrderRaw* MakeOrderRawImpl() override {
         return new f9omstw::OmsTwsOrderRaw;
      }
      f9omstw::OmsOrder* MakeOrderImpl() override {
         return new f9omstw::OmsOrder;
      }
      OrderFactory() : base(fon9::Named{"OrdFac"}, fon9::seed::Fields{}) {
      }
   };
   OrderFactory   ordFactory;

   using RequestNew = f9omstw::OmsTwsRequestIni;
   struct RequestFactory : public f9omstw::OmsRequestFactory {
      fon9_NON_COPY_NON_MOVE(RequestFactory);
      using base = f9omstw::OmsRequestFactory;

      f9omstw::OmsRequestSP MakeRequestImpl() override {
         return new RequestNew{*this};
      }
      RequestFactory() : base(fon9::Named{"ReqFac"}, fon9::seed::Fields{}) {
      }
   };
   RequestFactory   reqFactory;

   // 測試用, 不啟動 OmsThread: 把 main thread 當成 OmsThread.
   struct TestOrdNoCore : public f9omstw::OmsCore {
      fon9_NON_COPY_NON_MOVE(TestOrdNoCore);
      using base = f9omstw::OmsCore;
      TestOrdNoCore() : base(fon9::LocalNow(), new f9omstw::OmsCoreMgr{nullptr}, "seed/path", "OmsOrdNoMap_UT") {
         this->ThreadId_ = fon9::GetThisThreadId().ThreadId_;
         DummyBrk::MakeBrkTree(*this);
      }
      ~TestOrdNoCore() {
      }
      f9omstw::OmsResource& GetResource() {
         return *static_cast<f9omstw::OmsResource*>(this);
      }
      bool RunCoreTask(f9omstw::OmsCoreTask&&) override { return false; }
      bool MoveToCoreImpl(f9omstw::OmsRequestRunner&&) override { return false; }
   };
   TestOrdNoCore                    testCore;
   fon9::intrusive_ptr<RequestNew>  newReq{new RequestNew{reqFactory}};
   f9omstw::OmsRequestRunnerInCore  runner{testCore.GetResource(),
      *ordFactory.MakeOrder(*newReq, nullptr),
      256u};
   testCore.GetResource().FetchRequestId(*newReq);

   f9omstw::OmsRequestPolicy* reqPolicy{new f9omstw::OmsRequestPolicy{}};
   newReq->SetPolicy(reqPolicy);
   auto& ordraw = runner.OrderRaw();
   ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_NewStarting;

   f9omstw::OmsOrdNoMap    ordNoMap;
   const f9omstw::OmsOrdNo ordNoNil{""};
 
   // -----------------------------
   std::cout << "[TEST ] OrdTeamGroupId=0, No groups.";
   VerifyAllocError(ordraw, ordNoMap.AllocOrdNo(runner, ordNoNil), OmsErrCode_OrdTeamGroupId);
   // -----------------------------
   // 可自訂委託書號, 但櫃號只能用 'adm' 或 'X'.
   reqPolicy->SetOrdTeamGroupCfg(testCore.GetResource().OrdTeamGroupMgr_.SetTeamGroup("admin", "*,adm,X"));
   std::cout << "[TEST ] SetTeamGroup('*,adm,X').    ";
   VerifyAllocOK(ordraw, ordNoMap.AllocOrdNo(runner, ordNoNil), "adm00");
   std::cout << "[TEST ]  AllocOrdNo() again.        ";
   VerifyAllocOK(ordraw, ordNoMap.AllocOrdNo(runner, ordNoNil), "adm01");
   std::cout << "[TEST ]  reqOrdNo='X'               ";
   VerifyAllocOK(ordraw, ordNoMap.AllocOrdNo(runner, "X"), "X0000");
   std::cout << "[TEST ]  reqOrdNo='X0000': OrdExists";
   VerifyAllocError(ordraw, ordNoMap.AllocOrdNo(runner, "X0000"), OmsErrCode_OrderAlreadyExists);
   std::cout << "[TEST ]  reqOrdNo='Xzzzz'           ";
   VerifyAllocOK(ordraw, ordNoMap.AllocOrdNo(runner, "Xzzzz"), "Xzzzz");
   std::cout << "[TEST ]  reqOrdNo='X':      Overflow";
   VerifyAllocError(ordraw, ordNoMap.AllocOrdNo(runner, "X"), OmsErrCode_OrdNoOverflow);
   // -----------------------------
   reqPolicy->SetOrdTeamGroupCfg(testCore.GetResource().OrdTeamGroupMgr_.SetTeamGroup("usr.TeamA", "a00,b00"));
   std::cout << "[TEST ] SetTeamGroup('a00,b00').    ";
   VerifyAllocOK(ordraw, ordNoMap.AllocOrdNo(runner, ordNoNil), "a0000");
   std::cout << "[TEST ]  AllocOrdNo(OrdTeamGroupId).";
   VerifyAllocOK(ordraw, ordNoMap.AllocOrdNo(runner, ordraw.Order().Initiator()->Policy()->OrdTeamGroupId()), "a0001");
   std::cout << "[TEST ]  reqOrdNo='X':     MustEmpty";
   VerifyAllocError(ordraw, ordNoMap.AllocOrdNo(runner, "X"), OmsErrCode_OrdNoMustEmpty);

   // 把 a00, b00 用完, 測試是否會 OmsErrCode_OrdTeamUsedUp
   std::cout << "[TEST ]  Alloc: a00*,b00*           ";
   for (unsigned L = 2; L < fon9::kSeq2AlphaSize * fon9::kSeq2AlphaSize * 2; ++L)
      ordNoMap.AllocOrdNo(runner, ordraw.Order().Initiator()->Policy()->OrdTeamGroupId());
   std::cout << "|LastOrdNo=" << std::string{ordraw.OrdNo_.begin(), ordraw.OrdNo_.end()};
   if (ordraw.OrdNo_ != f9omstw::OmsOrdNo{"b00zz"}) {
      std::cout << "|expected=b00zz" "\r[ERROR]" << std::endl;
      abort();
   }
   std::cout << "\r[OK   ]" << std::endl;

   std::cout << "[TEST ]  AllocOrdNo() again:  UsedUp";
   VerifyAllocError(ordraw, ordNoMap.AllocOrdNo(runner, ordraw.Order().Initiator()->Policy()->OrdTeamGroupId()), OmsErrCode_OrdTeamUsedUp);
   std::cout << "[TEST ]  SetTeamGroup('..,c01')     ";
   testCore.GetResource().OrdTeamGroupMgr_.SetTeamGroup("usr.TeamA", "a00,b00,c01");
   VerifyAllocOK(ordraw, ordNoMap.AllocOrdNo(runner, ordraw.Order().Initiator()->Policy()->OrdTeamGroupId()), "c0100");
   // -----------------------------
   reqPolicy->SetOrdTeamGroupCfg(nullptr);
   std::cout << "[TEST ] OrdTeamGroupId=0, 2 groups. ";
   VerifyAllocError(ordraw, ordNoMap.AllocOrdNo(runner, ordNoNil), OmsErrCode_OrdTeamGroupId);
   std::cout << "[TEST ] OrdTeamGroupId=3, 2 groups. ";
   f9omstw::OmsOrdTeamGroupCfg badcfg;
   badcfg.TeamGroupId_ = 3;
   reqPolicy->SetOrdTeamGroupCfg(&badcfg);
   VerifyAllocError(ordraw, ordNoMap.AllocOrdNo(runner, ordNoNil), OmsErrCode_OrdTeamGroupId);
   // -----------------------------
   reqPolicy->SetOrdTeamGroupCfg(testCore.GetResource().OrdTeamGroupMgr_.SetTeamGroup("usr.TeamB", "B00,C00,D"));
   #ifdef _DEBUG
      const unsigned  kTimes = 1000 * 100;
   #else
      const unsigned  kTimes = 1000 * 1000;
   #endif
   fon9::StopWatch stopWatch;
   for (unsigned L = 0; L < kTimes; ++L)
      ordNoMap.AllocOrdNo(runner, ordNoNil);
   stopWatch.PrintResultNoEOL("AllocOrdNo ", kTimes)
      << "|LastOrdNo=" << std::string{ordraw.OrdNo_.begin(), ordraw.OrdNo_.end()};

   f9omstw::OmsOrdNo ordno{"D0000"};
   unsigned count = kTimes - (fon9::kSeq2AlphaSize * fon9::kSeq2AlphaSize * 2) - 1; // 扣除: B00??, C00??, D0000
   for (unsigned L = 4; L > 0; --L) {
      ordno.Chars_[L] = fon9::Seq2Alpha(static_cast<uint8_t>(count % fon9::kSeq2AlphaSize));
      count /= fon9::kSeq2AlphaSize;
   }
   if (ordno != ordraw.OrdNo_) {
      std::cout << "\n[ERROR] expected.LastOrdNo=" << std::string{ordno.begin(), ordno.end()} << std::endl;
      abort();
   }
   std::cout << std::endl;
}
