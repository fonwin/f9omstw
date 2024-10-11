// \file f9utw/UnitTestCore.hpp
// \author fonwinz@gmail.com
#ifndef __f9utw_UnitTestCore_hpp__
#define __f9utw_UnitTestCore_hpp__
#define _CRT_SECURE_NO_WARNINGS
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsEventSessionSt.hpp"
#include "f9omstw/OmsReportFactory.hpp"
#include "f9omstw/OmsRequestPolicy.hpp"
#include "f9omstw/OmsReportRunner.hpp"

#include "f9omstws/OmsTwsOrder.hpp"
#include "f9omstws/OmsTwsReport.hpp"
#include "f9omstws/OmsTwsFilled.hpp"

#include "f9omstwf/OmsTwfOrder1.hpp"
#include "f9omstwf/OmsTwfReport2.hpp"
#include "f9omstwf/OmsTwfFilled.hpp"
#include "f9omstwf/OmsTwfOrder7.hpp"
#include "f9omstwf/OmsTwfReport8.hpp"
#include "f9omstwf/OmsTwfOrder9.hpp"
#include "f9omstwf/OmsTwfReport9.hpp"

#include "f9utw/UtwsBrk.hpp"
#include "f9utw/UtwsSymb.hpp"

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
static bool       gIsLessInfo{false};
//--------------------------------------------------------------------------//
template <class OrderT, class OrderRawT>
class OmsOrderFactoryT : public f9omstw::OmsOrderFactory {
   fon9_NON_COPY_NON_MOVE(OmsOrderFactoryT);
   using base = f9omstw::OmsOrderFactory;

   using RawSupplier = fon9::ObjSupplier<OrderRawT, kPoolObjCount>;
   typename RawSupplier::ThisSP RawSupplier_{RawSupplier::Make()};
   OrderRawT* MakeOrderRawImpl() override {
      if (gAllocFrom == AllocFrom::Supplier)
         return this->RawSupplier_->Alloc();
      return new OrderRawT{};
   }

   using OrderSupplier = fon9::ObjSupplier<OrderT, kPoolObjCount>;
   typename OrderSupplier::ThisSP OrderSupplier_{OrderSupplier::Make()};
   OrderT* MakeOrderImpl() override {
      if (gAllocFrom == AllocFrom::Supplier)
         return this->OrderSupplier_->Alloc();
      return new OrderT{};
   }
public:
   OmsOrderFactoryT(std::string name)
      : base(fon9::Named{std::move(name)}, f9omstw::MakeFieldsT<OrderRawT>()) {
   }
   ~OmsOrderFactoryT() {
   }
};

using OmsTwsOrderFactory = OmsOrderFactoryT<f9omstw::OmsOrder, f9omstw::OmsTwsOrderRaw>;
using OmsTwfOrder1Factory = OmsOrderFactoryT<f9omstw::OmsTwfOrder1, f9omstw::OmsTwfOrderRaw1>;
using OmsTwfOrder7Factory = OmsOrderFactoryT<f9omstw::OmsTwfOrder7, f9omstw::OmsTwfOrderRaw7>;
using OmsTwfOrder9Factory = OmsOrderFactoryT<f9omstw::OmsTwfOrder9, f9omstw::OmsTwfOrderRaw9>;
//--------------------------------------------------------------------------//
template <class OmsRequestBaseT>
class OmsRequestFactoryT : public f9omstw::OmsRequestFactoryT<OmsRequestBaseT, kPoolObjCount> {
   fon9_NON_COPY_NON_MOVE(OmsRequestFactoryT);
   using base = f9omstw::OmsRequestFactoryT<OmsRequestBaseT, kPoolObjCount>;

   f9omstw::OmsRequestSP MakeRequestImpl() override {
      if (gAllocFrom == AllocFrom::Supplier)
         return this->RequestTape_->Alloc();
      return new OmsRequestBaseT{};
   }
public:
   using base::base;
};

using OmsTwsRequestChgFactory = OmsRequestFactoryT<f9omstw::OmsTwsRequestChg>;
using OmsTwsRequestIniFactory = OmsRequestFactoryT<f9omstw::OmsTwsRequestIni>;

