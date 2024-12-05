// \file f9omstw/OmsCx_UT.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCx_UT_hpp__
#define __f9omstw_OmsCx_UT_hpp__
#include "f9omstw/OmsCxReqBase.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsCxSymbTree.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9utw/UtwsSymb.hpp"
#include "f9utw/UtwsBrk.hpp"
#include "f9utw/UtwRequests.hpp"
#include "fon9/TestTools.hpp"
using namespace f9omstw;
// ======================================================================== //
constexpr unsigned kPoolObjCount = 100;
using TwsOrderRaw     = UtwsOrderRaw;
using TwsNewRequest   = UtwsRequestIni;
using TwsNewRequestSP = fon9::intrusive_ptr<TwsNewRequest>;
using TwsOrderFactory = OmsOrderFactoryT<OmsOrder, TwsOrderRaw, kPoolObjCount>;
using TwsNewFactory   = OmsRequestFactoryT<TwsNewRequest, kPoolObjCount>;
#define kBrkId    "8610"
//--------------------------------------------------------------------------//
struct TwsNewRiskCheckStep : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(TwsNewRiskCheckStep);
   using base = OmsRequestRunStep;
   using base::base;
   void RunRequest(OmsRequestRunnerInCore&& runner) override {
      auto& ordraw = runner.OrderRawT<TwsOrderRaw>();
      auto* inireq = static_cast<const TwsNewRequest*>(ordraw.Order().Initiator());
      ordraw.LastPri_ = inireq->Pri_;
      ordraw.LastPriType_ = inireq->PriType_;
      if (inireq->RxKind() == f9fmkt_RxKind_RequestNew) {
         ordraw.BeforeQty_ = 0;
         // 全新的新單要求, 若 LeavesQty_ > 0: 則可能是經過某些處理後(例:條件成立後), 的實際下單數量;
         if (ordraw.LeavesQty_ == 0)
            ordraw.LeavesQty_ = inireq->Qty_;
         ordraw.AfterQty_ = ordraw.LeavesQty_;
      }
      this->ToNextStep(std::move(runner));
   }
};
struct TwsNewSendStep : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(TwsNewSendStep);
   using base = OmsRequestRunStep;
   TwsNewSendStep() = default;
   void RunRequest(OmsRequestRunnerInCore&& runner) override {
      runner.Update(f9fmkt_TradingRequestSt_Sent);
   }
};
//--------------------------------------------------------------------------//
struct TestCore : public OmsCore {
   fon9_NON_COPY_NON_MOVE(TestCore);
   using base = OmsCore;
   TwsNewFactory*       TwsNewFactory_;
   OmsRequestPolicySP   ReqPolicy_;

