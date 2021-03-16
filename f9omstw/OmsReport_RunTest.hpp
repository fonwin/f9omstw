// \file f9omstw/OmsReport_RunTest.hpp
//
// - 必須額外提供 void InitTestCore(TestCore& core);
//
// \author fonwinz@gmail.com
#include "f9utw/UnitTestCore.hpp"
//--------------------------------------------------------------------------//
void AssignKeyFromRef(f9omstw::OmsRequestBase& dst, const fon9::fmkt::TradingRxItem* ref) {
   if (ref) {
      if (auto ordraw = dynamic_cast<const f9omstw::OmsOrderRaw*>(ref))
         dst.OrdNo_ = ordraw->OrdNo_;
      else if (auto req = dynamic_cast<const f9omstw::OmsRequestBase*>(ref)) {
         dst.OrdNo_ = req->LastUpdated()->Order().Tail()->OrdNo_;
         dst.ReqUID_ = req->ReqUID_;
      }
   }
}
const fon9::fmkt::TradingRxItem* RunRequest(f9omstw::OmsCore& core,
                                            const fon9::fmkt::TradingRxItem* ref,
                                            f9omstw::OmsRequestPolicySP pol,
                                            fon9::StrView reqstr) {
   auto runner = MakeOmsRequestRunner(core.Owner_->RequestFactoryPark(), reqstr);
   AssignKeyFromRef(*runner.Request_, ref);
   ref = runner.Request_.get();
   static_cast<f9omstw::OmsRequestTrade*>(runner.Request_.get())->SetPolicy(pol);
   core.MoveToCore(std::move(runner));
   return ref;
}
const fon9::fmkt::TradingRxItem* RunReport(TestCore& core,
                                           const fon9::fmkt::TradingRxItem* ref,
                                           fon9::StrView rptstr) {
   int  ch1 = rptstr.Get1st();
   if (!fon9::isalnum(ch1)) {
      rptstr.SetBegin(rptstr.begin() + 1);
      fon9::StrTrim(&rptstr);
   }
   auto runner = MakeOmsReportRunner(core.Owner_->RequestFactoryPark(), rptstr, f9fmkt_RxKind_Filled);
   AssignKeyFromRef(*runner.Request_, ref);
   auto retval = runner.Request_.get();
   core.MoveToCore(std::move(runner));
   if (ch1 != '=') {  // 測試重複回報: 應拋棄, 不會寫入 history.
      runner = MakeOmsReportRunner(core.Owner_->RequestFactoryPark(), rptstr, f9fmkt_RxKind_Filled);
      AssignKeyFromRef(*runner.Request_, ref);
      auto lastSNO = core.GetResource().Backend_.LastSNO();
      core.MoveToCore(std::move(runner));
      if (lastSNO != core.GetResource().Backend_.LastSNO()) {
         std::cout << "|err=Dup report." "\r[ERROR]" << std::endl;
         abort();
      }
   }
   return retval;
}
//--------------------------------------------------------------------------//
[[noreturn]] void AbortAtLine(size_t lineNo) {
   std::cout << "|lineNo=" << lineNo << "\r[ERROR]" << std::endl;
   abort();
}
void CheckOrderRaw(const f9omstw::OmsOrderRaw& ordraw, fon9::StrView chkstr, size_t lineNo) {
   fon9::StrView tag = fon9::StrFetchNoTrim(chkstr, '|');
   auto& ordfac = ordraw.Order().Creator();
   if (tag != &ordfac.Name_) {
      std::cout << "|err=OrderRaw creator not match, "
         "expected=" << tag.ToString() << "|curr=" << ordfac.Name_;
      AbortAtLine(lineNo);
   }
   fon9::seed::SimpleRawRd rd{ordraw};
   fon9::StrView           value;
   fon9::RevBufferFixedSize<1024> rbuf;
   while (fon9::StrFetchTagValue(chkstr, tag, value)) {
      auto* fld = ordfac.Fields_.Get(tag);
      if (fld == nullptr) {
         std::cout << "|err=OrderRaw field not found: " << tag.ToString();
         AbortAtLine(lineNo);
      }
      rbuf.Rewind();
      fld->CellRevPrint(rd, nullptr, rbuf);
      if (value != ToStrView(rbuf)) {
         std::cout << "|err=OrderRaw field not match|field=" << fld->Name_
            << "|expected=" << value.ToString() << "|curr=" << rbuf.ToStrT<std::string>();
         AbortAtLine(lineNo);
      }
   }
}
//--------------------------------------------------------------------------//
using SnoVect = std::vector<f9omstw::OmsRxSNO>;
const fon9::fmkt::TradingRxItem* GetReference(TestCore& core,
                                              f9omstw::OmsRxSNO snoCurr,
                                              const SnoVect& snoVect,
                                              fon9::StrView& rptstr) {
   if (rptstr.Get1st() == 'L') { // lineNo
      rptstr.SetBegin(rptstr.begin() + 1);
      snoCurr = snoVect[fon9::StrTo(&rptstr, 0u) - 1];
   }
   else
      snoCurr = snoCurr - fon9::StrTo(&rptstr, 0u);
   if (!fon9::StrTrim(&rptstr).empty() && !fon9::isalpha(*rptstr.begin())) {
      switch (rptstr.Get1st()) {
      case '.': case ' ':
         rptstr.SetBegin(rptstr.begin() + 1);
         fon9::StrTrim(&rptstr);
         break;
      }
   }
   return core.GetResource().Backend_.GetItem(snoCurr);
}