using OmsTwfRequestChg1Factory = OmsRequestFactoryT<f9omstw::OmsTwfRequestChg1>;
using OmsTwfRequestIni1Factory = OmsRequestFactoryT<f9omstw::OmsTwfRequestIni1>;
using OmsTwfRequestIni7Factory = OmsRequestFactoryT<f9omstw::OmsTwfRequestIni7>;
using OmsTwfRequestChg9Factory = OmsRequestFactoryT<f9omstw::OmsTwfRequestChg9>;
using OmsTwfRequestIni9Factory = OmsRequestFactoryT<f9omstw::OmsTwfRequestIni9>;
//--------------------------------------------------------------------------//
using f9omstw::OmsTwsFilledFactory;
using f9omstw::OmsTwsReportFactory;

using f9omstw::OmsTwfReport2Factory;
using f9omstw::OmsTwfReport8Factory;
using f9omstw::OmsTwfReport9Factory;
using f9omstw::OmsTwfFilled1Factory;
using f9omstw::OmsTwfFilled2Factory;
//--------------------------------------------------------------------------//
bool ParseRequestFields(f9omstw::OmsRequestFactory& fac, f9omstw::OmsRequestBase& req, fon9::StrView reqstr) {
   fon9::seed::SimpleRawWr wr{req};
   fon9::StrView           tag, value;
   while (fon9::StrFetchTagValue(reqstr, tag, value)) {
      auto* fld = fac.Fields_.Get(tag);
      if (fld == nullptr) {
         fon9_LOG_ERROR("ParseRequestFields|err=Unknown field|reqfac=", fac.Name_, "|fld=", tag);
         return false;
      }
      fld->StrToCell(wr, value);
   }
   return true;
}

f9omstw::OmsRequestRunner MakeOmsRequestRunner(const f9omstw::OmsRequestFactoryPark& facPark, fon9::StrView reqstr) {
   f9omstw::OmsRequestRunner retval{reqstr};
   fon9::StrView tag = fon9::StrFetchNoTrim(reqstr, '|');
   if (auto* fac = facPark.GetFactory(tag)) {
      auto req = fac->MakeRequest(fon9::UtcNow());
      if (req.get() == nullptr)
         fon9_LOG_ERROR("MakeOmsRequestRunner|err=MakeRequest return nullptr|reqfac=", tag);
      else if (ParseRequestFields(*fac, *req, reqstr))
         retval.Request_ = std::move(req);
      return retval;
   }
   fon9_LOG_ERROR("MakeOmsRequestRunner|err=Unknown request factory|reqfac=", tag);
   return retval;
}
f9omstw::OmsRequestRunner MakeOmsReportRunner(const f9omstw::OmsRequestFactoryPark& facPark,
                                              fon9::StrView reqstr, f9fmkt_RxKind reqKind) {
   f9omstw::OmsRequestRunner retval{reqstr};
   fon9::StrView tag = fon9::StrFetchNoTrim(reqstr, '|');
   if (auto* fac = facPark.GetFactory(tag)) {
      auto req = fac->MakeReportIn(reqKind, fon9::UtcNow());
      if (req.get() == nullptr)
         fon9_LOG_ERROR("MakeOmsReportRunner|err=MakeReportIn() return nullptr|reqfac=", tag);
      else if (ParseRequestFields(*fac, *req, reqstr))
         retval.Request_ = std::move(req);
      return retval;
   }
   fon9_LOG_ERROR("MakeOmsReportRunner|err=Unknown request factory|reqfac=", tag);
   return retval;
}
void PrintOmsRequest(f9omstw::OmsRequestBase& req) {
   fon9::RevBufferList     rbuf{128};
   fon9::seed::SimpleRawRd rd{req};
   for (size_t fidx = req.Creator().Fields_.size(); fidx > 0;) {
      auto fld = req.Creator().Fields_.Get(--fidx);
      fld->CellRevPrint(rd, nullptr, rbuf);
      fon9::RevPrint(rbuf, *fon9_kCSTR_CELLSPL, fld->Name_, '=');
   }
   fon9::RevPrint(rbuf, req.Creator().Name_);
   puts(fon9::BufferTo<std::string>(rbuf.MoveOut()).c_str());
}
//--------------------------------------------------------------------------//
inline void AssignOrdRaw(f9omstw::OmsTwsOrderRaw& ordraw, const f9omstw::OmsTwsRequestIni& inireq) {
   if (ordraw.OType_ == f9omstw::OmsTwsOType{})
      ordraw.OType_ = inireq.OType_;
}
// ----------
inline void AssignOrdRaw(f9omstw::OmsTwfOrderRaw1& ordraw, const f9omstw::OmsTwfRequestIni1& inireq) {
   if (ordraw.PosEff_ == f9omstw::OmsTwfPosEff{})
      ordraw.PosEff_ = inireq.PosEff_;
   if (ordraw.LastTimeInForce_ == f9fmkt_TimeInForce{})
      ordraw.LastTimeInForce_ = inireq.TimeInForce_;
}
// ----------
template <class ReqIniT, class OrdRawT>
struct UomsReqIniRiskCheck : public f9omstw::OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(UomsReqIniRiskCheck);
   using base = f9omstw::OmsRequestRunStep;
   using base::base;

   void RunRequest(f9omstw::OmsRequestRunnerInCore&& runner) override {
      assert(dynamic_cast<const ReqIniT*>(runner.OrderRaw().Order().Initiator()) != nullptr);
      auto& ordraw = runner.OrderRawT<OrdRawT>();
      auto* inireq = static_cast<const ReqIniT*>(ordraw.Order().Initiator());
      AssignOrdRaw(ordraw, *inireq);
      // 風控成功, 設定委託剩餘數量及價格(提供給風控資料計算), 然後執行下一步驟.
      if (&ordraw.Request() == inireq) {
         ordraw.LastPri_ = inireq->Pri_;
         ordraw.LastPriType_ = inireq->PriType_;
         if (inireq->RxKind() == f9fmkt_RxKind_RequestNew) {
            ordraw.BeforeQty_ = 0;
            if (ordraw.LeavesQty_ == 0) // 全新的新單要求, 若 LeavesQty_ > 0: 則可能是經過某些處理後(例:條件成立後), 的實際下單數量;
               ordraw.LeavesQty_ = inireq->Qty_;
            ordraw.AfterQty_ = ordraw.LeavesQty_;
         }
      }
      this->ToNextStep(std::move(runner));
   }
};
using UomsTwsIniRiskCheck = UomsReqIniRiskCheck<f9omstw::OmsTwsRequestIni, f9omstw::OmsTwsOrderRaw>;
using UomsTwfIniRiskCheck = UomsReqIniRiskCheck<f9omstw::OmsTwfRequestIni1, f9omstw::OmsTwfOrderRaw1>;

