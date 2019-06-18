// \file f9omstw/apps/f9omstw/UnitTestCore.cpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_apps_UnitTestCore_hpp__
#define __f9omstw_apps_UnitTestCore_hpp__
#define _CRT_SECURE_NO_WARNINGS
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsRequestPolicy.hpp"
#include "f9omstw/OmsOrderTws.hpp"
#include "utws/UtwsBrk.hpp"
#include "utws/UtwsSymb.hpp"

#include "fon9/CmdArgs.hpp"
#include "fon9/Log.hpp"
#include "fon9/ThreadId.hpp"
#include "fon9/TestTools.hpp"
//--------------------------------------------------------------------------//
#include "fon9/ObjSupplier.hpp"
#ifdef _DEBUG
const unsigned    kPoolObjCount = 1000;
#else
const unsigned    kPoolObjCount = 1000 * 100;
#endif

enum class AllocFrom {
   Supplier,
   Memory,
};
static AllocFrom  gAllocFrom{AllocFrom::Supplier};
//--------------------------------------------------------------------------//
class OmsOrderTwsFactory : public f9omstw::OmsOrderFactory {
   fon9_NON_COPY_NON_MOVE(OmsOrderTwsFactory);
   using base = f9omstw::OmsOrderFactory;

   using OmsOrderTwsRaw = f9omstw::OmsOrderTwsRaw;
   using RawSupplier = fon9::ObjSupplier<OmsOrderTwsRaw, kPoolObjCount>;
   RawSupplier::ThisSP RawSupplier_{RawSupplier::Make()};
   OmsOrderTwsRaw* MakeOrderRawImpl() override {
      if (gAllocFrom == AllocFrom::Supplier)
         return this->RawSupplier_->Alloc();
      return new OmsOrderTwsRaw{};
   }

   using OmsOrderTws = f9omstw::OmsOrder;
   using OrderSupplier = fon9::ObjSupplier<OmsOrderTws, kPoolObjCount>;
   OrderSupplier::ThisSP OrderSupplier_{OrderSupplier::Make()};
   OmsOrderTws* MakeOrderImpl() override {
      if (gAllocFrom == AllocFrom::Supplier)
         return this->OrderSupplier_->Alloc();
      return new OmsOrderTws{};
   }
public:
   OmsOrderTwsFactory()
      : base(fon9::Named{"TwsOrd"}, f9omstw::MakeFieldsT<OmsOrderTwsRaw>()) {
   }

   ~OmsOrderTwsFactory() {
   }
};
//--------------------------------------------------------------------------//
template <class OmsRequestBaseT>
class OmsRequestFactoryT : public f9omstw::OmsRequestFactory {
   fon9_NON_COPY_NON_MOVE(OmsRequestFactoryT);
   using base = f9omstw::OmsRequestFactory;

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
   f9omstw::OmsRequestSP MakeRequestImpl() override {
      if (gAllocFrom == AllocFrom::Supplier)
         return this->RequestTape_->Alloc();
      return new OmsRequestT{};
   }
public:
   OmsRequestFactoryT(std::string name, f9omstw::OmsRequestRunStepSP runStepList)
      : base(std::move(runStepList),
             fon9::Named(std::move(name)),
             f9omstw::MakeFieldsT<OmsRequestT>()) {
   }
   OmsRequestFactoryT(std::string name, f9omstw::OmsOrderFactorySP ordFactory, f9omstw::OmsRequestRunStepSP runStepList)
      : base(std::move(ordFactory), std::move(runStepList),
             fon9::Named(std::move(name)),
             f9omstw::MakeFieldsT<OmsRequestT>()) {
   }

   ~OmsRequestFactoryT() {
   }
};
using OmsRequestTwsIniFactory = OmsRequestFactoryT<f9omstw::OmsRequestTwsIni>;
using OmsRequestTwsChgFactory = OmsRequestFactoryT<f9omstw::OmsRequestTwsChg>;
using OmsRequestTwsFilledFactory = OmsRequestFactoryT<f9omstw::OmsRequestTwsFilled>;
//--------------------------------------------------------------------------//
f9omstw::OmsRequestRunner MakeOmsRequestRunner(const f9omstw::OmsRequestFactoryPark& facPark, fon9::StrView reqstr) {
   f9omstw::OmsRequestRunner retval{reqstr};
   fon9::StrView tag = fon9::StrFetchNoTrim(reqstr, '|');
   if (auto* fac = facPark.GetFactory(tag)) {
      auto req = fac->MakeRequest();
      if (dynamic_cast<f9omstw::OmsRequestTrade*>(req.get()) == nullptr) {
         fon9_LOG_ERROR("MakeOmsRequestRunner|err=It's not a trading request factory|reqfac=", tag);
         return retval;
      }
      fon9::seed::SimpleRawWr wr{*req};
      fon9::StrView value;
      while(fon9::StrFetchTagValue(reqstr, tag, value)) {
         auto* fld = fac->Fields_.Get(tag);
         if (fld == nullptr) {
            fon9_LOG_ERROR("MakeOmsRequestRunner|err=Unknown field|reqfac=", fac->Name_, "|fld=", tag);
            return retval;
         }
         fld->StrToCell(wr, value);
      }
      retval.Request_ = std::move(fon9::static_pointer_cast<f9omstw::OmsRequestTrade>(req));
      return retval;
   }
   fon9_LOG_ERROR("MakeOmsRequestRunner|err=Unknown request factory|reqfac=", tag);
   return retval;
}
void PrintOmsRequest(f9omstw::OmsRequestBase& req) {
   fon9::RevBufferList     rbuf{128};
   fon9::seed::SimpleRawRd rd{req};
   for (size_t fidx = req.Creator_->Fields_.size(); fidx > 0;) {
      auto fld = req.Creator_->Fields_.Get(--fidx);
      fld->CellRevPrint(rd, nullptr, rbuf);
      fon9::RevPrint(rbuf, *fon9_kCSTR_CELLSPL, fld->Name_, '=');
   }
   fon9::RevPrint(rbuf, req.Creator_->Name_);
   puts(fon9::BufferTo<std::string>(rbuf.MoveOut()).c_str());
}
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
   UomsTwsExgSender() = default;
   /// 「線路群組」的「櫃號組別Id」, 需透過 OmsResource 取得.
   f9omstw::OmsOrdTeamGroupId TgId_ = 0;
   char                       padding_____[4];

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
// 測試用, 不啟動 OmsThread: 把 main thread 當成 OmsThread.
struct TestCore : public f9omstw::OmsCore {
   fon9_NON_COPY_NON_MOVE(TestCore);
   using base = f9omstw::OmsCore;
   unsigned TestCount_;
   bool     IsWaitQuit_{false};
   char     padding___[3];