   TestCore(int argc, const char* argv[], std::string name = "ut")
      : base(fon9::LocalNow(), 0/*forceTDay*/, new OmsCoreMgr{nullptr}, "seed/path", name) {
      (void)argc; (void)argv;
      this->ThreadId_ = fon9::GetThisThreadId().ThreadId_;

      this->UsrDef_.reset(new UtwOmsCoreUsrDef{});

      this->Brks_ = new OmsBrkTree(*this, UtwsBrk::MakeLayout(OmsBrkTree::DefaultTreeFlag()), &OmsBrkTree::TwsBrkIndex1);
      this->Brks_->Initialize(&UtwsBrk::BrkMaker, kBrkId, 1, &f9omstw_IncStrAlpha);

      OmsCreateSymbTreeResouce<OmsSymbTree, UtwsSymb>(this->GetResource(), true);

      TwsOrderFactory* twsOrdFactory = new TwsOrderFactory{"TwsOrd"};
      this->TwsNewFactory_ = new TwsNewFactory("TwsNew", twsOrdFactory,
                  OmsRequestRunStepSP{new UtwsCondStepBfSc(
                  OmsRequestRunStepSP{new TwsNewRiskCheckStep(
                  OmsRequestRunStepSP{new UtwsCondStepAfSc(
                  OmsRequestRunStepSP{new TwsNewSendStep}
                  )} )} )} );
      this->Owner_->SetOrderFactoryPark(new OmsOrderFactoryPark(
         twsOrdFactory
      ));
      this->Owner_->SetRequestFactoryPark(new OmsRequestFactoryPark(
         this->TwsNewFactory_
      ));
      OmsRequestPolicy* reqPolicy;
      this->ReqPolicy_.reset(reqPolicy = new OmsRequestPolicy);
      reqPolicy->SetCondAllows("*");
      reqPolicy->SetCondPriorityM(10000);
      reqPolicy->SetCondPriorityL(20000);
      reqPolicy->SetCondExpMaxC(20);
      reqPolicy->SetCondGrpMaxC(20);
      reqPolicy->AddIvConfig(nullptr, "*", OmsIvConfig{});
   }
   ~TestCore() {
      this->Brks_->InThr_OnParentSeedClear();
   }
   OmsSymbSP FetchSymb(fon9::StrView symbId) {
      OmsSymbSP symb = this->GetResource().Symbs_->FetchOmsSymb(symbId);
      if (symb->TradingMarket_ == f9fmkt_TradingMarket_Unknown) {
         symb->TradingMarket_ = f9fmkt_TradingMarket_TwSEC;
         symb->TradingSessionId_ = f9fmkt_TradingSessionId_Normal;
      }
      return symb;
   }
   OmsResource& GetResource() {
      return *static_cast<OmsResource*>(this);
   }
   // TestCore: 使用 單一 thread, 無 locker, 所以直接呼叫
   bool RunCoreTask(OmsCoreTask&& task) override {
      task(this->GetResource());
      return true;
   }
   bool MoveToCoreImpl(OmsRequestRunner&& runner) override {
      this->RunInCore(std::move(runner));
      return true;
   }
   TwsNewRequestSP MakeTwsNewRequest() {
      TwsNewRequestSP retval = fon9::static_pointer_cast<TwsNewRequest>(this->TwsNewFactory_->MakeRequest());
      retval->SetPolicy(this->ReqPolicy_);
      retval->BrkId_.CopyFrom(kBrkId, sizeof(kBrkId) - 1);
      return retval;
   }
   f9fmkt_TradingRequestSt RunRequest(TwsNewRequestSP req) {
      OmsRequestRunner  runner;
      runner.Request_ = req;
      this->MoveToCore(std::move(runner));
      const auto* ordraw = req->LastUpdated();
      return(ordraw ? ordraw->RequestSt_ : req->ReportSt());
   }

   enum TestFlag {
      /// 當 req 因行情觸發執行, 自動建立一個相同 CxArgs 的 req, 檢查是否會立即觸發執行;
      TestFlag_AutoChkFireNow = 0x01,
      /// 使用 TestCast() 時, 用 TestCastImpl() 測試一次之後,
      /// 是否自動調整 testItemName 及 cfgstr 的內容, 用以自動測試零股;
      TestFlag_AutoChkOddLot = 0x02,
      TestFlag_AutoAll  = TestFlag_AutoChkFireNow | TestFlag_AutoChkOddLot,
      TestFlag_AutoNone = 0,
   };
   // 測試方法:
   // for(;;) {
   //    SymbSet:設定 Symb Md 初始值;
   //    ReqMake:CxArgs => ReqRun:檢查RequestSt;
   //    for(;;) {
   //       SymbEv(設定 Symb Md, 並觸發事件) => ReqChk:RequestSt;
   //       // 測試 IsNeedsFireNow 必須與 ReqMake 的 req 相同;
   //       建立Req(CxArgs) => ReqRun => RequestSt;
   //    }
   // }
   // ---------- cfgstr 資料範例:
   // SymbId:  1101
   // SymbSet: MdDeal:LastPrice=|LastQty=|TotalQty=
   // ReqMake: LP>100
   // ReqRun:  Waiting
   //
   // SymbEv:  MdDeal:LastPrice=|LastQty=|TotalQty=
   // ReqChk:  Waiting
   //
   // SymbEv:  MdDeal:LastPrice=|LastQty=|TotalQty=
   // ReqChk:  Sent
   void TestCase(TestFlag flags, std::string testItemName, std::string cfgstr);