// ----------
template <class ReqIniT, class OrdRawT>
struct UomsReqIni9RiskCheck : public f9omstw::OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(UomsReqIni9RiskCheck);
   using base = f9omstw::OmsRequestRunStep;
   using base::base;

   void RunRequest(f9omstw::OmsRequestRunnerInCore&& runner) override {
      assert(dynamic_cast<const ReqIniT*>(runner.OrderRaw().Order().Initiator()) != nullptr);
      auto& ordraw = runner.OrderRawT<OrdRawT>();
      auto* inireq = static_cast<const ReqIniT*>(ordraw.Order().Initiator());
      // 風控成功, 設定委託剩餘數量及價格(提供給風控資料計算), 然後執行下一步驟.
      if (&ordraw.Request() == inireq) {
         if (inireq->RxKind() == f9fmkt_RxKind_RequestNew) {
            ordraw.Bid_.AfterQty_ = ordraw.Bid_.LeavesQty_ = inireq->BidQty_;
            ordraw.Offer_.AfterQty_ = ordraw.Offer_.LeavesQty_ = inireq->OfferQty_;
         }
      }
      this->ToNextStep(std::move(runner));
   }
};
using UomsTwfIni9RiskCheck = UomsReqIni9RiskCheck<f9omstw::OmsTwfRequestIni9, f9omstw::OmsTwfOrderRaw9>;
// ----------
struct UomsTwsExgSender : public f9omstw::OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(UomsTwsExgSender);
   using base = f9omstw::OmsRequestRunStep;
   using base::base;
   UomsTwsExgSender() = default;
   /// 「線路群組」的「櫃號組別Id」, 需透過 OmsResource 取得.
   f9omstw::OmsOrdTeamGroupId TgId_ = 0;
   char                       padding_____[4];

   void TestRun(f9omstw::OmsRequestRunnerInCore& runner) {
      auto& ordraw = runner.OrderRaw();
      if (ordraw.Request().RxKind() == f9fmkt_RxKind_RequestNew)
         ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_NewSending;
      ordraw.RequestSt_ = f9fmkt_TradingRequestSt_Sending;
      ordraw.Message_.assign("Sending by 8610T1");
   }
   void RunRequest(f9omstw::OmsRequestRunnerInCore&& runner) override {
      // 排隊 or 送單.
      // 最遲在下單要求送出(交易所)前, 必須編製委託書號.
      if (!runner.AllocOrdNo_IniOrTgid(this->TgId_, *static_cast<const f9omstw::OmsRequestTrade*>(&runner.OrderRaw().Request())))
         return;
      this->TestRun(runner);
      fon9::RevPrint(runner.ExLogForUpd_, "<<Sending packet for: ",
                     runner.OrderRaw().Request().ReqUID_, ">>\n");
   }
   virtual bool RerunCheck(f9omstw::OmsOrderRaw& ordraw) {
      assert(dynamic_cast<f9omstw::OmsTwsOrderRaw*>(&ordraw) != nullptr);
      auto& twsord = *static_cast<f9omstw::OmsTwsOrderRaw*>(&ordraw);
      if (twsord.Request().RxKind() == f9fmkt_RxKind_RequestNew) {
         twsord.LeavesQty_ = twsord.BeforeQty_;
      }
      else if (twsord.LeavesQty_ <= 0) {
         // 刪改失敗, 想要重送, 但剩餘量已為0, 沒有重送的必要.
         return false;
      }
      return true;
   }
   void RerunRequest(f9omstw::OmsReportRunnerInCore&& runner) override {
      if (!this->RerunCheck(runner.OrderRaw()))
         return;
      this->TestRun(runner);
      if (runner.ExLogForUpd_.cfront() != nullptr)
         fon9::RevPrint(runner.ExLogForUpd_, ">" fon9_kCSTR_CELLSPL);
      fon9::RevPrint(runner.ExLogForUpd_, "<<Resending packet for: ",
                     runner.OrderRaw().Request().ReqUID_, ">>" fon9_kCSTR_ROWSPL);
   }
};
struct UomsTwfExgSender : public UomsTwsExgSender {
   fon9_NON_COPY_NON_MOVE(UomsTwfExgSender);
   using base = UomsTwsExgSender;
   using base::base;
   UomsTwfExgSender() = default;

