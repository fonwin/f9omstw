// \file f9omstw/OmsOrdNoMap_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9omstw/OmsCore.hpp"
#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsIvSymb.hpp"
#include "fon9/ThreadId.hpp"
#include "fon9/TestTools.hpp"
//--------------------------------------------------------------------------//
void VerifyAllocError(f9omstw::OmsOrderRaw& ord, bool allocResult, OmsErrCode errc) {
   std::cout << "|TeamGroupId=" << ord.Order_->Initiator_->Policy()->OrdTeamGroupId()
             << "|expected.ErrCode=" << errc;
   if (allocResult == true) {
      std::cout << "|err=expect AllocOrdNo() fail, but it success."
         "|expected.ErrCode=" << errc
         << "\r[ERROR]" << std::endl;
      abort();
   }
   if (ord.ErrCode_ == errc
       && ord.RequestSt_ == f9fmkt_TradingRequestSt_OrdNoRejected
       && ord.OrderSt_ == f9fmkt_OrderSt_NewOrdNoRejected) {
      ord.ErrCode_ = OmsErrCode_NoError;
      ord.RequestSt_ = f9fmkt_TradingRequestSt_Initialing;
      ord.OrderSt_ = f9fmkt_OrderSt_Initialing;
      std::cout << "\r[OK   ]" << std::endl;
      return;
   }
   std::cout
      << "|ErrCode=" << ord.ErrCode_
      << std::hex
      << "|reqSt=" << ord.RequestSt_
      << "|ordSt=" << ord.OrderSt_
      << "\r[ERROR]" << std::endl;
   abort();
}
void VerifyAllocOK(f9omstw::OmsOrderRaw& ord, bool allocResult, f9omstw::OmsOrdNo expected) {
   std::cout << "|TeamGroupId=" << ord.Order_->Initiator_->Policy()->OrdTeamGroupId();
   if (allocResult == false) {
      std::cout << "|err=expect AllocOrdNo() OK, but it fail."
         "|ErrCode=" << ord.ErrCode_
         << std::hex
         << "|reqSt=" << ord.RequestSt_
         << "|ordSt=" << ord.OrderSt_
         << "\r[ERROR]" << std::endl;
      abort();
   }
   std::cout << "|ordNo=" << std::string{ord.OrdNo_.begin(), ord.OrdNo_.end()};
   if (ord.OrdNo_ != expected) {
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

   struct RequestNew : public f9omstw::OmsTwsRequestIni {
      fon9_NON_COPY_NON_MOVE(RequestNew);
      using base = f9omstw::OmsTwsRequestIni;
      using base::base;
      void NoReadyLineReject(fon9::StrView) override {
      }
   };
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
   struct TestCore : public f9omstw::OmsCore {
      fon9_NON_COPY_NON_MOVE(TestCore);
      using base = f9omstw::OmsCore;
      TestCore() : base(new f9omstw::OmsCoreMgr{"ut"}, "seed/path", "OmsOrdNoMap_UT") {
         this->ThreadId_ = fon9::GetThisThreadId().ThreadId_;
      }
      ~TestCore() {
      }
      f9omstw::OmsResource& GetResource() {
         return *static_cast<f9omstw::OmsResource*>(this);
      }
      void RunCoreTask(f9omstw::OmsCoreTask&&) override {}
      bool MoveToCoreImpl(f9omstw::OmsRequestRunner&&) override { return false; }
   };
   TestCore                         testCore;
   fon9::intrusive_ptr<RequestNew>  newReq{new RequestNew{reqFactory}};
   f9omstw::OmsRequestRunnerInCore  runner{testCore.GetResource(),
      *ordFactory.MakeOrder(*newReq, nullptr),
      256u};
   testCore.GetResource().PutRequestId(*newReq);

   f9omstw::OmsRequestPolicy* reqPolicy{new f9omstw::OmsRequestPolicy{}};
   newReq->SetPolicy(reqPolicy);

   f9omstw::OmsOrdNoMap    ordNoMap;
   const f9omstw::OmsOrdNo ordNoNil{""};
 
   // -----------------------------
   std::cout << "[TEST ] OrdTeamGroupId=0, No groups.";
   VerifyAllocError(runner.OrderRaw_, ordNoMap.AllocOrdNo(runner, ordNoNil), OmsErrCode_OrdTeamGroupId);
   // -----------------------------
   reqPolicy->SetOrdTeamGroupCfg(testCore.GetResource().OrdTeamGroupMgr_.SetTeamGroup("admin", "*,adm,A"));
   std::cout << "[TEST ] SetTeamGroup('*,adm,A').    ";
   VerifyAllocOK(runner.OrderRaw_, ordNoMap.AllocOrdNo(runner, ordNoNil), "adm00");
   std::cout << "[TEST ]  AllocOrdNo() again.        ";
   VerifyAllocOK(runner.OrderRaw_, ordNoMap.AllocOrdNo(runner, ordNoNil), "adm01");
   std::cout << "[TEST ]  reqOrdNo='X'               ";
   VerifyAllocOK(runner.OrderRaw_, ordNoMap.AllocOrdNo(runner, "X"), "X0000");
   std::cout << "[TEST ]  reqOrdNo='X0000': OrdExists";
   VerifyAllocError(runner.OrderRaw_, ordNoMap.AllocOrdNo(runner, "X0000"), OmsErrCode_OrderAlreadyExists);
   std::cout << "[TEST ]  reqOrdNo='Xzzzz'           ";
   VerifyAllocOK(runner.OrderRaw_, ordNoMap.AllocOrdNo(runner, "Xzzzz"), "Xzzzz");
   std::cout << "[TEST ]  reqOrdNo='X':      Overflow";
   VerifyAllocError(runner.OrderRaw_, ordNoMap.AllocOrdNo(runner, "X"), OmsErrCode_OrdNoOverflow);
   // -----------------------------
   reqPolicy->SetOrdTeamGroupCfg(testCore.GetResource().OrdTeamGroupMgr_.SetTeamGroup("usr.TeamA", "a00,b00"));
   std::cout << "[TEST ] SetTeamGroup('a00,b00').    ";
   VerifyAllocOK(runner.OrderRaw_, ordNoMap.AllocOrdNo(runner, ordNoNil), "a0000");
   std::cout << "[TEST ]  AllocOrdNo(OrdTeamGroupId).";
   VerifyAllocOK(runner.OrderRaw_, ordNoMap.AllocOrdNo(runner, runner.OrderRaw_.Order_->Initiator_->Policy()->OrdTeamGroupId()), "a0001");
   std::cout << "[TEST ]  reqOrdNo='X':     MustEmpty";
   VerifyAllocError(runner.OrderRaw_, ordNoMap.AllocOrdNo(runner, "X"), OmsErrCode_OrdNoMustEmpty);

   // 把 a00, b00 用完, 測試是否會 OmsErrCode_OrdTeamUsedUp
   std::cout << "[TEST ]  Alloc: a00*,b00*           ";
   for (unsigned L = 2; L < fon9::kSeq2AlphaSize * fon9::kSeq2AlphaSize * 2; ++L)
      ordNoMap.AllocOrdNo(runner, runner.OrderRaw_.Order_->Initiator_->Policy()->OrdTeamGroupId());
   std::cout << "|LastOrdNo=" << std::string{runner.OrderRaw_.OrdNo_.begin(), runner.OrderRaw_.OrdNo_.end()};
   if (runner.OrderRaw_.OrdNo_ != f9omstw::OmsOrdNo{"b00zz"}) {
      std::cout << "|expected=b00zz" "\r[ERROR]" << std::endl;
      abort();
   }
   std::cout << "\r[OK   ]" << std::endl;

   std::cout << "[TEST ]  AllocOrdNo() again:  UsedUp";
   VerifyAllocError(runner.OrderRaw_, ordNoMap.AllocOrdNo(runner, runner.OrderRaw_.Order_->Initiator_->Policy()->OrdTeamGroupId()), OmsErrCode_OrdTeamUsedUp);
   std::cout << "[TEST ]  SetTeamGroup('..,c01')     ";
   testCore.GetResource().OrdTeamGroupMgr_.SetTeamGroup("usr.TeamA", "a00,b00,c01");
   VerifyAllocOK(runner.OrderRaw_, ordNoMap.AllocOrdNo(runner, runner.OrderRaw_.Order_->Initiator_->Policy()->OrdTeamGroupId()), "c0100");
   // -----------------------------
   reqPolicy->SetOrdTeamGroupCfg(nullptr);
   std::cout << "[TEST ] OrdTeamGroupId=0, 2 groups. ";
   VerifyAllocError(runner.OrderRaw_, ordNoMap.AllocOrdNo(runner, ordNoNil), OmsErrCode_OrdTeamGroupId);
   std::cout << "[TEST ] OrdTeamGroupId=3, 2 groups. ";
   f9omstw::OmsOrdTeamGroupCfg badcfg;
   badcfg.TeamGroupId_ = 3;
   reqPolicy->SetOrdTeamGroupCfg(&badcfg);
   VerifyAllocError(runner.OrderRaw_, ordNoMap.AllocOrdNo(runner, ordNoNil), OmsErrCode_OrdTeamGroupId);
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
      << "|LastOrdNo=" << std::string{runner.OrderRaw_.OrdNo_.begin(), runner.OrderRaw_.OrdNo_.end()};

   f9omstw::OmsOrdNo ordno{"D0000"};
   unsigned count = kTimes - (fon9::kSeq2AlphaSize * fon9::kSeq2AlphaSize * 2) - 1; // 扣除: B00??, C00??, D0000
   for (unsigned L = 4; L > 0; --L) {
      ordno.Chars_[L] = fon9::Seq2Alpha(static_cast<uint8_t>(count % fon9::kSeq2AlphaSize));
      count /= fon9::kSeq2AlphaSize;
   }
   if (ordno != runner.OrderRaw_.OrdNo_) {
      std::cout << "\n[ERROR] expected.LastOrdNo=" << std::string{ordno.begin(), ordno.end()} << std::endl;
      abort();
   }
   std::cout << std::endl;
}