   struct TestImpl {
      OmsSymbSP         CxSymb_;
      TwsNewRequestSP   CxReq_;
      unsigned          LnCount_ = 0;
      char              Padding_[4];
      bool Check_CxSymb();
      bool Check_CxReq();
      void FetchSymb(TestCore& owner, fon9::StrView symbId);
      /// cfgln = TabName:Field=Value|...
      const fon9::seed::Tab* SetSymbFieldValues(TestCore& owner, fon9::StrView cfgln);
      /// evName = "MdDeal", "MdDealOddLot", "MdBS", "MdBSOddLot"
      void FireSymbEv(TestCore& owner, fon9::StrView evName);
      void ReqMake(TestCore& owner, fon9::StrView condArgs);
      /// finalSt: RequestSt 或 ec=ErrCode
      void ReqRun(TestCore& owner, fon9::StrView finalSt);
      void ReqChk(TestCore& owner, fon9::StrView reqSt, TestFlag flags);
   };
   void TestCaseImpl(TestFlag flags, const char* testItemName, fon9::StrView cfgstr);
};
fon9_ENABLE_ENUM_BITWISE_OP(TestCore::TestFlag);
//--------------------------------------------------------------------------//
bool TestCore::TestImpl::Check_CxSymb() {
   if (this->CxSymb_)
      return true;
   printf("\n" "Ln:%u: " "Must first use 'SymbId:' to obtain the symb.\n", this->LnCount_);
   abort();
// return false;
}
bool TestCore::TestImpl::Check_CxReq() {
   if (this->CxReq_)
      return true;
   printf("\n" "Ln:%u: " "Must first use 'ReqMake:' to make the request.\n", this->LnCount_);
   abort();
// return false;
}

