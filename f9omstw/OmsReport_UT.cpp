// \file f9omstw/OmsReport_UT.cpp
// \author fonwinz@gmail.com
#include "f9utws/UnitTestCore.hpp"
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
const fon9::fmkt::TradingRxItem* RunReport(f9omstw::OmsCore& core,
                                           const fon9::fmkt::TradingRxItem* ref,
                                           fon9::StrView rptstr) {
   auto runner = MakeOmsReportRunner(core.Owner_->RequestFactoryPark(), rptstr, f9fmkt_RxKind_Filled);
   AssignKeyFromRef(*runner.Request_, ref);
   ref = runner.Request_.get();
   core.MoveToCore(std::move(runner));
   return ref;
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
      rptstr.SetBegin(rptstr.begin() + 1);
      fon9::StrTrim(&rptstr);
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
   for (size_t lineNo = 1; lineNo <= count; ++lineNo) {
      fon9::StrView  rptstr = fon9::StrView_cstr(*cstrTestList++);
      // [0]: '+'=Report占用序號, '-'=Report不占用序號, '*'=Request, '/'=OrderRaw
      int chHead = fon9::StrTrim(&rptstr).Get1st();
      if (chHead == EOF)
         continue;
      fon9::StrTrimHead(&rptstr, rptstr.begin() + 1);
      switch (chHead) {
      case '+':
         if (rptstr.Get1st() != 'L' && !fon9::isdigit(rptstr.Get1st())) {
            fnBeforeAddReq(core, pol);
            rxItem = RunReport(*core, nullptr, rptstr);
            break;
         }
         // "+N.RptName" 接續前第N筆的異動.
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
      default:
         continue;
      }
      if (rxItem->RxSNO() != snoCurr) {
         std::cout << "|err=Unexpected SNO:" << snoCurr;
         AbortAtLine(lineNo);
      }
      snoVect[lineNo - 1] = snoCurr;
      ++snoCurr;
   }
}
//--------------------------------------------------------------------------//
int      gArgc;
char**   gArgv;
void ResetTestCore(TestCoreSP& core, f9omstw::OmsRequestPolicySP& poAdmin) {
   core.reset();
   core.reset(new TestCore{gArgc, gArgv});
   core->IsWaitQuit_ = false;
   auto  ordfac = core->Owner_->OrderFactoryPark().GetFactory("TwsOrd");
   core->Owner_->SetRequestFactoryPark(new f9omstw::OmsRequestFactoryPark(
      new OmsTwsRequestIniFactory("TwsNew", ordfac,
                                  f9omstw::OmsRequestRunStepSP{new UomsTwsIniRiskCheck{
      f9omstw::OmsRequestRunStepSP{new UomsTwsExgSender}}}),
      new OmsTwsRequestChgFactory("TwsChg",
      f9omstw::OmsRequestRunStepSP{new UomsTwsExgSender}),
      new OmsTwsFilledFactory("TwsFil", ordfac),
      new OmsTwsReportFactory("TwsRpt", ordfac)
      ));
   core->OpenReload(gArgc, gArgv, "OmsReport_UT.log");
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
void RunTest(const char* testName, const char** cstrTestList, size_t count) {
   TestCoreSP core;
   f9omstw::OmsRequestPolicySP pol;
   ResetTestCore(core, pol);
   std::this_thread::sleep_for(std::chrono::milliseconds{100}); // 等候 Backend thread 啟動.
   std::cout << "[TEST ] " << testName;
   RunTestCore(&ResetTestCoreDummy, core, pol, cstrTestList, count);
   std::cout << "\r[OK   ]" << std::endl;
   // 每遇到 Req or Rpt 就先重啟 core: 測試 reload 之後是否會正確.
   RunTestCore(&ResetTestCore, core, pol, cstrTestList, count);
}
//--------------------------------------------------------------------------//
#define kREQ_BASE             "|Market=T|SessionId=N|BrkId=8610"
#define kREQ_SYMB             kREQ_BASE "|IvacNo=10|Symbol=2317|Side=B"

#define kCHK_ST(ordst,reqst)  "|OrdSt=" fon9_CTXTOCSTR(ordst) "|ReqSt=" fon9_CTXTOCSTR(reqst)
#define kVAL_Sending          30
#define kVAL_ExchangeAccepted f2
#define kVAL_PartFilled       f3
#define kVAL_FullFilled       f4
#define kVAL_ExchangeNoLeaves ef
#define kVAL_PartCanceled     f5
#define kVAL_ExchangeCanceled fa
#define kVAL_UserCanceled     fd

#define kCHK_NewSending       kCHK_ST(kVAL_Sending, kVAL_Sending)
#define kCHK_ExchangeAccepted kCHK_ST(kVAL_ExchangeAccepted, kVAL_ExchangeAccepted)
#define kCHK_PartFilled       kCHK_ST(kVAL_PartFilled, kVAL_PartFilled)
#define kCHK_PartFilled_Cum(qty,amt,tmstr) \
         kCHK_PartFilled                   \
         "|CumQty=" fon9_CTXTOCSTR(qty)    \
         "|CumAmt=" fon9_CTXTOCSTR(amt)    \
         "|LastFilledTime=" tmstr

#define kVAL_ReportPending    1
#define kVAL_ReportStale      2
#define kREQ_ReportSt(st)     "|ReportSt=" fon9_CTXTOCSTR(st)

#define kREQ_PRI(pri)         "|Pri=" fon9_CTXTOCSTR(pri) "|PriType="
#define kREQ_MarketPRI        "|Pri=|PriType=M"

#define kCHK_PRI(pri)         "|LastPri=" fon9_CTXTOCSTR(pri) "|LastPriType="
#define kCHK_MarketPRI        "|LastPri=|LastPriType=M"
#define kCHK_QTYS(bf,af,le)   "|BeforeQty=" fon9_CTXTOCSTR(bf) "|AfterQty=" fon9_CTXTOCSTR(af) \
                              "|LeavesQty=" fon9_CTXTOCSTR(le)
//--------------------------------------------------------------------------//
int main(int argc, char* argv[]) {
   gArgc = argc;
   gArgv = argv;
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif
   fon9::AutoPrintTestInfo utinfo{"OmsReport"};
   //---------------------------------------------
   #define kREQ_P200_Q10   kREQ_SYMB kREQ_PRI(200) "|Qty=10000"
   const char* cstrNormalTestList[] = {
"*"   "TwsNew" kREQ_P200_Q10,
"/"   "TwsOrd" kCHK_NewSending kCHK_PRI(200) kCHK_QTYS(0,10000,10000) "|LastExgTime=|LastPriTime=",
"-2." "TwsRpt" "|Kind=N" kREQ_P200_Q10 "|BeforeQty=0" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000",
"/"   "TwsOrd" kCHK_ExchangeAccepted kCHK_PRI(200) kCHK_QTYS(10000,10000,10000) "|LastExgTime=09:00:00|LastPriTime=09:00:00",

"+1." "TwsFil" kREQ_SYMB "|Qty=1000|Pri=201|Time=090000.1|MatchKey=1",
"/"   "TwsOrd" kCHK_PartFilled_Cum(1000,201000,"09:00:00.1") kCHK_QTYS(10000,9000,9000),

"+1." "TwsFil" kREQ_SYMB "|Qty=2000|Pri=202|Time=090000.2|MatchKey=2",
"/"   "TwsOrd" kCHK_PartFilled_Cum(3000,605000,"09:00:00.2") kCHK_QTYS(9000,7000,7000),

"*1." "TwsChg" "|Market=T|SessionId=N|BrkId=8610" "|Qty=-3000",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled, kVAL_Sending) kCHK_PRI(200) kCHK_QTYS(7000,7000,7000),
"-2." "TwsRpt" "|Kind=C" kREQ_SYMB "|BeforeQty=7000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.3",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled, kVAL_ExchangeAccepted) kCHK_PRI(200) kCHK_QTYS(7000,4000,4000) "|LastExgTime=09:00:00.3|LastPriTime=09:00:00",

"*1." "TwsChg" "|Market=T|SessionId=N|BrkId=8610" kREQ_PRI(203) "L",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled, kVAL_Sending) kCHK_PRI(200) kCHK_QTYS(4000,4000,4000),
"-2." "TwsRpt" "|Kind=P" kREQ_SYMB kREQ_PRI(203) "L" "|BeforeQty=4000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.4",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(4000,4000,4000) "|LastExgTime=09:00:00.4|LastPriTime=09:00:00.4",