   bool RerunCheck(f9omstw::OmsOrderRaw& ordraw) override {
      if (auto* ordraw1 = static_cast<f9omstw::OmsTwfOrderRaw1*>(&ordraw)) {
         if (ordraw1->Request().RxKind() == f9fmkt_RxKind_RequestNew) {
            ordraw1->LeavesQty_ = ordraw1->BeforeQty_;
         }
         else if (ordraw1->LeavesQty_ <= 0) {
            // 刪改失敗, 想要重送, 但剩餘量已為0, 沒有重送的必要.
            return false;
         }
      }
      else if (auto* ordraw9 = static_cast<f9omstw::OmsTwfOrderRaw9*>(&ordraw)) {
         if (ordraw9->Request().RxKind() == f9fmkt_RxKind_RequestNew) {
            ordraw9->Bid_.LeavesQty_ = ordraw9->Bid_.BeforeQty_;
            ordraw9->Offer_.LeavesQty_ = ordraw9->Offer_.BeforeQty_;
         }
         else if (ordraw9->Bid_.LeavesQty_ <= 0 || ordraw9->Offer_.LeavesQty_ <= 0) {
            // 刪改失敗, 想要重送, 但剩餘量已為0, 沒有重送的必要.
            return false;
         }
      }
      return true;
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

   static fon9::intrusive_ptr<TestCore> MakeCoreMgr(int argc, const char* argv[]) {
      fon9::intrusive_ptr<TestCore> core{new TestCore{argc, argv}};
      core->Owner_->Add(&core->GetResource());
      return core;
   }

   TestCore(int argc, const char* argv[], std::string name = "ut", f9omstw::OmsCoreMgrSP owner = nullptr)
      : TestCore(argc, argv, 0, name, owner) {
   }
   TestCore(int argc, const char* argv[], uint32_t forceTDay, std::string name = "ut", f9omstw::OmsCoreMgrSP owner = nullptr)
      : base(fon9::LocalNow(), forceTDay, owner ? owner : new f9omstw::OmsCoreMgr{nullptr}, "seed/path", name) {
      this->ThreadId_ = fon9::GetThisThreadId().ThreadId_;

      if (owner.get() == nullptr) {
         this->Owner_->SetOrderFactoryPark(new f9omstw::OmsOrderFactoryPark(
            new OmsTwsOrderFactory{"TwsOrd"},
            new OmsTwfOrder1Factory{"TwfOrd"},
            new OmsTwfOrder7Factory{"TwfOrdQR"},
            new OmsTwfOrder9Factory{"TwfOrdQ"}
         ));
         this->Owner_->SetEventFactoryPark(new f9omstw::OmsEventFactoryPark(
            new f9omstw::OmsEventInfoFactory(),
            new f9omstw::OmsEventSessionStFactory()
         ));
         gAllocFrom = static_cast<AllocFrom>(fon9::StrTo(fon9::GetCmdArg(argc, argv, "f", "allocfrom"), 0u));
         if (!gIsLessInfo)
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
      this->Brks_->Initialize(&UtwsBrk::BrkMaker, "8610", 5u, &f9omstw_IncStrAlpha);
      // 建立委託書號表的關聯.
      this->Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwSEC);
      this->Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwOTC);
      this->Brks_->InitializeTwfOrdNoMap(f9fmkt_TradingMarket_TwFUT);
      this->Brks_->InitializeTwfOrdNoMap(f9fmkt_TradingMarket_TwOPT);
   }