static f9fmkt_TradingRequestSt StrToRequestSt(fon9::StrView str, f9fmkt_TradingRequestSt nullValue = f9fmkt_TradingRequestSt{}) {
   fon9::StrTrim(&str);
   if (fon9::iequals(str, "Waiting"))
      return f9fmkt_TradingRequestSt_WaitingCond;
   if (fon9::iequals(str, "Sent"))
      return f9fmkt_TradingRequestSt_Sent;
   if (fon9::iequals(str, "Done"))
      return f9fmkt_TradingRequestSt_Done;
   return nullValue;
}
void TestCore::TestImpl::FetchSymb(TestCore& owner, fon9::StrView symbId) {
   this->CxSymb_ = owner.FetchSymb(symbId);
   if (!this->CxSymb_) {
      printf("\n" "Ln:%u: " "Symb not found: %s\n", this->LnCount_, symbId.ToString().c_str());
      abort();
   }
}
const fon9::seed::Tab* TestCore::TestImpl::SetSymbFieldValues(TestCore& owner, fon9::StrView cfgln) {
   if (!this->Check_CxSymb())
      return nullptr;
   fon9::StrView tabName = fon9::StrFetchTrim(cfgln, ':');
   const auto* tab = owner.Symbs_->LayoutSP_->GetTab(tabName);
   if (tab == nullptr) {
      printf("\n" "Ln:%u: " "Symb TabName(%s) not found.\n", this->LnCount_, tabName.ToString().c_str());
      abort();
   }
   fon9::seed::SimpleRawWr wr{*this->CxSymb_->GetSymbData(tab->GetIndex())};
   while (!cfgln.empty()) {
      fon9::StrView fldName = fon9::StrFetchTrim(cfgln, '=');
      const auto*   fld = tab->Fields_.Get(fldName);
      if (fld == nullptr) {
         printf("\n" "Ln:%u: " "Field(%s) not found.\n", this->LnCount_, fldName.ToString().c_str());
         abort();
      }
      fld->StrToCell(wr, fon9::StrFetchTrim(cfgln, '|'));
   }
   return tab;
}
void TestCore::TestImpl::FireSymbEv(TestCore& owner, fon9::StrView evName) {
   if (!this->Check_CxSymb())
      return;
   fon9::StrTrim(&evName);
   OmsMdLastPriceEv* evMdDeal;
   OmsMdBSEv*        evMdBS;
   if (fon9::iequals(evName, "MdDeal")) {
      evMdDeal = this->CxSymb_->GetMdLastPriceEv();
      evMdDeal->OnMdLastPriceEv(*evMdDeal, owner);
   }
   else if (fon9::iequals(evName, "MdDealOddLot")) {
      evMdDeal = this->CxSymb_->GetMdLastPriceEv_OddLot();
      evMdDeal->OnMdLastPriceEv(*evMdDeal, owner);
   }
   else if (fon9::iequals(evName, "MdBS")) {
      evMdBS = this->CxSymb_->GetMdBSEv();
      evMdBS->OnMdBSEv(*evMdBS, owner);
   }
   else if (fon9::iequals(evName, "MdBSOddLot")) {
      evMdBS = this->CxSymb_->GetMdBSEv_OddLot();
      evMdBS->OnMdBSEv(*evMdBS, owner);
   }
}
void TestCore::TestImpl::ReqMake(TestCore& owner, fon9::StrView condArgs) {
   if (!this->Check_CxSymb())
      return;
   this->CxReq_ = owner.MakeTwsNewRequest();
   this->CxReq_->Symbol_.AssignFrom(ToStrView(this->CxSymb_->SymbId_));
   this->CxReq_->CxArgs_.assign(fon9::StrTrim(&condArgs));
   this->CxReq_->Qty_ = 100;
}
void TestCore::TestImpl::ReqRun(TestCore& owner, fon9::StrView finalSt) {
   if (!this->Check_CxReq())
      return;
   f9fmkt_TradingRequestSt expectedSt = StrToRequestSt(fon9::StrTrim(&finalSt), f9fmkt_TradingRequestSt{});
   OmsErrCode              ec = OmsErrCode_Null;
   if (expectedSt == f9fmkt_TradingRequestSt{}) {
      fon9::StrView tagv = finalSt;
      if (fon9::iequals("ec", fon9::StrFetchTrim(tagv, '=')))
         ec = static_cast<OmsErrCode>(fon9::StrTo(tagv, fon9::cast_to_underlying(OmsErrCode_Null)));
      else {
         printf("\n" "Ln:%u: " "Unknown RequestSt(%s).\n", this->LnCount_, finalSt.ToString().c_str());
         abort();
      }
   }
   f9fmkt_TradingRequestSt  resultSt = owner.RunRequest(this->CxReq_);
   if (expectedSt == f9fmkt_TradingRequestSt{}) {
      if (ec == this->CxReq_->ErrCode())
         return;
      printf("\n" "Ln:%u: " "ReqRun: ErrCode not match, expect(%u) != %u\n", this->LnCount_, ec, this->CxReq_->ErrCode());
      abort();
   }
   if (expectedSt != resultSt) {
      printf("\n" "Ln:%u: " "ReqRun: RequestSt not match, expect(%s:0x%02x) != 0x%02x.", this->LnCount_,
             finalSt.ToString().c_str(), expectedSt, resultSt);
      if ((ec = this->CxReq_->ErrCode()) != OmsErrCode_Null)
         printf(" ec=%u", ec);
      puts("");
      abort();
   }
}
void TestCore::TestImpl::ReqChk(TestCore& owner, fon9::StrView reqSt, TestFlag flags) {
   if (!this->Check_CxReq())
      return;
   const auto* ordraw = this->CxReq_->LastUpdated();
   if (ordraw == nullptr) {
      printf("\n" "Ln:%u: " "The Request has not been run yet.\n", this->LnCount_);
      abort();
   }
   f9fmkt_TradingRequestSt expectedSt = StrToRequestSt(fon9::StrTrim(&reqSt), f9fmkt_TradingRequestSt{});
   if (ordraw->RequestSt_ != expectedSt) {
      printf("\n" "Ln:%u: " "ReqChk: RequestSt not match, expect(%s:0x%02x) != 0x%02x.\n", this->LnCount_,
             reqSt.ToString().c_str(), expectedSt, ordraw->RequestSt_);
      abort();
   }
   if (!IsEnumContains(flags, TestFlag_AutoChkFireNow))
      return;
   // 同條件內容, 應該會與 this->CxReq_ 相同: 立即觸發 or 等候.
   auto reReq = owner.MakeTwsNewRequest();
   reReq->Symbol_ = this->CxReq_->Symbol_;
   reReq->CxArgs_ = this->CxReq_->CxArgs_;
   reReq->Qty_ = this->CxReq_->Qty_;
   f9fmkt_TradingRequestSt  resultSt = owner.RunRequest(reReq);
   if (expectedSt != resultSt) {
      printf("\n" "Ln:%u: " "ReqChk: rerun RequestSt not match, expect(%s:0x%02x) != 0x%02x.\n", this->LnCount_,
             reqSt.ToString().c_str(), expectedSt, resultSt);
      abort();
   }
}