   static fon9::intrusive_ptr<TestCore> MakeCoreMgr(int argc, char* argv[]) {
      fon9::intrusive_ptr<TestCore> core{new TestCore{argc, argv}};
      core->Owner_->Add(&core->GetResource());
      return core;
   }

   TestCore(int argc, char* argv[], std::string name = "ut", f9omstw::OmsCoreMgrSP owner = nullptr)
      : base(owner ? owner : new f9omstw::OmsCoreMgr{"ut"}, "seed/path", name) {
      this->ThreadId_ = fon9::GetThisThreadId().ThreadId_;

      if (owner.get() == nullptr) {
         this->Owner_->SetOrderFactoryPark(new f9omstw::OmsOrderFactoryPark(
            new OmsOrderTwsFactory
         ));
         gAllocFrom = static_cast<AllocFrom>(fon9::StrTo(fon9::GetCmdArg(argc, argv, "f", "allocfrom"), 0u));
         std::cout << "AllocFrom = " << (gAllocFrom == AllocFrom::Supplier ? "Supplier" : "Memory") << std::endl;
      }

      this->TestCount_ = fon9::StrTo(fon9::GetCmdArg(argc, argv, "c", "count"), 0u);
      if (this->TestCount_ <= 0)
         this->TestCount_ = kPoolObjCount * 2;

      const auto isWait = fon9::GetCmdArg(argc, argv, "w", "wait");
      this->IsWaitQuit_ = (isWait.begin() && (isWait.empty() || fon9::toupper(static_cast<unsigned char>(*isWait.begin())) == 'Y'));

      using namespace f9omstw;
      this->Symbs_.reset(new OmsSymbTree(*this, UtwsSymb::MakeLayout(OmsSymbTree::DefaultTreeFlag()), &UtwsSymb::SymbMaker));
      this->Brks_.reset(new OmsBrkTree(*this, UtwsBrk::MakeLayout(OmsBrkTree::DefaultTreeFlag()), &OmsBrkTree::TwsBrkIndex1));
      this->Brks_->Initialize(&UtwsBrk::BrkMaker, "8610", 5u, &IncStrAlpha);
      // 建立委託書號表的關聯.
      this->Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwSEC);
      this->Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwOTC);
   }

   ~TestCore() {
      std::cout << "LastSNO = " << this->Backend_.LastSNO() << std::endl;
      if (this->IsWaitQuit_) {
         // 要查看資源用量(時間、記憶體...), 可透過 `/usr/bin/time` 指令:
         //    /usr/bin/time --verbose ~/devel/output/f9omstw/release/f9omstw/OmsReqOrd_UT
         // 或在結束前先暫停, 在透過其他外部工具查看:
         //    ~/devel/output/f9omstw/release/f9omstw/OmsReqOrd_UT -w
         //    例如: $cat /proc/19420(pid)/status 查看 VmSize
         std::cout << "Press <Enter> to quit.";
         getchar();
      }
      this->Backend_.WaitForEndNow();
      this->Brks_->InThr_OnParentSeedClear();
   }

   void OpenReload(int argc, char* argv[], std::string fnDefault, uint32_t forceTDay = 0) {
      const auto  outfn = fon9::GetCmdArg(argc, argv, "o", "out");
      if (!outfn.empty())
         fnDefault = outfn.ToString();
      if (forceTDay != 0) {
         if (fnDefault.size() >= 4 && memcmp(&*(fnDefault.end() - 4), ".log", 4) == 0)
            fnDefault.resize(fnDefault.size() - 4);
         fnDefault += fon9::RevPrintTo<std::string>('.', forceTDay, ".log");
      }
      StartResult res = this->Start(fon9::UtcNow(), fnDefault, forceTDay);
      if (res.IsError())
         std::cout << "OmsCore.Reload error:" << fon9::RevPrintTo<std::string>(res) << std::endl;
   }

   f9omstw::OmsResource& GetResource() {
      return *static_cast<f9omstw::OmsResource*>(this);
   }

   void RunCoreTask(f9omstw::OmsCoreTask&& task) override {
      task(this->GetResource());
   }
   bool MoveToCoreImpl(f9omstw::OmsRequestRunner&& runner) override {
      // TestCore: 使用 單一 thread, 無 locker, 所以直接呼叫
      this->RunInCore(std::move(runner));
      return true;
   }
};
#endif//__f9omstw_apps_UnitTestCore_hpp__
