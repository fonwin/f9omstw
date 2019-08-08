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
const fon9::fmkt::TradingRxItem* RunReport(TestCore& core,
                                           const fon9::fmkt::TradingRxItem* ref,
                                           fon9::StrView rptstr) {
   auto runner = MakeOmsReportRunner(core.Owner_->RequestFactoryPark(), rptstr, f9fmkt_RxKind_Filled);
   AssignKeyFromRef(*runner.Request_, ref);
   auto retval = runner.Request_.get();
   core.MoveToCore(std::move(runner));
   {  // 測試重複回報: 應拋棄, 不會寫入 history.
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
   std::string                      rptstrApp;
   for (size_t lineNo = 1; lineNo <= count; ++lineNo) {
      fon9::StrView  rptstr = fon9::StrView_cstr(*cstrTestList++);
      // [0]: '+'=Report占用序號, '-'=Report不占用序號, '*'=Request, '/'=OrderRaw
      int chHead = fon9::StrTrim(&rptstr).Get1st();
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
      case '>':
         // ">AllocOrdNo=A"
         fon9::StrView tag, value;
         fon9::StrFetchTagValue(rptstr, tag, value);
         if (tag == "AllocOrdNo") {
            auto brk = core->GetResource().Brks_->GetBrkRec("8610");
            auto ordnoMap = brk->GetMarket(f9fmkt_TradingMarket_TwSEC)
               .GetSession(f9fmkt_TradingSessionId_Normal)
               .GetOrdNoMap();
            f9omstw::OmsOrdNo  ordno;
            ordnoMap->GetNextOrdNo(value, ordno);
            rptstrApp = "|OrdNo=";
            rptstrApp.append(ordno.begin(), ordno.end());
         }
         continue;
      }
      if (rxItem->RxSNO() != snoCurr) {
         std::cout << "|err=Unexpected SNO:" << rxItem->RxSNO()
            << "|expected=" << snoCurr;
         AbortAtLine(lineNo);
      }
      snoVect[lineNo - 1] = snoCurr;
      ++snoCurr;
   }
}
//--------------------------------------------------------------------------//
int      gArgc;
char**   gArgv;
bool     gIsTestReload = false;
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
void RunTest(TestCoreSP& core, const char* testName, const char** cstrTestList, size_t count) {
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

#define kCHK_PartFilled_CUM(qty,amt,tmstr)   kCHK_PartFilled kCHK_CUM(qty,amt,tmstr)
#define kCHK_CUM(qty,amt,tmstr)         \
         "|CumQty=" fon9_CTXTOCSTR(qty) \
         "|CumAmt=" fon9_CTXTOCSTR(amt) \
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
// 一般正常順序下單回報.
void TestNormalOrder(TestCoreSP& core) {
   #define kREQ_P200_Q10   kREQ_SYMB kREQ_PRI(200) "|Qty=10000"
   const char* cstrTestList[] = {
"*"   "TwsNew" kREQ_P200_Q10,
"/"   "TwsOrd" kCHK_NewSending kCHK_PRI(200) kCHK_QTYS(0,10000,10000) "|LastExgTime=|LastPriTime=",
"-2." "TwsRpt" "|Kind=N" kREQ_P200_Q10 "|BeforeQty=10000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000",
"/"   "TwsOrd" kCHK_ExchangeAccepted kCHK_PRI(200) kCHK_QTYS(10000,10000,10000) "|LastExgTime=09:00:00|LastPriTime=09:00:00",
// 成交.
"+1." "TwsFil" kREQ_SYMB "|Qty=1000|Pri=201|Time=090000.1|MatchKey=1",
"/"   "TwsOrd" kCHK_PartFilled_CUM(1000,201000,"09:00:00.1") kCHK_QTYS(10000,9000,9000),
"+1." "TwsFil" kREQ_SYMB "|Qty=2000|Pri=202|Time=090000.2|MatchKey=2",
"/"   "TwsOrd" kCHK_PartFilled_CUM(3000,605000,"09:00:00.2") kCHK_QTYS(9000,7000,7000),
// 減3
"*1." "TwsChg" "|Market=T|SessionId=N|BrkId=8610" "|Qty=-3000",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled, kVAL_Sending) kCHK_PRI(200) kCHK_QTYS(7000,7000,7000),
"-2." "TwsRpt" "|Kind=C" kREQ_SYMB "|BeforeQty=7000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.3",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled, kVAL_ExchangeAccepted) kCHK_PRI(200) kCHK_QTYS(7000,4000,4000) "|LastExgTime=09:00:00.3|LastPriTime=09:00:00",
// 改價$203
"*1." "TwsChg" "|Market=T|SessionId=N|BrkId=8610" kREQ_PRI(203) "L",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled, kVAL_Sending) kCHK_PRI(200) kCHK_QTYS(4000,4000,4000),
"-2." "TwsRpt" "|Kind=P" kREQ_SYMB kREQ_PRI(203) "L" "|BeforeQty=4000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.4",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(4000,4000,4000) "|LastExgTime=09:00:00.4|LastPriTime=09:00:00.4",
// 成交4
"+1." "TwsFil" kREQ_SYMB "|Qty=4000|Pri=204|Time=090000.5|MatchKey=5",
"/"   "TwsOrd" kCHK_ST(kVAL_FullFilled, kVAL_PartFilled) kCHK_QTYS(4000,0,0) kCHK_CUM(7000,1421000,"09:00:00.5"),
// 刪單
"*1." "TwsChg" "|Market=T|SessionId=N|BrkId=8610" "|Qty=0",
"/"   "TwsOrd" kCHK_ST(kVAL_FullFilled, kVAL_Sending) kCHK_PRI(203) "L" kCHK_QTYS(0,0,0),
"-2." "TwsRpt" "|Kind=D" kREQ_BASE "|BeforeQty=0|Qty=0" kREQ_ReportSt(kVAL_ExchangeNoLeaves) "|ExgTime=090000.5|ErrCode=20050",
"/"   "TwsOrd" kCHK_ST(kVAL_FullFilled, kVAL_ExchangeNoLeaves) "|ErrCode=20050" kCHK_PRI(203) "L" kCHK_QTYS(0,0,0) "|LastExgTime=09:00:00.5|LastPriTime=09:00:00.4",
   };
   RunTest(core, "Normal-order", cstrTestList, fon9::numofele(cstrTestList));
}
// 下單後,亂序回報.
void TestOutofOrder(TestCoreSP& core) {
   const char* cstrTestList[] = {
"*"   "TwsNew" kREQ_P200_Q10,
"/"   "TwsOrd" kCHK_NewSending kCHK_PRI(200) kCHK_QTYS(0,10000,10000),
// 新單尚未回報, 先收到刪改查成交 => 透過 OmsOrderRaw.UpdateOrderSt_ = ReportPending 保留.
"+1." "TwsFil" kREQ_SYMB "|Qty=1000|Pri=201|Time=090000.1|MatchKey=1",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_PartFilled) kCHK_QTYS(10000,10000,10000) kCHK_CUM(0,0,""),
"+1." "TwsFil" kREQ_SYMB "|Qty=2000|Pri=202|Time=090000.2|MatchKey=2",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_PartFilled) kCHK_QTYS(10000,10000,10000) kCHK_CUM(0,0,""),
// 減1成功.
"*1." "TwsChg" "|Market=T|SessionId=N|BrkId=8610" "|Qty=-1000",
"/"   "TwsOrd" kCHK_ST(kVAL_Sending, kVAL_Sending) kCHK_PRI(200) kCHK_QTYS(10000,10000,10000),
"-2." "TwsRpt" "|Kind=C" kREQ_SYMB kREQ_PRI(200) "|BeforeQty=7000|Qty=6000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.3",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(200) kCHK_QTYS(7000,6000,10000) "|LastExgTime=09:00:00.3|LastPriTime=09:00:00.3",
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
"-L1.TwsRpt" "|Kind=N" kREQ_P200_Q10 "|BeforeQty=10000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000",
"/" "TwsOrd" kCHK_ExchangeAccepted                             kCHK_QTYS(10000,10000,10000) kCHK_PRI(200)     "|LastExgTime=09:00:00|LastPriTime=09:00:00" "|ErrCode=0",
"/" "TwsOrd" kCHK_PartFilled_CUM(1000,201000,"09:00:00.1")     kCHK_QTYS(10000,9000,9000),
"/" "TwsOrd" kCHK_PartFilled_CUM(3000,605000,"09:00:00.2")     kCHK_QTYS(9000,7000,7000),
"/" "TwsOrd" kCHK_ST(kVAL_PartFilled,   kVAL_ExchangeAccepted) kCHK_QTYS(7000,6000,6000)    kCHK_PRI(200)     "|LastExgTime=09:00:00.3|LastPriTime=09:00:00",
"/" "TwsOrd" kCHK_ST(kVAL_PartFilled,   kVAL_ExchangeAccepted) kCHK_QTYS(4000,4000,6000)    kCHK_PRI(203) "L" "|LastExgTime=09:00:00.5|LastPriTime=09:00:00.5",
"/" "TwsOrd" kCHK_ST(kVAL_PartFilled,   kVAL_ExchangeAccepted) kCHK_QTYS(6000,4000,4000)    kCHK_PRI(203) "L" "|LastExgTime=09:00:00.4|LastPriTime=09:00:00.5",
"/" "TwsOrd" kCHK_ST(kVAL_UserCanceled, kVAL_ExchangeAccepted) kCHK_QTYS(4000,0,0)          kCHK_PRI(203) "L" "|LastExgTime=09:00:00.6|LastPriTime=09:00:00.5" "|ErrCode=0",
"/" "TwsOrd" kCHK_ST(kVAL_UserCanceled, kVAL_ExchangeNoLeaves) kCHK_QTYS(0,0,0)             kCHK_PRI(203) "L" "|LastExgTime=09:00:00.9|LastPriTime=09:00:00.5" "|ErrCode=20050",
   };
   RunTest(core, "Out-of-order", cstrTestList, fon9::numofele(cstrTestList));
}
// 委託不存在(尚未收到新單回報), 先收到成交及其他回報.
void TestNoNewFilled(TestCoreSP& core) {
   const char* cstrTestList[] = {
// 先收到刪改查成交 => 透過 OmsOrderRaw.UpdateOrderSt_ = ReportPending 保留.
">AllocOrdNo=A",
"+"   "TwsFil" kREQ_SYMB "|Qty=1000|Pri=201|Time=090000.1|MatchKey=1",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_PartFilled) kCHK_QTYS(0,0,0) kCHK_CUM(0,0,""),
"+1." "TwsFil" kREQ_SYMB "|Qty=2000|Pri=202|Time=090000.2|MatchKey=2",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_PartFilled) kCHK_QTYS(0,0,0) kCHK_CUM(0,0,""),
// 減1成功.
"+1." "TwsRpt" "|Kind=C" kREQ_SYMB kREQ_ReportSt(kVAL_ExchangeAccepted) kREQ_PRI(200) "|BeforeQty=7000|Qty=6000|ExgTime=090000.3",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(200) kCHK_QTYS(7000,6000,0) "|LastExgTime=09:00:00.3|LastPriTime=09:00:00.3",
// 改價203成功,但「遺漏2」
"+1." "TwsRpt" "|Kind=P" kREQ_SYMB kREQ_PRI(203) "L" "|BeforeQty=4000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.5",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(4000,4000,0) "|LastExgTime=09:00:00.5|LastPriTime=09:00:00.5",
// 刪單失敗, 等「LeavesQty==0」後處理.
"+1." "TwsRpt" "|Kind=D" kREQ_BASE "|BeforeQty=0|Qty=0" kREQ_ReportSt(kVAL_ExchangeNoLeaves) "|ExgTime=090000.9|ErrCode=20050",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeNoLeaves) kCHK_PRI(203) "L" kCHK_QTYS(0,0,0) "|LastExgTime=09:00:00.9|LastPriTime=09:00:00.5|ErrCode=20050",
// 刪單成功, 但會等「遺漏2補齊」後才處理.
"+1." "TwsRpt" "|Kind=D" kREQ_SYMB kREQ_PRI(203) "L" "|BeforeQty=4000|Qty=0" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.6",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(4000,0,0) "|LastExgTime=09:00:00.6|LastPriTime=09:00:00.5|ErrCode=0",
// 減2回報成功.
"+1." "TwsRpt" "|Kind=C" kREQ_SYMB kREQ_PRI(200) "|BeforeQty=6000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.4",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(6000,4000,0) "|LastExgTime=09:00:00.4|LastPriTime=09:00:00.5",
// -------------------------------------
// 新單回報
"+1." "TwsRpt" "|Kind=N" kREQ_SYMB kREQ_PRI(200) "|BeforeQty=10000|Qty=10000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000",
"/"   "TwsOrd" kCHK_ExchangeAccepted                             kCHK_QTYS(0,10000,10000)  kCHK_PRI(200) "|LastExgTime=09:00:00|LastPriTime=09:00:00",
"/"   "TwsOrd" kCHK_PartFilled_CUM(1000,201000,"09:00:00.1")     kCHK_QTYS(10000,9000,9000),
"/"   "TwsOrd" kCHK_PartFilled_CUM(3000,605000,"09:00:00.2")     kCHK_QTYS(9000,7000,7000),
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled,   kVAL_ExchangeAccepted) kCHK_QTYS(7000,6000,6000) kCHK_PRI(200)     "|LastExgTime=09:00:00.3|LastPriTime=09:00:00",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled,   kVAL_ExchangeAccepted) kCHK_QTYS(4000,4000,6000) kCHK_PRI(203) "L" "|LastExgTime=09:00:00.5|LastPriTime=09:00:00.5",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled,   kVAL_ExchangeAccepted) kCHK_QTYS(6000,4000,4000) kCHK_PRI(203) "L" "|LastExgTime=09:00:00.4|LastPriTime=09:00:00.5",
"/"   "TwsOrd" kCHK_ST(kVAL_UserCanceled, kVAL_ExchangeAccepted) kCHK_QTYS(4000,0,0)       kCHK_PRI(203) "L" "|LastExgTime=09:00:00.6|LastPriTime=09:00:00.5" "|ErrCode=0",
"/"   "TwsOrd" kCHK_ST(kVAL_UserCanceled, kVAL_ExchangeNoLeaves) kCHK_QTYS(0,0,0)          kCHK_PRI(203) "L" "|LastExgTime=09:00:00.9|LastPriTime=09:00:00.5" "|ErrCode=20050",
   };
   RunTest(core, "Out-of-order(NoNewFilled)", cstrTestList, fon9::numofele(cstrTestList));
}
// 委託不存在(尚未收到新單回報), 先收到ChgQty及其他回報.
void TestNoNewChgQty(TestCoreSP& core) {
   const char* cstrTestList[] = {
">AllocOrdNo=A",
// 減1成功.
"+"   "TwsRpt" "|Kind=C" kREQ_SYMB kREQ_ReportSt(kVAL_ExchangeAccepted) kREQ_PRI(200) "|BeforeQty=7000|Qty=6000|ExgTime=090000.3",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(200) kCHK_QTYS(7000,6000,0) "|LastExgTime=09:00:00.3|LastPriTime=09:00:00.3",
// 成交.
"+1"  "TwsFil" kREQ_SYMB "|Qty=1000|Pri=201|Time=090000.1|MatchKey=1",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_PartFilled) kCHK_QTYS(0,0,0) kCHK_CUM(0,0,""),
"+1." "TwsFil" kREQ_SYMB "|Qty=2000|Pri=202|Time=090000.2|MatchKey=2",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_PartFilled) kCHK_QTYS(0,0,0) kCHK_CUM(0,0,""),
// 改價203成功,但「遺漏2」
"+1." "TwsRpt" "|Kind=P" kREQ_SYMB kREQ_PRI(203) "L" "|BeforeQty=4000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.5",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(4000,4000,0) "|LastExgTime=09:00:00.5|LastPriTime=09:00:00.5",
// 刪單失敗, 等「LeavesQty==0」後處理.
"+1." "TwsRpt" "|Kind=D" kREQ_BASE "|BeforeQty=0|Qty=0" kREQ_ReportSt(kVAL_ExchangeNoLeaves) "|ExgTime=090000.9|ErrCode=20050",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeNoLeaves) kCHK_PRI(203) "L" kCHK_QTYS(0,0,0) "|LastExgTime=09:00:00.9|LastPriTime=09:00:00.5|ErrCode=20050",
// 刪單成功, 但會等「遺漏2補齊」後才處理.
"+1." "TwsRpt" "|Kind=D" kREQ_SYMB kREQ_PRI(203) "L" "|BeforeQty=4000|Qty=0" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.6",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(4000,0,0) "|LastExgTime=09:00:00.6|LastPriTime=09:00:00.5|ErrCode=0",
// 減2回報成功.
"+1." "TwsRpt" "|Kind=C" kREQ_SYMB kREQ_PRI(200) "|BeforeQty=6000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.4",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(6000,4000,0) "|LastExgTime=09:00:00.4|LastPriTime=09:00:00.5",
// -------------------------------------
// 新單回報
"+1." "TwsRpt" "|Kind=N" kREQ_SYMB kREQ_PRI(200) "|BeforeQty=10000|Qty=10000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000",
"/"   "TwsOrd" kCHK_ExchangeAccepted                             kCHK_QTYS(0,10000,10000)  kCHK_PRI(200) "|LastExgTime=09:00:00|LastPriTime=09:00:00",
"/"   "TwsOrd" kCHK_ExchangeAccepted                             kCHK_QTYS(7000,6000,9000) kCHK_PRI(200) "|LastExgTime=09:00:00.3|LastPriTime=09:00:00",
"/"   "TwsOrd" kCHK_PartFilled_CUM(1000,201000,"09:00:00.1")     kCHK_QTYS(9000,8000,8000),
"/"   "TwsOrd" kCHK_PartFilled_CUM(3000,605000,"09:00:00.2")     kCHK_QTYS(8000,6000,6000),
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled,   kVAL_ExchangeAccepted) kCHK_QTYS(4000,4000,6000) kCHK_PRI(203) "L" "|LastExgTime=09:00:00.5|LastPriTime=09:00:00.5",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled,   kVAL_ExchangeAccepted) kCHK_QTYS(6000,4000,4000) kCHK_PRI(203) "L" "|LastExgTime=09:00:00.4|LastPriTime=09:00:00.5",
"/"   "TwsOrd" kCHK_ST(kVAL_UserCanceled, kVAL_ExchangeAccepted) kCHK_QTYS(4000,0,0)       kCHK_PRI(203) "L" "|LastExgTime=09:00:00.6|LastPriTime=09:00:00.5" "|ErrCode=0",
"/"   "TwsOrd" kCHK_ST(kVAL_UserCanceled, kVAL_ExchangeNoLeaves) kCHK_QTYS(0,0,0)          kCHK_PRI(203) "L" "|LastExgTime=09:00:00.9|LastPriTime=09:00:00.5" "|ErrCode=20050",
   };
   RunTest(core, "Out-of-order(NoNewChgQty)", cstrTestList, fon9::numofele(cstrTestList));
}
// 委託不存在(尚未收到新單回報), 先收到Delete及其他回報.
void TestNoNewDelete(TestCoreSP& core) {
   const char* cstrTestList[] = {
">AllocOrdNo=A",
// 刪單成功, 但會等「LeavesQty=BeforeQty=4」才處理.
"+"   "TwsRpt" "|Kind=D" kREQ_SYMB kREQ_PRI(203) "L" "|BeforeQty=4000|Qty=0" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.6",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(4000,0,0) "|LastExgTime=09:00:00.6|LastPriTime=09:00:00.6",
// 減1成功.
"+1"  "TwsRpt" "|Kind=C" kREQ_SYMB kREQ_ReportSt(kVAL_ExchangeAccepted) kREQ_PRI(200) "|BeforeQty=7000|Qty=6000|ExgTime=090000.3",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(7000,6000,0) "|LastExgTime=09:00:00.3|LastPriTime=09:00:00.6",
// 成交.
"+1"  "TwsFil" kREQ_SYMB "|Qty=1000|Pri=201|Time=090000.1|MatchKey=1",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_PartFilled) kCHK_QTYS(0,0,0) kCHK_CUM(0,0,""),
"+1." "TwsFil" kREQ_SYMB "|Qty=2000|Pri=202|Time=090000.2|MatchKey=2",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_PartFilled) kCHK_QTYS(0,0,0) kCHK_CUM(0,0,""),
// 改價203成功.
"+1." "TwsRpt" "|Kind=P" kREQ_SYMB kREQ_PRI(203) "L" "|BeforeQty=4000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.5",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(4000,4000,0) "|LastExgTime=09:00:00.5|LastPriTime=09:00:00.6",
// 刪單失敗, 等「LeavesQty==0」後處理.
"+1." "TwsRpt" "|Kind=D" kREQ_BASE "|BeforeQty=0|Qty=0" kREQ_ReportSt(kVAL_ExchangeNoLeaves) "|ExgTime=090000.9|ErrCode=20050",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeNoLeaves) kCHK_PRI(203) "L" kCHK_QTYS(0,0,0) "|LastExgTime=09:00:00.9|LastPriTime=09:00:00.6|ErrCode=20050",
// 減2回報成功.
"+1." "TwsRpt" "|Kind=C" kREQ_SYMB kREQ_PRI(200) "|BeforeQty=6000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.4",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(6000,4000,0) "|LastExgTime=09:00:00.4|LastPriTime=09:00:00.6",
// -------------------------------------
// 新單回報
"+1." "TwsRpt" "|Kind=N" kREQ_SYMB kREQ_PRI(200) "|BeforeQty=10000|Qty=10000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000",
"/"   "TwsOrd" kCHK_ExchangeAccepted                             kCHK_QTYS(0,10000,10000)  kCHK_PRI(200)     "|LastExgTime=09:00:00|LastPriTime=09:00:00",
"/"   "TwsOrd" kCHK_ExchangeAccepted                             kCHK_QTYS(7000,6000,9000) kCHK_PRI(203) "L" "|LastExgTime=09:00:00.3|LastPriTime=09:00:00.6",
"/"   "TwsOrd" kCHK_PartFilled_CUM(1000,201000,"09:00:00.1")     kCHK_QTYS(9000,8000,8000),
"/"   "TwsOrd" kCHK_PartFilled_CUM(3000,605000,"09:00:00.2")     kCHK_QTYS(8000,6000,6000),
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled,   kVAL_ExchangeAccepted) kCHK_QTYS(4000,4000,6000) kCHK_PRI(203) "L" "|LastExgTime=09:00:00.5|LastPriTime=09:00:00.6",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled,   kVAL_ExchangeAccepted) kCHK_QTYS(6000,4000,4000) kCHK_PRI(203) "L" "|LastExgTime=09:00:00.4|LastPriTime=09:00:00.6",
"/"   "TwsOrd" kCHK_ST(kVAL_UserCanceled, kVAL_ExchangeAccepted) kCHK_QTYS(4000,0,0)       kCHK_PRI(203) "L" "|LastExgTime=09:00:00.6|LastPriTime=09:00:00.6" "|ErrCode=0",
"/"   "TwsOrd" kCHK_ST(kVAL_UserCanceled, kVAL_ExchangeNoLeaves) kCHK_QTYS(0,0,0)          kCHK_PRI(203) "L" "|LastExgTime=09:00:00.9|LastPriTime=09:00:00.6" "|ErrCode=20050",
   };
   RunTest(core, "Out-of-order(NoNewDelete)", cstrTestList, fon9::numofele(cstrTestList));
}
// 委託不存在(尚未收到新單回報), 先收到Query及其他回報.
void TestNoNewQuery(TestCoreSP& core) {
   const char* cstrTestList[] = {
">AllocOrdNo=A",
"+"   "TwsRpt" "|Kind=Q" kREQ_SYMB kREQ_PRI(203) "L" "|BeforeQty=4000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.8",
"/"   "TwsOrd" kCHK_ST(0, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(4000,4000,0) "|LastExgTime=09:00:00.8|LastPriTime=09:00:00.8",
// 刪單成功, 但會等「LeavesQty=BeforeQty=4」才處理.
"+1"  "TwsRpt" "|Kind=D" kREQ_SYMB kREQ_PRI(203) "L" "|BeforeQty=4000|Qty=0" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.6",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(4000,0,0) "|LastExgTime=09:00:00.6|LastPriTime=09:00:00.8",
// 減1成功.
"+1"  "TwsRpt" "|Kind=C" kREQ_SYMB kREQ_ReportSt(kVAL_ExchangeAccepted) kREQ_PRI(200) "|BeforeQty=7000|Qty=6000|ExgTime=090000.3",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(7000,6000,0) "|LastExgTime=09:00:00.3|LastPriTime=09:00:00.8",
// 成交.
"+1"  "TwsFil" kREQ_SYMB "|Qty=1000|Pri=201|Time=090000.1|MatchKey=1",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_PartFilled) kCHK_QTYS(0,0,0) kCHK_CUM(0,0,""),
"+1." "TwsFil" kREQ_SYMB "|Qty=2000|Pri=202|Time=090000.2|MatchKey=2",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_PartFilled) kCHK_QTYS(0,0,0) kCHK_CUM(0,0,""),
// 改價203成功.
"+1." "TwsRpt" "|Kind=P" kREQ_SYMB kREQ_PRI(203) "L" "|BeforeQty=4000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.5",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(4000,4000,0) "|LastExgTime=09:00:00.5|LastPriTime=09:00:00.8",
// 刪單失敗, 等「LeavesQty==0」後處理.
"+1." "TwsRpt" "|Kind=D" kREQ_BASE "|BeforeQty=0|Qty=0" kREQ_ReportSt(kVAL_ExchangeNoLeaves) "|ExgTime=090000.9|ErrCode=20050",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeNoLeaves) kCHK_PRI(203) "L" kCHK_QTYS(0,0,0) "|LastExgTime=09:00:00.9|LastPriTime=09:00:00.8|ErrCode=20050",
// 減2回報成功.
"+1." "TwsRpt" "|Kind=C" kREQ_SYMB kREQ_PRI(200) "|BeforeQty=6000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.4",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(6000,4000,0) "|LastExgTime=09:00:00.4|LastPriTime=09:00:00.8",
// -------------------------------------
// 新單回報
"+1." "TwsRpt" "|Kind=N" kREQ_SYMB kREQ_PRI(200) "|BeforeQty=10000|Qty=10000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000",
"/"   "TwsOrd" kCHK_ExchangeAccepted                             kCHK_QTYS(0,10000,10000)  kCHK_PRI(200)     "|LastExgTime=09:00:00|LastPriTime=09:00:00",
"/"   "TwsOrd" kCHK_ExchangeAccepted                             kCHK_QTYS(7000,6000,9000) kCHK_PRI(203) "L" "|LastExgTime=09:00:00.3|LastPriTime=09:00:00.8",
"/"   "TwsOrd" kCHK_PartFilled_CUM(1000,201000,"09:00:00.1")     kCHK_QTYS(9000,8000,8000),
"/"   "TwsOrd" kCHK_PartFilled_CUM(3000,605000,"09:00:00.2")     kCHK_QTYS(8000,6000,6000),
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled,   kVAL_ExchangeAccepted) kCHK_QTYS(4000,4000,6000) kCHK_PRI(203) "L" "|LastExgTime=09:00:00.5|LastPriTime=09:00:00.8",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled,   kVAL_ExchangeAccepted) kCHK_QTYS(6000,4000,4000) kCHK_PRI(203) "L" "|LastExgTime=09:00:00.4|LastPriTime=09:00:00.8",
"/"   "TwsOrd" kCHK_ST(kVAL_UserCanceled, kVAL_ExchangeAccepted) kCHK_QTYS(4000,0,0)       kCHK_PRI(203) "L" "|LastExgTime=09:00:00.6|LastPriTime=09:00:00.8" "|ErrCode=0",
"/"   "TwsOrd" kCHK_ST(kVAL_UserCanceled, kVAL_ExchangeNoLeaves) kCHK_QTYS(0,0,0)          kCHK_PRI(203) "L" "|LastExgTime=09:00:00.9|LastPriTime=09:00:00.8" "|ErrCode=20050",
   };
   RunTest(core, "Out-of-order(NoNewQuery)", cstrTestList, fon9::numofele(cstrTestList));
}
// 有收到新單回報, 及後續回報.
void TestNormalReport(TestCoreSP& core) {
   const char* cstrTestList[] = {
">AllocOrdNo=A",
"+"   "TwsRpt" "|Kind=Q" kREQ_SYMB kREQ_PRI(203) "L" "|BeforeQty=4000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.8",
"/"   "TwsOrd" kCHK_ST(0, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(4000,4000,0) "|LastExgTime=09:00:00.8|LastPriTime=09:00:00.8",
// 減1成功.
"+1"  "TwsRpt" "|Kind=C" kREQ_SYMB kREQ_ReportSt(kVAL_ExchangeAccepted) kREQ_PRI(200) "|BeforeQty=7000|Qty=6000|ExgTime=090000.3",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(7000,6000,0) "|LastExgTime=09:00:00.3|LastPriTime=09:00:00.8",
// 成交.
"+1"  "TwsFil" kREQ_SYMB "|Qty=1000|Pri=201|Time=090000.1|MatchKey=1",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_PartFilled) kCHK_QTYS(0,0,0) kCHK_CUM(0,0,""),
"+1." "TwsFil" kREQ_SYMB "|Qty=2000|Pri=202|Time=090000.2|MatchKey=2",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_PartFilled) kCHK_QTYS(0,0,0) kCHK_CUM(0,0,""),
// 改價203成功.
"+1." "TwsRpt" "|Kind=P" kREQ_SYMB kREQ_PRI(203) "L" "|BeforeQty=4000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.5",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_PRI(203) "L" kCHK_QTYS(4000,4000,0) "|LastExgTime=09:00:00.5|LastPriTime=09:00:00.8",
// -------------------------------------
// 新單回報
"+1." "TwsRpt" "|Kind=N" kREQ_SYMB kREQ_PRI(200) "|BeforeQty=10000|Qty=10000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000",
"/"   "TwsOrd" kCHK_ExchangeAccepted                           kCHK_QTYS(0,10000,10000)  kCHK_PRI(200)     "|LastExgTime=09:00:00|LastPriTime=09:00:00",
"/"   "TwsOrd" kCHK_ExchangeAccepted                           kCHK_QTYS(7000,6000,9000) kCHK_PRI(203) "L" "|LastExgTime=09:00:00.3|LastPriTime=09:00:00.8",
"/"   "TwsOrd" kCHK_PartFilled_CUM(1000,201000,"09:00:00.1")   kCHK_QTYS(9000,8000,8000),
"/"   "TwsOrd" kCHK_PartFilled_CUM(3000,605000,"09:00:00.2")   kCHK_QTYS(8000,6000,6000),
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled, kVAL_ExchangeAccepted) kCHK_QTYS(4000,4000,6000) kCHK_PRI(203) "L" "|LastExgTime=09:00:00.5|LastPriTime=09:00:00.8",
// -------------------------------------
// 減1回報成功.
"+1." "TwsRpt" "|Kind=C" kREQ_SYMB kREQ_PRI(204) "|BeforeQty=4000|Qty=3000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090002",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled, kVAL_ExchangeAccepted) kCHK_QTYS(4000,3000,5000) kCHK_PRI(204)     "|LastExgTime=09:00:02|LastPriTime=09:00:02",
// -------------------------------------
// 減2回報成功(補單).
"+1." "TwsRpt" "|Kind=C" kREQ_SYMB kREQ_PRI(203) "|BeforeQty=6000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.4",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled, kVAL_ExchangeAccepted) kCHK_QTYS(6000,4000,3000) kCHK_PRI(204)     "|LastExgTime=09:00:00.4|LastPriTime=09:00:02",
// -------------------------------------
// 改價204成功(補單).
"+1." "TwsRpt" "|Kind=P" kREQ_SYMB kREQ_PRI(204) "|BeforeQty=4000|Qty=4000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090001",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportStale,kVAL_ExchangeAccepted) kCHK_QTYS(4000,4000,3000) kCHK_PRI(204)    "|LastExgTime=09:00:01|LastPriTime=09:00:02",
// 改價205成功.
"+1." "TwsRpt" "|Kind=P" kREQ_SYMB kREQ_PRI(205) "|BeforeQty=3000|Qty=3000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090003",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled, kVAL_ExchangeAccepted) kCHK_QTYS(3000,3000,3000) kCHK_PRI(205)    "|LastExgTime=09:00:03|LastPriTime=09:00:03",
// -------------------------------------
// 刪單失敗, 等「LeavesQty==0」後處理.
"+1." "TwsRpt" "|Kind=D" kREQ_BASE "|BeforeQty=0|Qty=0" kREQ_ReportSt(kVAL_ExchangeNoLeaves) "|ExgTime=090005|ErrCode=20050",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeNoLeaves) kCHK_QTYS(0,0,3000)    kCHK_PRI(205) "|LastExgTime=09:00:05|LastPriTime=09:00:03|ErrCode=20050",
// 刪單成功, 但會等「LeavesQty=BeforeQty=2」才處理.
"+1"  "TwsRpt" "|Kind=D" kREQ_SYMB kREQ_PRI(205) "|BeforeQty=2000|Qty=0" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090004",
"/"   "TwsOrd" kCHK_ST(kVAL_ReportPending, kVAL_ExchangeAccepted) kCHK_QTYS(2000,0,3000) kCHK_PRI(205) "|LastExgTime=09:00:04|LastPriTime=09:00:03",
// -------------------------------------
// 減1回報成功(補單).
"+1." "TwsRpt" "|Kind=C" kREQ_SYMB kREQ_PRI(203) "|BeforeQty=6000|Qty=5000" kREQ_ReportSt(kVAL_ExchangeAccepted) "|ExgTime=090000.4",
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled, kVAL_ExchangeAccepted) kCHK_QTYS(6000,5000,2000) kCHK_PRI(205) "|LastExgTime=09:00:00.4|LastPriTime=09:00:03",
"/"   "TwsOrd" kCHK_ST(kVAL_UserCanceled, kVAL_ExchangeAccepted) kCHK_QTYS(2000,0,0)     kCHK_PRI(205) "|LastExgTime=09:00:04|LastPriTime=09:00:03" "|ErrCode=0",
"/"   "TwsOrd" kCHK_ST(kVAL_UserCanceled, kVAL_ExchangeNoLeaves) kCHK_QTYS(0,0,0)        kCHK_PRI(205) "|LastExgTime=09:00:05|LastPriTime=09:00:03" "|ErrCode=20050",
   };
   RunTest(core, "Out-of-order(TestNormalReport)", cstrTestList, fon9::numofele(cstrTestList));
}
// 先收到成交, 後收到減量回報(造成 LeavesQty=0).
void TestPartCancel(TestCoreSP& core) {
   const char* cstrTestList[] = {
">AllocOrdNo=A",
// 新單回報.
"+"   "TwsRpt" "|Kind=N" kREQ_ReportSt(kVAL_ExchangeAccepted) kREQ_PRI(200) "|BeforeQty=10000|Qty=10000" "|ExgTime=090000" kREQ_SYMB,
"/"   "TwsOrd" kCHK_ExchangeAccepted                          kCHK_PRI(200) kCHK_QTYS(0,10000,10000) "|LastExgTime=09:00:00|LastPriTime=09:00:00",
// 成交.
"+1"  "TwsFil" kREQ_SYMB "|Qty=1000|Pri=201|Time=090000.5|MatchKey=1",
"/"   "TwsOrd" kCHK_PartFilled_CUM(1000,201000,"09:00:00.5")   kCHK_QTYS(10000,9000,9000),
"+1." "TwsFil" kREQ_SYMB "|Qty=2000|Pri=202|Time=090000.6|MatchKey=2",
"/"   "TwsOrd" kCHK_PartFilled_CUM(3000,605000,"09:00:00.6")   kCHK_QTYS(9000,7000,7000),
// 減3(補單).
"+1." "TwsRpt" "|Kind=C" kREQ_ReportSt(kVAL_ExchangeAccepted)    kREQ_PRI(200) "|BeforeQty=10000|Qty=7000" "|ExgTime=090000.1" kREQ_SYMB,
"/"   "TwsOrd" kCHK_ST(kVAL_PartFilled, kVAL_ExchangeAccepted)   kCHK_PRI(200) kCHK_QTYS(10000,7000,4000) "|LastExgTime=09:00:00.1|LastPriTime=09:00:00",
// 減4(補單).
"+1." "TwsRpt" "|Kind=C" kREQ_ReportSt(kVAL_ExchangeAccepted)    kREQ_PRI(200) "|BeforeQty=7000|Qty=3000" "|ExgTime=090000.2" kREQ_SYMB,
"/"   "TwsOrd" kCHK_ST(kVAL_PartCanceled, kVAL_ExchangeAccepted) kCHK_PRI(200) kCHK_QTYS(7000,3000,0) "|LastExgTime=09:00:00.2|LastPriTime=09:00:00",
   };
   RunTest(core, "Out-of-order(PartCancel)", cstrTestList, fon9::numofele(cstrTestList));
}
//--------------------------------------------------------------------------//
int main(int argc, char* argv[]) {
   gArgc = argc;
   gArgv = argv;
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif

   const auto isTestReload = fon9::GetCmdArg(argc, argv, "r", "reload");
   gIsTestReload = (isTestReload.begin() && (isTestReload.empty() || fon9::toupper(static_cast<unsigned char>(*isTestReload.begin())) == 'Y'));

   fon9::AutoPrintTestInfo utinfo{"OmsReport"};
   TestCoreSP core;

   TestNormalOrder(core);
   utinfo.PrintSplitter();

   TestOutofOrder(core);
   utinfo.PrintSplitter();

   TestNoNewFilled(core);
   utinfo.PrintSplitter();

   TestNoNewChgQty(core);
   utinfo.PrintSplitter();

   TestNoNewDelete(core);
   utinfo.PrintSplitter();

   TestNoNewQuery(core);
   utinfo.PrintSplitter();

   TestNormalReport(core);
   utinfo.PrintSplitter();

   TestPartCancel(core);
   utinfo.PrintSplitter();
   //---------------------------------------------
   // Query, ChgPri 回報, 有 ReportPending 的需求嗎?
   // ChgPri => 需要 => 如果 Client 端使用 FIX 下單, 則必須依序處理下單要求及回報.
   // Query => 不需要, 因應異常處理(例:新單回報遺漏), 透過查單確認結果後, 手動補回報.
   //       => 查單不改變 Order 的數量(但可能改變 Order.LastPri);
   //       => 所以不會改變 OrderSt;
   //       => 只要在 Message 顯示當下的結果就足夠了.
   //---------------------------------------------
}