"+1." "TwsFil" kREQ_SYMB "|Qty=4000|Pri=204|Time=090000.5|MatchKey=5",
"/"   "TwsOrd" kCHK_ST(kVAL_FullFilled, kVAL_PartFilled) kCHK_QTYS(4000,0,0) "|CumQty=7000|CumAmt=1421000|LastFilledTime=09:00:00.5",

"*1." "TwsChg" "|Market=T|SessionId=N|BrkId=8610" "|Qty=0",
"/"   "TwsOrd" kCHK_ST(kVAL_FullFilled, kVAL_Sending) kCHK_PRI(203) "L" kCHK_QTYS(0,0,0),
"-2." "TwsRpt" "|Kind=D" kREQ_BASE "|BeforeQty=0|Qty=0" kREQ_ReportSt(kVAL_ExchangeNoLeaves) "|ExgTime=090000.5|ErrCode=20050",
"/"   "TwsOrd" kCHK_ST(kVAL_FullFilled, kVAL_ExchangeNoLeaves) "|ErrCode=20050" kCHK_PRI(203) "L" kCHK_QTYS(0,0,0) "|LastExgTime=09:00:00.5|LastPriTime=09:00:00.4",
   };
   RunTest("Normal-order", cstrNormalTestList, fon9::numofele(cstrNormalTestList));
   utinfo.PrintSplitter();
   //---------------------------------------------
   const char* cstrTestList1[] = {
"*"   "TwsNew" kREQ_P200_Q10,
"/"   "TwsOrd" kCHK_NewSending kCHK_PRI(200) kCHK_QTYS(0,10000,10000),
// 新單尚未回報, 先收到刪改查成交 => 透過 OmsOrderRaw.UpdateOrderSt_ = ReportPending 保留.
"+1." "TwsFil" kREQ_SYMB "|Qty=1000|Pri=201|Time=090000.1|MatchKey=1",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_PartFilled) kCHK_QTYS(10000,10000,10000) "|CumQty=0|CumAmt=0|LastFilledTime=",
"+1." "TwsFil" kREQ_SYMB "|Qty=2000|Pri=202|Time=090000.2|MatchKey=2",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_PartFilled) kCHK_QTYS(10000,10000,10000) "|CumQty=0|CumAmt=0|LastFilledTime=",
// 減1成功.
"*1." "TwsChg" "|Market=T|SessionId=N|BrkId=8610" "|Qty=-1000",
"/"   "TwsOrd" kCHK_ST(kVAL_Sending, kVAL_Sending) kCHK_PRI(200) kCHK_QTYS(10000,10000,10000),
"-2." "TwsRpt" "|Kind=C" kREQ_SYMB kREQ_PRI(200) "|BeforeQty=7000|Qty=6000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.3",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(200) kCHK_QTYS(7000,6000,10000) "|LastExgTime=09:00:00.3|LastPriTime=",
// ** 減2沒回報(Ln#11)
"*1." "TwsChg" "|Market=T|SessionId=N|BrkId=8610" "|Qty=-2000",
"/"   "TwsOrd" kCHK_ST(kVAL_Sending, kVAL_Sending) kCHK_PRI(200) kCHK_QTYS(10000,10000,10000),
// 改價$203成功
"*1." "TwsChg" "|Market=T|SessionId=N|BrkId=8610" kREQ_PRI(203) "L",
"/"   "TwsOrd" kCHK_ST(kVAL_Sending, kVAL_Sending) kCHK_PRI(200) kCHK_QTYS(10000,10000,10000),
"-2." "TwsRpt" "|Kind=P" kREQ_SYMB kREQ_PRI(203) "L" "|BeforeQty=4000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.5",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(4000,4000,10000) "|LastExgTime=09:00:00.5|LastPriTime=09:00:00.5",
// >> 刪單未回報(Ln#17).
"*1." "TwsChg" "|Market=T|SessionId=N|BrkId=8610" "|Qty=0",
"/"   "TwsOrd" kCHK_ST(kVAL_Sending, kVAL_Sending) kCHK_PRI(203) "L" kCHK_QTYS(10000,10000,10000),
// 刪單成功, 但會等「減2成功」後才處理.
"*1." "TwsChg" "|Market=T|SessionId=N|BrkId=8610" "|Qty=0",
"/"   "TwsOrd" kCHK_ST(kVAL_Sending, kVAL_Sending) kCHK_PRI(203) "L" kCHK_QTYS(10000,10000,10000),
"-2." "TwsRpt" "|Kind=D" kREQ_SYMB kREQ_PRI(203) "L" "|BeforeQty=4000|Qty=0" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.6",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(4000,0,10000) "|LastExgTime=09:00:00.6|LastPriTime=09:00:00.5",
// >> 刪單失敗(接續 Ln#17), 等刪單成功後處理.
"-L17. TwsRpt" "|Kind=D" kREQ_BASE "|BeforeQty=0|Qty=0" kREQ_ReportSt(kVAL_ExchangeNoLeaves) "|ExgTime=090000.9|ErrCode=20050",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeNoLeaves) "|ErrCode=20050" kCHK_PRI(203) "L" kCHK_QTYS(0,0,10000) "|LastExgTime=09:00:00.9|LastPriTime=09:00:00.5",
// ** 減2回報成功(接續 Ln#11).
"-L11. TwsRpt" "|Kind=C" kREQ_SYMB kREQ_PRI(200) "|BeforeQty=6000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.4",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(6000,4000,10000) "|LastExgTime=09:00:00.4|LastPriTime=09:00:00.5|ErrCode=0",
// -------------------------------------
// 新單回報(接續 Ln#1) => 處理等候中的回報.
"-L1.TwsRpt" "|Kind=N" kREQ_P200_Q10 "|BeforeQty=0" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000",
"/" "TwsOrd" kCHK_ExchangeAccepted                             kCHK_QTYS(10000,10000,10000) kCHK_PRI(200)  "|LastExgTime=09:00:00|LastPriTime=09:00:00" "|ErrCode=0",
"/" "TwsOrd" kCHK_PartFilled_Cum(1000,201000,"09:00:00.1")     kCHK_QTYS(10000,9000,9000),
"/" "TwsOrd" kCHK_PartFilled_Cum(3000,605000,"09:00:00.2")     kCHK_QTYS(9000,7000,7000),
"/" "TwsOrd" kCHK_ST(kVAL_PartFilled,   kVAL_ExchangeAccepted) kCHK_QTYS(7000,6000,6000) kCHK_PRI(200)     "|LastExgTime=09:00:00.3|LastPriTime=09:00:00",
"/" "TwsOrd" kCHK_ST(kVAL_PartFilled,   kVAL_ExchangeAccepted) kCHK_QTYS(4000,4000,6000) kCHK_PRI(203) "L" "|LastExgTime=09:00:00.5|LastPriTime=09:00:00.5",
"/" "TwsOrd" kCHK_ST(kVAL_PartFilled,   kVAL_ExchangeAccepted) kCHK_QTYS(6000,4000,4000) kCHK_PRI(203) "L" "|LastExgTime=09:00:00.4|LastPriTime=09:00:00.5",
"/" "TwsOrd" kCHK_ST(kVAL_UserCanceled, kVAL_ExchangeAccepted) kCHK_QTYS(4000,0,0)       kCHK_PRI(203) "L" "|LastExgTime=09:00:00.6|LastPriTime=09:00:00.5" "|ErrCode=0",
"/" "TwsOrd" kCHK_ST(kVAL_UserCanceled, kVAL_ExchangeNoLeaves) kCHK_QTYS(0,0,0)          kCHK_PRI(203) "L" "|LastExgTime=09:00:00.9|LastPriTime=09:00:00.5" "|ErrCode=20050",
   };
   RunTest("Out-of-order", cstrTestList1, fon9::numofele(cstrTestList1));
   utinfo.PrintSplitter();
   //---------------------------------------------
   // Query, ChgPri 回報, 有 ReportPending 的需求嗎?
   // ChgPri => 需要 => 如果 Client 端使用 FIX 下單, 則必須依序處理下單要求及回報.
   // Query => 不需要, 因應異常處理(例:新單回報遺漏), 透過查單確認結果後, 手動補回報.
   //       => 查單不改變 Order 的數量(但可能改變 Order.LastPri);
   //       => 所以不會改變 OrderSt;
   //       => 只要在 Message 顯示當下的結果就足夠了.
   //---------------------------------------------
   if (0); // 測試直接 TwsRpt, 沒有 TwsNew、TwsChg;
}