using TestCoreSP = fon9::intrusive_ptr<TestCore>;
typedef void (*FnBeforeAddReq)(TestCoreSP& core, f9omstw::OmsRequestPolicySP& poAdmin);

void RunTestCore(FnBeforeAddReq fnBeforeAddReq,
                 TestCoreSP& core, f9omstw::OmsRequestPolicySP& pol,
                 const char** cstrTestList, size_t count) {
   f9omstw::OmsRxSNO                snoCurr = core->GetResource().Backend_.LastSNO() + 1;
   SnoVect                          snoVect(count);
   const fon9::fmkt::TradingRxItem* rxItem;
   const f9omstw::OmsOrderRaw*      ordraw;
   std::string                      rptstrApp;
   int                              chHeadPrev{0};
   for (size_t lineNo = 1; lineNo <= count; ++lineNo) {
      fon9::StrView  rptstr = fon9::StrView_cstr(*cstrTestList++);
      // [0]: '+'=Report占用序號, '-'=Report不占用序號, '*'=Request, '/'=OrderRaw
      //      'v'=「新單合併成交」(占用「下一個」序號).
      const int chHead = fon9::StrTrim(&rptstr).Get1st();
      if (chHead == EOF)
         continue;
      std::string tmpstr;
      fon9::StrTrimHead(&rptstr, rptstr.begin() + 1);
      if (!rptstrApp.empty()) {
         tmpstr = rptstr.ToString() + rptstrApp;
         rptstr = &tmpstr;
         rptstrApp.clear();
      }
      switch (chHead) {
      default:
         continue;
      case 'v':
         // "v1.TwfFil|..."  snoCurr + 2
         // "/  TwfOrd|..."  snoCurr + 1
         // "/  TwfOrd|..."  snoCurr + 3
      case '+':
         if (rptstr.Get1st() != 'L' && !fon9::isdigit(rptstr.Get1st())) {
            fnBeforeAddReq(core, pol);
            rxItem = RunReport(*core, nullptr, rptstr);
            break;
         }
         /* fall through */ // "+N.RptName" 接續前第N筆的異動.
      case '-':
         fnBeforeAddReq(core, pol);
         rxItem = GetReference(*core, snoCurr, snoVect, rptstr);
         rxItem = RunReport(*core, rxItem, rptstr);
         if (chHead == '-') // 不占序號的回報.
            continue;
         break;
      case '*':
         fnBeforeAddReq(core, pol);
         rxItem = (rptstr.Get1st() == 'L' || fon9::isdigit(rptstr.Get1st()))
            ? GetReference(*core, snoCurr, snoVect, rptstr)
            : nullptr;
         rxItem = RunRequest(*core, rxItem, pol, rptstr);
         break;
      case '/':
         rxItem = core->GetResource().Backend_.GetItem(snoCurr);
         ordraw = dynamic_cast<const f9omstw::OmsOrderRaw*>(rxItem);
         if (ordraw == nullptr) {
            std::cout << "|err=Not OrderRaw SNO:" << snoCurr;
            AbortAtLine(lineNo);
         }
         CheckOrderRaw(*ordraw, rptstr, lineNo);
         break;
      case '>':
         // ">AllocOrdNo=M-S-BrkId-A"  M=Market, S=SessionId, A=TeamNo
         fon9::StrView tag, value;
         fon9::StrFetchTagValue(rptstr, tag, value);
         if (tag == "AllocOrdNo") {
            auto mkt = static_cast<f9fmkt_TradingMarket>(fon9::StrFetchTrim(value, '-').Get1st());
            auto ses = static_cast<f9fmkt_TradingSessionId>(fon9::StrFetchTrim(value, '-').Get1st());
            auto brk = core->GetResource().Brks_->GetBrkRec(fon9::StrFetchTrim(value, '-'));
            if (brk == nullptr) {
               std::cout << "|err=AllocOrdNo:Brk not found." << snoCurr;
               AbortAtLine(lineNo);
            }
            auto ordnoMap = brk->GetMarket(mkt).GetSession(ses).GetOrdNoMap();
            if (ordnoMap == nullptr) {
               std::cout << "|err=AllocOrdNo:OrdNoMap not found." << snoCurr;
               AbortAtLine(lineNo);
            }
            f9omstw::OmsOrdNo  ordno;
            ordnoMap->GetNextOrdNo(fon9::StrTrimHead(&value), ordno);
            rptstrApp = "|OrdNo=";
            rptstrApp.append(ordno.begin(), ordno.end());
         }
         continue;
      }
      f9omstw::OmsRxSNO rxSNO = snoCurr++;
      if (chHead == 'v') {
         // 占用下一序號的回報.
         ++rxSNO;
         --snoCurr; // "v" 下一行的委託序號.
         chHeadPrev = '\v';
      }
      else {
         if (chHeadPrev == '\v')
            ++snoCurr;
         chHeadPrev = chHead;
      }
      if (rxItem->RxSNO() != rxSNO) {
         std::cout << "|err=Unexpected SNO:" << rxItem->RxSNO() << "|expected=" << rxSNO;
         AbortAtLine(lineNo);
      }
      snoVect[lineNo - 1] = rxSNO;
   }
}
//--------------------------------------------------------------------------//
int      gArgc;
char**   gArgv;
bool     gIsTestReload = false;
void InitReportUT(int argc, char* argv[]) {
   gArgc = argc;
   gArgv = argv;

   const auto isTestReload = fon9::GetCmdArg(argc, argv, "r", "reload");
   gIsTestReload = (isTestReload.begin() && (isTestReload.empty() || fon9::toupper(static_cast<unsigned char>(*isTestReload.begin())) == 'Y'));
   gIsLessInfo = true;
   fon9::LogLevel_ = fon9::LogLevel::Warn;
}