void TestCore::TestCaseImpl(TestFlag flags, const char* testItemName, fon9::StrView cfgstr) {
   printf("[TEST ] %s", testItemName);
   TestImpl impl;
   while (!cfgstr.empty()) {
      ++impl.LnCount_;
      fon9::StrView cfgln = fon9::StrFetchNoTrim(cfgstr, '\n');
      // printf("Ln %3u: %s\n", lnCount, cfgln.ToString().c_str());
      if (fon9::StrTrim(&cfgln).empty())
         continue;
      if (cfgln.Get1st() == '#') {
         if (fon9::iequals(cfgln, "# debug-pause"))
            continue;
         continue;
      }
      fon9::StrView  cmd = fon9::StrFetchTrim(cfgln, ':');
      if (fon9::iequals(cmd, "SymbId"))
         impl.FetchSymb(*this, fon9::StrTrim(&cfgln));
      else if (fon9::iequals(cmd, "SymbSet"))
         impl.SetSymbFieldValues(*this, cfgln);
      else if (fon9::iequals(cmd, "SymbEv")) {
         if (const auto* tab = impl.SetSymbFieldValues(*this, cfgln))
            impl.FireSymbEv(*this, &tab->Name_);
      }
      else if (fon9::iequals(cmd, "ReqMake"))
         impl.ReqMake(*this, cfgln);
      else if (fon9::iequals(cmd, "ReqRun"))
         impl.ReqRun(*this, cfgln);
      else if (fon9::iequals(cmd, "ReqChk"))
         impl.ReqChk(*this, cfgln, flags);
      else if (fon9::iequals(cmd, "MakeRun")) {
         // ReqMake & ReqRun
         // finalSt|CondArgs
         fon9::StrView finalSt = fon9::StrFetchTrim(cfgln, ':');
         impl.ReqMake(*this, cfgln);
         impl.ReqRun(*this, finalSt);
      }
      else {
         printf("\n" "Ln:%u: " "Unknown test Command.\n", impl.LnCount_);
         abort();
      }
   }
   printf("\r" "[OK   ]\n");
}
void TestCore::TestCase(TestFlag flags, std::string testItemName, std::string cfgstr) {
   this->TestCaseImpl(flags, testItemName.c_str(), &cfgstr);
   if (IsEnumContains(flags, TestFlag_AutoChkOddLot))
      return;
   char& condName1 = testItemName[1];
   if (!fon9::isupper(condName1))
      return;
   const char condName0 = testItemName[0];
   fon9::StrView strOddLot{&cfgstr};
   while (!strOddLot.empty()) {
      fon9::StrView cfgln = fon9::StrFetchNoTrim(strOddLot, '\n');
      if (fon9::StrTrim(&cfgln).empty())
         continue;
      fon9::StrView cmd = fon9::StrFetchTrim(cfgln, ':');
      if (fon9::iequals(cmd, "SymbSet") || fon9::iequals(cmd, "SymbEv")) {
         cmd = fon9::StrFetchTrim(cfgln, ':');
         const unsigned ipos = static_cast<unsigned>(cmd.end() - &cfgstr[0]);
         constexpr char kStrOddLot[] = "OddLot";
         cfgstr.insert(ipos, kStrOddLot);
         strOddLot = fon9::StrView{&cfgstr};
         strOddLot.IncBegin(static_cast<unsigned>(ipos + sizeof(kStrOddLot) - 1));
      }
      else if (fon9::iequals(cmd, "ReqMake") && cfgln.size() > 2) {
         char* pcfg = const_cast<char*>(cfgln.begin() + 1);
         while (pcfg != cfgln.end()) {
            if (*(pcfg - 1) == condName0 && *pcfg == condName1)
               *pcfg = static_cast<char>(fon9::tolower(*pcfg));
            ++pcfg;
         }
      }
   }
   condName1 = static_cast<char>(fon9::tolower(condName1));
   this->TestCaseImpl(flags, testItemName.c_str(), &cfgstr);
}
// ======================================================================== //
#endif//__f9omstw_OmsCx_UT_hpp__