   ~TestCore() {
      this->Backend_OnBeforeDestroy();
      this->Brks_->InThr_OnParentSeedClear();
      if (this->IsWaitQuit_) {
         // 要查看資源用量(時間、記憶體...), 可透過 `/usr/bin/time` 指令:
         //    /usr/bin/time --verbose ~/devel/output/f9omstw/release/f9omstw/OmsReqOrd_UT
         // 或在結束前先暫停, 在透過其他外部工具查看:
         //    ~/devel/output/f9omstw/release/f9omstw/OmsReqOrd_UT -w
         //    例如: $cat /proc/19420(pid)/status 查看 VmSize
         std::cout << "Press <Enter> to quit.";
         getchar();
      }
   }
   void Backend_OnBeforeDestroy() {
      auto lastSNO = this->Backend_.LastSNO();
      this->Backend_.OnBeforeDestroy();
      if (lastSNO > 0 && !gIsLessInfo)
         std::cout << this->Name_ << ".LastSNO = " << lastSNO << std::endl;
   }

   void OpenReload(int argc, const char* argv[], std::string fnDefault) {
      const auto  outfn = fon9::GetCmdArg(argc, argv, "o", "out");
      if (!outfn.empty())
         fnDefault = outfn.ToString();
      if (auto forceTDay = this->ForceTDayId()) {
         if (fnDefault.size() >= 4 && memcmp(&*(fnDefault.end() - 4), ".log", 4) == 0)
            fnDefault.resize(fnDefault.size() - 4);
         fnDefault += fon9::RevPrintTo<std::string>('.', forceTDay, ".log");
      }
      StartResult res = this->Start(fnDefault, fon9::TimeInterval_Microsecond(1), -1);
      if (res.IsError())
         std::cout << "OmsCore.Reload error:" << fon9::RevPrintTo<std::string>(res) << std::endl;
   }

   f9omstw::OmsResource& GetResource() {
      return *static_cast<f9omstw::OmsResource*>(this);
   }

   bool RunCoreTask(f9omstw::OmsCoreTask&& task) override {
      task(this->GetResource());
      return true;
   }
   bool MoveToCoreImpl(f9omstw::OmsRequestRunner&& runner) override {
      // TestCore: 使用 單一 thread, 無 locker, 所以直接呼叫
      this->ThreadId_ = fon9::GetThisThreadId().ThreadId_;
      this->RunInCore(std::move(runner));
      return true;
   }
};
//--------------------------------------------------------------------------//
class DummyBrk : public f9omstw::OmsBrk {
   fon9_NON_COPY_NON_MOVE(DummyBrk);
   using base = OmsBrk;

protected:
   f9omstw::OmsIvacSP MakeIvac(f9omstw::IvacNo) override { return nullptr; }

public:
   DummyBrk(const fon9::StrView& brkid) : base(brkid) {
   }

   static f9omstw::OmsBrkSP BrkMaker(const fon9::StrView& brkid) {
      return new DummyBrk{brkid};
   }
   static void MakeBrkTree(f9omstw::OmsResource& res) {
      res.Brks_.reset(new f9omstw::OmsBrkTree(res.Core_, fon9::seed::LayoutSP{}, &f9omstw::OmsBrkTree::TwsBrkIndex1));
      res.Brks_->Initialize(&BrkMaker, "8610", 1u, &f9omstw_IncStrAlpha);
   }
};
#endif//__f9utw_UnitTestCore_hpp__