void InitTestCore(TestCore& core);

void ResetTestCore(TestCoreSP& core, f9omstw::OmsRequestPolicySP& poAdmin) {
   core.reset();
   core.reset(new TestCore{gArgc, gArgv});
   core->IsWaitQuit_ = false;

   InitTestCore(*core);

   f9omstw::OmsRequestPolicyCfg  polcfg;
   #define kCSTR_TEAM_NO   "A"
   polcfg.TeamGroupName_.assign("admin");
   polcfg.UserRights_.AllowOrdTeams_.assign(kCSTR_TEAM_NO);
   polcfg.IvList_.kfetch(f9omstw::OmsIvKey{"*"}).second = f9omstw::OmsIvRight::AllowAll;
   poAdmin = polcfg.MakePolicy(core->GetResource());
}
void ResetTestCoreDummy(TestCoreSP&, f9omstw::OmsRequestPolicySP&) {
}
//--------------------------------------------------------------------------//
void RunTestList(TestCoreSP& core, const char* testName, const char** cstrTestList, size_t count) {
   f9omstw::OmsRequestPolicySP pol;
   ResetTestCore(core, pol);
   std::this_thread::sleep_for(std::chrono::milliseconds{100}); // 等候 Backend thread 啟動.
   std::cout << "[TEST ] " << testName;
   RunTestCore(&ResetTestCoreDummy, core, pol, cstrTestList, count);
   std::cout << "\r[OK   ]" << std::endl;
   // 每遇到 Req or Rpt 就先重啟 core: 測試 reload 之後是否會正確.
   if (gIsTestReload)
      RunTestCore(&ResetTestCore, core, pol, cstrTestList, count);
}
template <size_t count>
void RunTestList(TestCoreSP& core, const char* testName, const char* (&cstrTestList)[count]) {
   RunTestList(core, testName, cstrTestList, count);
}
//--------------------------------------------------------------------------//
#define kOrdReqST(ordst,reqst) "|OrdSt=" fon9_CTXTOCSTR(ordst) "|ReqSt=" fon9_CTXTOCSTR(reqst)
#define kOrdReqVST(val)        kOrdReqST(kVAL_##val, kVAL_##val)
#define kRptST(st)             "|ReportSt=" fon9_CTXTOCSTR(st)
#define kRptVST(st)            kRptST(kVAL_##st)

#define kVAL_Initialing       0
#define kVAL_Sending          30
#define kVAL_ExchangeAccepted f2
#define kVAL_PartFilled       f3
#define kVAL_FullFilled       f4
#define kVAL_ExchangeNoLeaves ef
#define kVAL_ExchangeRejected ee
#define kVAL_AsCanceled       f5
#define kVAL_Canceling        f7
#define kVAL_ExchangeCanceled fa
#define kVAL_UserCanceled     fd

#define kVAL_ReportPending    1
#define kVAL_ReportStale      2
//--------------------------------------------------------------------------//
#define kOrdCum(qty,amt,tmstr)          \
         "|CumQty=" fon9_CTXTOCSTR(qty) \
         "|CumAmt=" fon9_CTXTOCSTR(amt) \
         "|LastFilledTime=" tmstr

#define kOrdQtys(bfQty,afQty,leaves)         \
         "|BeforeQty=" fon9_CTXTOCSTR(bfQty) \
         "|AfterQty="  fon9_CTXTOCSTR(afQty) \
         "|LeavesQty=" fon9_CTXTOCSTR(leaves)
//--------------------------------------------------------------------------//
