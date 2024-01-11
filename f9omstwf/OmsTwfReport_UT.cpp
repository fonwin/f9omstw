// \file f9omstw/OmsTwfReport_UT.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsReport_RunTest.hpp"
#include "f9omstwf/OmsTwfReport3.hpp"
using namespace f9omstw;
//--------------------------------------------------------------------------//
#define kSYMID_H  "TXF"
#define kSYMID_1  "L9"
#define kSYMID_2  "C0"
#define kSYMID    kSYMID_H kSYMID_1

void InitTestCore(TestCore& core) {
   auto  ordfac1 = core.Owner_->OrderFactoryPark().GetFactory("TwfOrd");
   auto  ordfac9 = core.Owner_->OrderFactoryPark().GetFactory("TwfOrdQ");
   core.Owner_->SetRequestFactoryPark(new OmsRequestFactoryPark(
      new OmsTwfRequestIni1Factory("TwfNew", ordfac1,
                                  OmsRequestRunStepSP{new UomsTwfIniRiskCheck{
                                       OmsRequestRunStepSP{new UomsTwfExgSender}}}),
      new OmsTwfRequestChg1Factory("TwfChg", OmsRequestRunStepSP{new UomsTwfExgSender}),
      new OmsTwfFilled1Factory("TwfFil", ordfac1, ordfac9),
      new OmsTwfFilled2Factory("TwfFil2", ordfac1),
      new OmsTwfReport2Factory("TwfRpt", ordfac1),
      new OmsTwfReport3Factory("TwfR03", nullptr)
      ));
   core.Owner_->ReloadErrCodeAct(
      "$x=x\n" // 用 $ 開頭, 表示此處提供的是設定內容, 不是檔案名稱.
      "40002:Rerun=3|Memo=TIME IS EARLY\n"
      "40005:Rerun=4|AtNewDone=Y|UseNewLine=Y|Memo=ORDER NOT FOUND\n"
      "40004:Rerun=2|AtNewDone=Y|Memo=商品處理中，暫時不接受委託\n"
      "40010:St=ExchangeNoLeavesQty\n"
   );
   core.GetResource().Symbs_->FetchOmsSymb(kSYMID_H kSYMID_1)->PriceOrigDiv_ = 1;
   core.GetResource().Symbs_->FetchOmsSymb(kSYMID_H kSYMID_2)->PriceOrigDiv_ = 1;
   core.OpenReload(gArgc, gArgv, "OmsTwfReport_UT.log");

#define kErrCode_0                  "|ErrCode=0"
#define kErrCode_NoLeaves           "|ErrCode=40010"
#define kErrCode_Rerun2_AtNewDone   "|ErrCode=40004"
#define kErrCode_Rerun3             "|ErrCode=40002"
#define kErrCode_Rerun4_AtNewDone   "|ErrCode=40005"
#define kErrCode_NoRerun            "|ErrCode=40001"
}
//--------------------------------------------------------------------------//
#define kTIME0 "20191107090000.000000"
#define kTIME1 "20191107090001.000000"
#define kTIME2 "20191107090002.000000"
#define kTIME3 "20191107090003.000000"
#define kTIME4 "20191107090004.000000"
#define kTIME5 "20191107090005.000000"
#define kTIME6 "20191107090006.000000"
#define kTIME7 "20191107090007.000000"
#define kTIME8 "20191107090008.000000"
#define kTIME9 "20191107090009.000000"
#define kTIMEA "20191107090010.000000"
#define kTIMEB "20191107090011.000000"
#define kTIMEC "20191107090012.000000"
#define kTIMED "20191107090013.000000"
#define kTIMEE "20191107090014.000000"
//--------------------------------------------------------------------------//
#define kOrdKey               "|Market=f|SessionId=N|BrkId=8610"
#define kReqBase              kOrdKey "|IvacNo=10|Symbol=" kSYMID "|Side=B|PosEff=O"
#define kPriLmt(tif,pri)      "|TimeInForce=" #tif "|Pri=" #pri "|PriType=L"
//#define kPriMkt               "|Pri=|PriType=M"

// [0]: '+'=Report (占用序號).
// [0]: 'v'=「新單合併成交」(占用「下一個」序號).
// [0]: '-'=Report (不占用序號).
// [0]: '*'=Request 透過 core.MoveToCore(OmsRequestRunner{}) 執行;
// [0]: '/'=檢查 OrderRaw 是否符合預期. (占用序號).
// [1..]: 參考資料「'L' + LineNo」或「序號-N」

// 新單要求測試內容:Pri=200,Qty=10,TimeInForce=R
#define kTwfNew   "* TwfNew" kReqBase "|Qty=10" kPriLmt(R,200)
// 新單要求 core.MoveToCore(OmsRequestRunner{}) 的結果.
#define kChkOrderNewSending \
        kChkOrderST(Sending,0,10,10,/*No exgTime*/) kOrdPri(R,200,/*No LastPriTime*/)

// -----
#define kChkOrder(ordSt,reqSt,bfQty,afQty,leaves,exgTime) \
        "/ TwfOrd" kOrdReqST(kVAL_##ordSt, kVAL_##reqSt)  \
                   kOrdQtys(bfQty,afQty,leaves)           \
                   "|LastExgTime=" exgTime
#define kChkOrderST(st,bfQty,afQty,leaves,exgTime) \
        kChkOrder(st,st,bfQty,afQty,leaves,exgTime)

#define kOrdPri(tif,pri,priTime) \
        "|LastTimeInForce=" #tif "|LastPriTime=" priTime "|LastPri=" #pri "|LastPriType=L"
// -----
#define kTwfRpt(st,kind,bfQty,afQty,exgTime) \
        "TwfRpt" kRptVST(st) kReqBase "|Kind=" #kind \
                 "|BeforeQty=" #bfQty "|Qty=" #afQty "|ExgTime=" exgTime

#define kTwfFil(matKey,qty,pri,tm) \
        "TwfFil" kReqBase "|MatchKey=" #matKey "|Qty=" #qty "|Pri=" #pri "|Time=" tm
#define kTwfFil2(matKey,qty,pri1,pri2,tm) \
        "TwfFil2" kReqBase "|MatchKey=" #matKey "|Qty=" #qty "|Pri=" #pri1 "|PriLeg2=" #pri2 "|Time=" tm

#define kTwfR03(kind,tm) \
        "TwfR03" kOrdKey kRptVST(ExchangeRejected) "|Kind=" #kind "|Side=B" \
                 "|ExgTime=" tm
//--------------------------------------------------------------------------//
// 一般正常順序下單回報.
void TestNormalOrder(TestCoreSP& core) {
   const char* cstrTestList[] = {
kTwfNew,
kChkOrderNewSending,
"-2." kTwfRpt(ExchangeAccepted,N,     10,10,   kTIME0) kPriLmt(R,200),
kChkOrderST(ExchangeAccepted,         10,10,10,kTIME0) kOrdPri(R,200,kTIME0),
// 成交.
"+1." kTwfFil(1,                                                                     1,201,kTIME1),
kChkOrder(PartFilled,PartFilled,      10, 9, 9,kTIME0) kOrdPri(R,200,kTIME0) kOrdCum(1,201,kTIME1),
"+1." kTwfFil(2,                                                                     2,202,kTIME2),
kChkOrder(PartFilled,PartFilled,       9, 7, 7,kTIME0) kOrdPri(R,200,kTIME0) kOrdCum(3,605,kTIME2),
// 減3
"*1.TwfChg" kOrdKey "|Qty=-3",
kChkOrder(PartFilled,Sending,          7, 7, 7,kTIME0) kOrdPri(R,200,kTIME0) kOrdCum(3,605,kTIME2),
"-2." kTwfRpt(ExchangeAccepted,C,      7, 4,   kTIME3),
kChkOrder(PartFilled,ExchangeAccepted, 7, 4, 4,kTIME3) kOrdPri(R,200,kTIME0) kOrdCum(3,605,kTIME2),
// 改價$203
"*1.TwfChg" kOrdKey kPriLmt(R,203),
kChkOrder(PartFilled,Sending,          4, 4, 4,kTIME3) kOrdPri(R,200,kTIME0) kOrdCum(3,605,kTIME2),
"-2." kTwfRpt(ExchangeAccepted,P,      4, 4,   kTIME4) kPriLmt(R,203),
kChkOrder(PartFilled,ExchangeAccepted, 4, 4, 4,kTIME4) kOrdPri(R,203,kTIME4) kOrdCum(3,605,kTIME2),
// 成交4
"+1." kTwfFil(5,                                                                     4, 204,kTIME5),
kChkOrder(FullFilled,PartFilled,       4, 0, 0,kTIME4) kOrdPri(R,203,kTIME4) kOrdCum(7,1421,kTIME5),
// 刪單
"*1.TwfChg" kOrdKey "|Qty=0",
kChkOrder(FullFilled,Sending,          0, 0, 0,kTIME4) kOrdPri(R,203,kTIME4) kOrdCum(7,1421,kTIME5),
"-2." kTwfR03(D,                               kTIME6)                                              kErrCode_NoLeaves,
kChkOrder(FullFilled,ExchangeNoLeaves, 0, 0, 0,kTIME6) kOrdPri(R,203,kTIME4) kOrdCum(7,1421,kTIME5) kErrCode_NoLeaves,
// - 新單失敗 -----
kTwfNew,
kChkOrderNewSending,
"-2." kTwfR03(N,                                      kTIME0),
kChkOrder(ExchangeRejected,ExchangeRejected, 10, 0, 0,kTIME0) kOrdPri(R,200,""),
kTwfNew,
kChkOrderNewSending,
"-2." kTwfR03(N,                                      kTIME0),
kChkOrder(ExchangeRejected,ExchangeRejected, 10, 0, 0,kTIME0) kOrdPri(R,200,""),
   };
   RunTestList(core, "Normal-order", cstrTestList);
}
//--------------------------------------------------------------------------//
// 特殊成交回報: 新單合併成交, IOC、FOK最終刪單回報, 47、48成交回報.
void TestSpecialFilled(TestCoreSP& core) {
   const char* cstrTestListNormal[] = {
// - 新單合併成交(沒有額外的新單回報) -----
kTwfNew,
kChkOrderNewSending,
"v1." kTwfFil(1,                                                                2,201,kTIME1) "|PriOrd=202",
kChkOrderST(ExchangeAccepted,    10,10,10,kTIME1) kOrdPri(R,202,kTIME1),
kChkOrder(PartFilled,PartFilled, 10, 8, 8,kTIME1) kOrdPri(R,202,kTIME1) kOrdCum(2,402,kTIME1),
"+1." kTwfFil(2,0,0,kTIME2) "|QtyCanceled=8", // 成交2口後, 交易所主動刪除剩餘的8口.
kChkOrderST(ExchangeCanceled,     8, 0, 0,kTIME2) kOrdPri(R,202,kTIME1) kOrdCum(2,402,kTIME1),
// - 新單合併成交, 部分成交, 部分刪單 -----
kTwfNew,
kChkOrderNewSending,
"v1." kTwfFil(1,2,201, kTIME1) "|QtyCanceled=8|PriOrd=202", // 成交2口, 交易所主動刪除剩餘的8口. 期交所使用一筆回報.
kChkOrderST(ExchangeAccepted,    10,10,10,kTIME1) kOrdPri(R,202,kTIME1),
kChkOrder(PartFilled,PartFilled, 10, 8, 8,kTIME1) kOrdPri(R,202,kTIME1) kOrdCum(2,402,kTIME1),
kChkOrderST(ExchangeCanceled,     8, 0, 0,kTIME1) kOrdPri(R,202,kTIME1) kOrdCum(2,402,kTIME1),
   };
   RunTestList(core, "Special filled(Normal-order)", cstrTestListNormal);

   const char* cstrTestListOutofOrder[] = {
// - 新單合併成交2(沒有額外的新單回報) => 交易所刪7(遺漏1) => 補足1 -----
kTwfNew,
kChkOrderNewSending,
"v1." kTwfFil(1,                                                                        2,201,kTIME1),
kChkOrderST(ExchangeAccepted,            10,10,10,kTIME1) kOrdPri(R,200,kTIME1),
kChkOrder(PartFilled,PartFilled,         10, 8, 8,kTIME1) kOrdPri(R,200,kTIME1) kOrdCum(2,402,kTIME1),
"+1." kTwfFil(2,0,0,kTIME2) "|QtyCanceled=7", // 收到上方的成交2口後, 交易所主動刪除剩餘7口, 所以有1口遺漏.
kChkOrder(ReportPending,ExchangeCanceled, 8, 8, 8,kTIME1) kOrdPri(R,200,kTIME1) kOrdCum(2,402,kTIME1),
// 補足1口, 然後處理交易所刪除7口.
"+1." kTwfFil(3,                                                                        1,200,kTIME3),
kChkOrder(PartFilled,PartFilled,          8, 7, 7,kTIME1) kOrdPri(R,200,kTIME1) kOrdCum(3,602,kTIME3),
kChkOrderST(ExchangeCanceled,             7, 0, 0,kTIME2) kOrdPri(R,200,kTIME1) kOrdCum(3,602,kTIME3),
// - 新單合併成交2/交易所刪7(遺漏1) => 補足1 -----
kTwfNew,
kChkOrderNewSending,
"v1." kTwfFil(1,2,201, kTIME1) "|QtyCanceled=7|PriOrd=202", // 成交2口, 交易所主動刪除剩餘的7口(遺漏1口). 期交所使用一筆回報.
kChkOrderST(ExchangeAccepted,       10,10,10,kTIME1) kOrdPri(R,202,kTIME1),
kChkOrder(ReportPending,PartFilled, 10,10,10,kTIME1) kOrdPri(R,202,kTIME1) kOrdCum(0,0,""),
// 補足1口, 然後處理:成交2 及 刪除7.
"+1." kTwfFil(3,                                                                     1,200,kTIME3),
kChkOrder(PartFilled,PartFilled,    10, 9, 9,kTIME1) kOrdPri(R,202,kTIME1) kOrdCum(1,200,kTIME3),
kChkOrder(PartFilled,PartFilled,     9, 7, 7,kTIME1) kOrdPri(R,202,kTIME1) kOrdCum(3,602,kTIME1),
kChkOrderST(ExchangeCanceled,        7, 0, 0,kTIME1) kOrdPri(R,202,kTIME1) kOrdCum(3,602,kTIME1),

// - 新單合併成交, 但沒有 order.Initiator() -----
// 新單合併成交回報要如何處理? 由回報接收端決定:
// 1. 使用 TwfRpt(Accepted) + TwfFil(或 TwfFil2): 若新單不存在則使用 TwfRpt 自動補單.
// 2. 使用 TwfFil(或 TwfFil2): 若新單不存在則 ReportPending; 等候新單補單.
// 這裡驗證: 方法2.
">AllocOrdNo=f-N-8610-A",
"+"   kTwfFil(1,1,201,kTIME1) "|QtyCanceled=7|PriOrd=203", // 成交1口, 交易所主動刪除剩餘的7口(遺漏1口,且無order.Initiator).
kChkOrder(ReportPending,PartFilled, 0, 0, 0,"") "|LastTimeInForce=|LastPriTime=|LastPri=|LastPriType=" kOrdCum(0,0,""),
// 回報補單: 新單10口; 因(成交1 及 刪除7)仍遺漏2口,所以繼續等.
"+1." kTwfRpt(ExchangeAccepted,N,  10,10,   kTIME0) kPriLmt(R,200),
kChkOrderST(ExchangeAccepted,       0,10,10,kTIME0) kOrdPri(R,200,kTIME0),
"+1." kTwfFil(2,                                                                   2,200,kTIME2),
kChkOrder(PartFilled,PartFilled,   10, 8, 8,kTIME0) kOrdPri(R,200,kTIME0) kOrdCum(2,400,kTIME2),
kChkOrder(PartFilled,PartFilled,    8, 7, 7,kTIME0) kOrdPri(R,203,kTIME1) kOrdCum(3,601,kTIME1),
kChkOrderST(ExchangeCanceled,       7, 0, 0,kTIME1) kOrdPri(R,203,kTIME1) kOrdCum(3,601,kTIME1),
   };
   RunTestList(core, "Special filled(OutofOrder)", cstrTestListOutofOrder);

   const char* cstrTestList_47_48[] = {
// - status_code=47,48; => 使用 ErrCode 記錄 -----
// - 47 未成交/剩餘取消: ST=ExchangeCanceling
//    => 48(ExchangeCanceled)
// - 47 部分成交/剩餘取消: OrdSt=PartFilled,ReqSt=Filled
//    => OrdSt=ExchangeCanceling
//    => 48(ExchangeCanceled)
kTwfNew,
kChkOrderNewSending,
"v1." kTwfFil(1,2,201, kTIME1) "|QtyCanceled=8|ErrCode=40047",
kChkOrderST(ExchangeAccepted,    10,10,10,kTIME1) kOrdPri(R,200,kTIME1),
kChkOrder(PartFilled,PartFilled, 10, 8, 8,kTIME1) kOrdPri(R,200,kTIME1) kOrdCum(2,402,kTIME1) "|ErrCode=40047",
kChkOrderST(Canceling,            8, 0, 0,kTIME1) kOrdPri(R,200,kTIME1) kOrdCum(2,402,kTIME1) "|ErrCode=40047",
"+1." kTwfFil(2,0,202, kTIME2) "|ErrCode=40048", // 取消, 並提供: 上限價 or 下限價.
kChkOrderST(ExchangeCanceled,     0, 0, 0,kTIME2) kOrdPri(R,200,kTIME1) kOrdCum(2,402,kTIME1) "|ErrCode=40048",
// 亂序.
kTwfNew,
kChkOrderNewSending,
"v1." kTwfFil(2,0,202, kTIME2) "|ErrCode=40048", // 取消, 並提供: 上限價 or 下限價.
kChkOrderST(ExchangeAccepted,             10,10,10,kTIME2) kOrdPri(R,200,kTIME2),
kChkOrder(ReportPending,ExchangeCanceled, 10,10,10,kTIME2) kOrdPri(R,200,kTIME2),
"+1." kTwfFil(1,2,201, kTIME1) "|QtyCanceled=8|ErrCode=40047",
kChkOrder(PartFilled,PartFilled,          10, 8, 8,kTIME2) kOrdPri(R,200,kTIME2) kOrdCum(2,402,kTIME1) "|ErrCode=40047",
kChkOrderST(Canceling,                     8, 0, 0,kTIME1) kOrdPri(R,200,kTIME2) kOrdCum(2,402,kTIME1) "|ErrCode=40047",
kChkOrderST(ExchangeCanceled,              0, 0, 0,kTIME2) kOrdPri(R,200,kTIME2) kOrdCum(2,402,kTIME1) "|ErrCode=40048",
   };
   RunTestList(core, "Special filled(status_code=47,48)", cstrTestList_47_48);
}
//--------------------------------------------------------------------------//
// 下單後,亂序回報.
void TestOutofOrder(TestCoreSP& core) {
   const char* cstrTestList[] = {
kTwfNew,
kChkOrderNewSending,
// 新單尚未回報, 先收到「成交」 => 新單合併成交 => 會先更新委託狀態:ExchangeAccepted, 然後直接處理成交回報.
"", // "+1." kTwfFil(1,1,201,kTIME1),
"", // kChkOrder(ReportPending,PartFilled,      10,10,10,"")     kOrdPri(R,200,"") kOrdCum(0,0,""),
"+1." kTwfRpt(ExchangeAccepted,C,        10, 7,   kTIME1) kPriLmt(R,200),
kChkOrder(ReportPending,ExchangeAccepted,10, 7,10,kTIME1) kOrdPri(R,200,kTIME1),

// 新單尚未回報, 先收到「刪、改、查」 => 透過 OmsOrderRaw.UpdateOrderSt_ = ReportPending 保留.
// 減1成功.
"*1.TwfChg" kOrdKey "|Qty=-1",
kChkOrder(Sending,Sending,               10,10,10,kTIME1) kOrdPri(R,200,kTIME1),
"-2." kTwfRpt(ExchangeAccepted,C,         7, 6,   kTIME3) kPriLmt(R,200),
kChkOrder(ReportPending,ExchangeAccepted, 7, 6,10,kTIME3) kOrdPri(R,200,kTIME1),
// ** 減2沒回報(Ln#11)
"*1.TwfChg" kOrdKey "|Qty=-2",
kChkOrder(Sending,Sending,               10,10,10,kTIME3) kOrdPri(R,200,kTIME1),
// 改價$203成功
"*1.TwfChg" kOrdKey kPriLmt(R,203),
kChkOrder(Sending,Sending,               10,10,10,kTIME3) kOrdPri(R,200,kTIME1),
"-2." kTwfRpt(ExchangeAccepted,P,         4, 4,   kTIME5) kPriLmt(R,203),
kChkOrder(ReportPending,ExchangeAccepted, 4, 4,10,kTIME5) kOrdPri(R,203,kTIME5),
// >> 刪單未回報(Ln#17).
"*1.TwfChg" kOrdKey "|Qty=0",
kChkOrder(Sending,Sending,               10,10,10,kTIME5) kOrdPri(R,203,kTIME5),
// 刪單成功, 但會等「減2成功」後才處理.
"*1.TwfChg" kOrdKey "|Qty=0",
kChkOrder(Sending,Sending,               10,10,10,kTIME5) kOrdPri(R,203,kTIME5),
"-2." kTwfRpt(ExchangeAccepted,D,         4, 0,   kTIME6) kPriLmt(R,203),
kChkOrder(ReportPending,ExchangeAccepted, 4, 0,10,kTIME6) kOrdPri(R,203,kTIME5),
// >> 刪單失敗(接續 Ln#17), 等刪單成功後處理.
"-L17." kTwfR03(D,                                kTIME9)                       kErrCode_NoLeaves,
kChkOrder(ReportPending,ExchangeNoLeaves, 0, 0,10,kTIME9) kOrdPri(R,203,kTIME5) kErrCode_NoLeaves,
// ** 減2回報成功(接續 Ln#11).
"-L11." kTwfRpt(ExchangeAccepted,C,       6, 4,   kTIME4) kPriLmt(R,200),
kChkOrder(ReportPending,ExchangeAccepted, 6, 4,10,kTIME4) kOrdPri(R,203,kTIME5) kErrCode_0,
// -------------------------------------
// 新單回報(接續 Ln#1) => 處理等候中的回報.
"-L1." kTwfRpt(ExchangeAccepted,N,       10,10,   kTIME0) kPriLmt(R,200),
kChkOrderST(ExchangeAccepted,            10,10,10,kTIME0) kOrdPri(R,200,kTIME0) kErrCode_0,
kChkOrderST(ExchangeAccepted,            10, 7, 7,kTIME1) kOrdPri(R,200,kTIME0) kErrCode_0,
kChkOrderST(ExchangeAccepted,             7, 6, 6,kTIME3) kOrdPri(R,200,kTIME0),
kChkOrderST(ExchangeAccepted,             4, 4, 6,kTIME5) kOrdPri(R,203,kTIME5),
kChkOrderST(ExchangeAccepted,             6, 4, 4,kTIME4) kOrdPri(R,203,kTIME5),
kChkOrder(UserCanceled,ExchangeAccepted,  4, 0, 0,kTIME6) kOrdPri(R,203,kTIME5) kErrCode_0,
kChkOrder(UserCanceled,ExchangeNoLeaves,  0, 0, 0,kTIME9) kOrdPri(R,203,kTIME5) kErrCode_NoLeaves,
   };
   RunTestList(core, "Out-of-order", cstrTestList);
}
//--------------------------------------------------------------------------//
// 委託不存在(尚未收到新單回報), 先收到成交及其他回報.
void TestNoNewFilled(TestCoreSP& core) {
   const char* cstrTestList[] = {
// 先收到刪改查成交 => 透過 OmsOrderRaw.UpdateOrderSt_ = ReportPending 保留.
">AllocOrdNo=f-N-8610-A",
"+"   kTwfFil(1,1,201,kTIME1),
kChkOrder(ReportPending,PartFilled,0,0,0,"") kOrdCum(0,0,""),
"+1." kTwfFil(2,2,202,kTIME2),
kChkOrder(ReportPending,PartFilled,0,0,0,"") kOrdCum(0,0,""),
// 減1成功.
"+1." kTwfRpt(ExchangeAccepted,C,        7,6,  kTIME3) kPriLmt(R,200),
kChkOrder(ReportPending,ExchangeAccepted,7,6,0,kTIME3) kOrdPri(R,200,kTIME3),
// 改價203成功,但「遺漏2」
"+1." kTwfRpt(ExchangeAccepted,P,        4,4,  kTIME5) kPriLmt(R,203),
kChkOrder(ReportPending,ExchangeAccepted,4,4,0,kTIME5) kOrdPri(R,203,kTIME5),
// 沒有「原始下單要求」的 R03 失敗會直接拋棄, 不會占用序號.
"-1." kTwfR03(D,                               kTIME9)                       kErrCode_NoLeaves,
// 刪單失敗, 等「LeavesQty==0」後處理.
"+1." kTwfRpt(ExchangeRejected,D,        0,0,  kTIME9)                       kErrCode_NoLeaves,
kChkOrder(ReportPending,ExchangeNoLeaves,0,0,0,kTIME9) kOrdPri(R,203,kTIME5) kErrCode_NoLeaves,
// 刪單成功, 但會等「遺漏2補齊」後才處理.
"+1." kTwfRpt(ExchangeAccepted,D,        4,0,  kTIME6) kPriLmt(R,203),
kChkOrder(ReportPending,ExchangeAccepted,4,0,0,kTIME6) kOrdPri(R,203,kTIME5) kErrCode_0,
// 減2回報成功.
"+1." kTwfRpt(ExchangeAccepted,C,        6,4,  kTIME4) kPriLmt(R,200),
kChkOrder(ReportPending,ExchangeAccepted,6,4,0,kTIME4) kOrdPri(R,203,kTIME5),
// -------------------------------------
// 新單回報
"+1." kTwfRpt(ExchangeAccepted,N,       10,10,   kTIME0) kPriLmt(R,200),
kChkOrderST(ExchangeAccepted,            0,10,10,kTIME0) kOrdPri(R,200,kTIME0),
kChkOrder(PartFilled,PartFilled,        10, 9, 9,kTIME0) kOrdPri(R,200,kTIME0) kOrdCum(1,201,kTIME1),
kChkOrder(PartFilled,PartFilled,         9, 7, 7,kTIME0) kOrdPri(R,200,kTIME0) kOrdCum(3,605,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,   7, 6, 6,kTIME3) kOrdPri(R,200,kTIME0) kOrdCum(3,605,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,   4, 4, 6,kTIME5) kOrdPri(R,203,kTIME5) kOrdCum(3,605,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,   6, 4, 4,kTIME4) kOrdPri(R,203,kTIME5) kOrdCum(3,605,kTIME2),
kChkOrder(UserCanceled,ExchangeAccepted, 4, 0, 0,kTIME6) kOrdPri(R,203,kTIME5) kOrdCum(3,605,kTIME2) kErrCode_0,
kChkOrder(UserCanceled,ExchangeNoLeaves, 0, 0, 0,kTIME9) kOrdPri(R,203,kTIME5) kOrdCum(3,605,kTIME2) kErrCode_NoLeaves,
   };
   RunTestList(core, "Out-of-order(NoNewFilled)", cstrTestList);
}
//--------------------------------------------------------------------------//
// 委託不存在(尚未收到新單回報), 先收到ChgQty及其他回報.
void TestNoNewChgQty(TestCoreSP& core) {
   const char* cstrTestList[] = {
">AllocOrdNo=f-N-8610-A",
// 減1成功.
"+" kTwfRpt(ExchangeAccepted,C,          7,6,  kTIME3) kPriLmt(R,200),
kChkOrder(ReportPending,ExchangeAccepted,7,6,0,kTIME3) kOrdPri(R,200,kTIME3),
// 成交.
"+1." kTwfFil(1,1,201,kTIME1),
kChkOrder(ReportPending,PartFilled,      0,0,0,kTIME3) kOrdPri(R,200,kTIME3) kOrdCum(0,0,""),
"+1." kTwfFil(2,2,202,kTIME2),
kChkOrder(ReportPending,PartFilled,      0,0,0,kTIME3) kOrdPri(R,200,kTIME3) kOrdCum(0,0,""),
// 改價203成功,但「遺漏2」
"+1." kTwfRpt(ExchangeAccepted,P,        4,4,  kTIME5) kPriLmt(R,203),
kChkOrder(ReportPending,ExchangeAccepted,4,4,0,kTIME5) kOrdPri(R,203,kTIME5),
// 沒有「原始下單要求」的 R03 失敗會直接拋棄, 不會占用序號.
"-1." kTwfR03(D,                               kTIME9)                       kErrCode_NoLeaves,
// 刪單失敗, 等「LeavesQty==0」後處理.
"+1." kTwfRpt(ExchangeRejected,D,        0,0,  kTIME9)                       kErrCode_NoLeaves,
kChkOrder(ReportPending,ExchangeNoLeaves,0,0,0,kTIME9) kOrdPri(R,203,kTIME5) kErrCode_NoLeaves,
// 刪單成功, 但會等「遺漏2補齊」後才處理.
"+1." kTwfRpt(ExchangeAccepted,D,        4,0,  kTIME6) kPriLmt(R,203),
kChkOrder(ReportPending,ExchangeAccepted,4,0,0,kTIME6) kOrdPri(R,203,kTIME5) kErrCode_0,
// 減2回報成功.
"+1." kTwfRpt(ExchangeAccepted,C,        6,4,  kTIME4) kPriLmt(R,200),
kChkOrder(ReportPending,ExchangeAccepted,6,4,0,kTIME4) kOrdPri(R,203,kTIME5) kErrCode_0,
// -------------------------------------
// 新單回報
"+1." kTwfRpt(ExchangeAccepted,N,       10,10,   kTIME0) kPriLmt(R,200),
kChkOrderST(ExchangeAccepted,            0,10,10,kTIME0) kOrdPri(R,200,kTIME0),
kChkOrderST(ExchangeAccepted,            7, 6, 9,kTIME3) kOrdPri(R,200,kTIME0),
kChkOrder(PartFilled,PartFilled,         9, 8, 8,kTIME3) kOrdPri(R,200,kTIME0) kOrdCum(1,201,kTIME1),
kChkOrder(PartFilled,PartFilled,         8, 6, 6,kTIME3) kOrdPri(R,200,kTIME0) kOrdCum(3,605,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,   4, 4, 6,kTIME5) kOrdPri(R,203,kTIME5) kOrdCum(3,605,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,   6, 4, 4,kTIME4) kOrdPri(R,203,kTIME5) kOrdCum(3,605,kTIME2),
kChkOrder(UserCanceled,ExchangeAccepted, 4, 0, 0,kTIME6) kOrdPri(R,203,kTIME5) kOrdCum(3,605,kTIME2) kErrCode_0,
kChkOrder(UserCanceled,ExchangeNoLeaves, 0, 0, 0,kTIME9) kOrdPri(R,203,kTIME5) kOrdCum(3,605,kTIME2) kErrCode_NoLeaves,
   };
   RunTestList(core, "Out-of-order(NoNewChgQty)", cstrTestList);
}
//--------------------------------------------------------------------------//
// 委託不存在(尚未收到新單回報), 先收到Delete及其他回報.
void TestNoNewDelete(TestCoreSP& core) {
   const char* cstrTestList[] = {
">AllocOrdNo=f-N-8610-A",
// 刪單成功(↑分配委託書號), 但會等「LeavesQty=BeforeQty=4」才處理.
"+" kTwfRpt(ExchangeAccepted,D,          4,0,  kTIME6) kPriLmt(R,203),
kChkOrder(ReportPending,ExchangeAccepted,4,0,0,kTIME6) kOrdPri(R,203,kTIME6),
// 減1成功.
"+1" kTwfRpt(ExchangeAccepted,C,         7,6,  kTIME3) kPriLmt(R,200),
kChkOrder(ReportPending,ExchangeAccepted,7,6,0,kTIME3) kOrdPri(R,203,kTIME6),
// 成交.
"+1." kTwfFil(1,1,201,kTIME1),
kChkOrder(ReportPending,PartFilled,      0,0,0,kTIME3) kOrdPri(R,203,kTIME6) kOrdCum(0,0,""),
"+1." kTwfFil(2,2,202,kTIME2),
kChkOrder(ReportPending,PartFilled,      0,0,0,kTIME3) kOrdPri(R,203,kTIME6) kOrdCum(0,0,""),
// 改價203成功.
"+1." kTwfRpt(ExchangeAccepted,P,        4,4,  kTIME5) kPriLmt(R,203),
kChkOrder(ReportPending,ExchangeAccepted,4,4,0,kTIME5) kOrdPri(R,203,kTIME6) kOrdCum(0,0,""),
// 沒有「原始下單要求」的 R03 失敗會直接拋棄, 不會占用序號.
"-1." kTwfR03(D,                               kTIME9)                       kErrCode_NoLeaves,
// 刪單失敗, 等「LeavesQty==0」後處理.
"+1." kTwfRpt(ExchangeRejected,D,        0,0,  kTIME9)                       kErrCode_NoLeaves,
kChkOrder(ReportPending,ExchangeNoLeaves,0,0,0,kTIME9) kOrdPri(R,203,kTIME6) kErrCode_NoLeaves,
// 減2回報成功.
"+1." kTwfRpt(ExchangeAccepted,C,        6,4,  kTIME4) kPriLmt(R,200),
kChkOrder(ReportPending,ExchangeAccepted,6,4,0,kTIME4) kOrdPri(R,203,kTIME6) kErrCode_0,
// -------------------------------------
// 新單回報
"+1." kTwfRpt(ExchangeAccepted,N,       10,10,   kTIME0) kPriLmt(R,200),
kChkOrderST(ExchangeAccepted,            0,10,10,kTIME0) kOrdPri(R,200,kTIME0),
kChkOrderST(ExchangeAccepted,            7, 6, 9,kTIME3) kOrdPri(R,203,kTIME6),
kChkOrder(PartFilled,PartFilled,         9, 8, 8,kTIME3) kOrdPri(R,203,kTIME6) kOrdCum(1,201,kTIME1),
kChkOrder(PartFilled,PartFilled,         8, 6, 6,kTIME3) kOrdPri(R,203,kTIME6) kOrdCum(3,605,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,   4, 4, 6,kTIME5) kOrdPri(R,203,kTIME6) kOrdCum(3,605,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,   6, 4, 4,kTIME4) kOrdPri(R,203,kTIME6) kOrdCum(3,605,kTIME2),
kChkOrder(UserCanceled,ExchangeAccepted, 4, 0, 0,kTIME6) kOrdPri(R,203,kTIME6) kOrdCum(3,605,kTIME2) kErrCode_0,
kChkOrder(UserCanceled,ExchangeNoLeaves, 0, 0, 0,kTIME9) kOrdPri(R,203,kTIME6) kOrdCum(3,605,kTIME2) kErrCode_NoLeaves,
   };
   RunTestList(core, "Out-of-order(NoNewDelete)", cstrTestList);
}
//--------------------------------------------------------------------------//
// 委託不存在(尚未收到新單回報), 先收到Query及其他回報.
void TestNoNewQuery(TestCoreSP& core) {
   const char* cstrTestList[] = {
">AllocOrdNo=f-N-8610-A",
"+" kTwfRpt(ExchangeAccepted,Q,          4,4,  kTIME8) kPriLmt(R,203),
kChkOrder(Initialing,ExchangeAccepted,   4,4,0,kTIME8) kOrdPri(R,203,kTIME8),
// 刪單成功, 但會等「LeavesQty=BeforeQty=4」才處理.
"+1" kTwfRpt(ExchangeAccepted,D,         4,0,  kTIME6) kPriLmt(R,203),
kChkOrder(ReportPending,ExchangeAccepted,4,0,0,kTIME6) kOrdPri(R,203,kTIME8),
// 減1成功.
"+1" kTwfRpt(ExchangeAccepted,C,         7,6,  kTIME3) kPriLmt(R,200),
kChkOrder(ReportPending,ExchangeAccepted,7,6,0,kTIME3) kOrdPri(R,203,kTIME8),
// 成交.
"+1." kTwfFil(1,1,201,kTIME1),
kChkOrder(ReportPending,PartFilled,      0,0,0,kTIME3) kOrdPri(R,203,kTIME8) kOrdCum(0,0,""),
"+1." kTwfFil(2,2,202,kTIME2),
kChkOrder(ReportPending,PartFilled,      0,0,0,kTIME3) kOrdPri(R,203,kTIME8) kOrdCum(0,0,""),
// 改價203成功.
"+1." kTwfRpt(ExchangeAccepted,P,        4,4,  kTIME5) kPriLmt(R,203),
kChkOrder(ReportPending,ExchangeAccepted,4,4,0,kTIME5) kOrdPri(R,203,kTIME8),
// 沒有「原始下單要求」的 R03 失敗會直接拋棄, 不會占用序號.
"-1." kTwfR03(D,                               kTIME9)                       kErrCode_NoLeaves,
// 刪單失敗, 等「LeavesQty==0」後處理.
"+1." kTwfRpt(ExchangeRejected,D,        0,0,  kTIME9)                       kErrCode_NoLeaves,
kChkOrder(ReportPending,ExchangeNoLeaves,0,0,0,kTIME9) kOrdPri(R,203,kTIME8) kErrCode_NoLeaves,
// 減2回報成功.
"+1." kTwfRpt(ExchangeAccepted,C,        6,4,  kTIME4) kPriLmt(R,200),
kChkOrder(ReportPending,ExchangeAccepted,6,4,0,kTIME4) kOrdPri(R,203,kTIME8) kErrCode_0,
// -------------------------------------
// 新單回報
"+1." kTwfRpt(ExchangeAccepted,N,       10,10,   kTIME0) kPriLmt(R,200),
kChkOrderST(ExchangeAccepted,            0,10,10,kTIME0) kOrdPri(R,200,kTIME0),
kChkOrderST(ExchangeAccepted,            7, 6, 9,kTIME3) kOrdPri(R,203,kTIME8),
kChkOrder(PartFilled,PartFilled,         9, 8, 8,kTIME3) kOrdPri(R,203,kTIME8) kOrdCum(1,201,kTIME1),
kChkOrder(PartFilled,PartFilled,         8, 6, 6,kTIME3) kOrdPri(R,203,kTIME8) kOrdCum(3,605,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,   4, 4, 6,kTIME5) kOrdPri(R,203,kTIME8) kOrdCum(3,605,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,   6, 4, 4,kTIME4) kOrdPri(R,203,kTIME8) kOrdCum(3,605,kTIME2),
kChkOrder(UserCanceled,ExchangeAccepted, 4, 0, 0,kTIME6) kOrdPri(R,203,kTIME8) kOrdCum(3,605,kTIME2) kErrCode_0,
kChkOrder(UserCanceled,ExchangeNoLeaves, 0, 0, 0,kTIME9) kOrdPri(R,203,kTIME8) kOrdCum(3,605,kTIME2) kErrCode_NoLeaves,
   };
   RunTestList(core, "Out-of-order(NoNewQuery)", cstrTestList);
}
//--------------------------------------------------------------------------//
// 有收到新單回報, 及後續回報.
void TestNormalReport(TestCoreSP& core) {
   const char* cstrTestList[] = {
">AllocOrdNo=f-N-8610-A",
"+" kTwfRpt(ExchangeAccepted,Q,          4,4,  kTIME8) kPriLmt(R,203),
kChkOrder(Initialing,ExchangeAccepted,   4,4,0,kTIME8) kOrdPri(R,203,kTIME8),
// 減1成功.
"+1" kTwfRpt(ExchangeAccepted,C,         7,6,  kTIME3) kPriLmt(R,200),
kChkOrder(ReportPending,ExchangeAccepted,7,6,0,kTIME3) kOrdPri(R,203,kTIME8),
// 成交.
"+1." kTwfFil(1,1,201,kTIME1),
kChkOrder(ReportPending,PartFilled,      0,0,0,kTIME3) kOrdPri(R,203,kTIME8) kOrdCum(0,0,""),
"+1." kTwfFil(2,2,202,kTIME2),
kChkOrder(ReportPending,PartFilled,      0,0,0,kTIME3) kOrdPri(R,203,kTIME8) kOrdCum(0,0,""),
// 改價203成功.
"+1." kTwfRpt(ExchangeAccepted,P,        4,4,  kTIME5) kPriLmt(R,203),
kChkOrder(ReportPending,ExchangeAccepted,4,4,0,kTIME5) kOrdPri(R,203,kTIME8),
// -------------------------------------
// 新單回報
"+1." kTwfRpt(ExchangeAccepted,N,    10,10,   kTIME0) kPriLmt(R,200),
kChkOrderST(ExchangeAccepted,         0,10,10,kTIME0) kOrdPri(R,200,kTIME0),
kChkOrderST(ExchangeAccepted,         7, 6, 9,kTIME3) kOrdPri(R,203,kTIME8),
kChkOrder(PartFilled,PartFilled,      9, 8, 8,kTIME3) kOrdPri(R,203,kTIME8) kOrdCum(1,201,kTIME1),
kChkOrder(PartFilled,PartFilled,      8, 6, 6,kTIME3) kOrdPri(R,203,kTIME8) kOrdCum(3,605,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,4, 4, 6,kTIME5) kOrdPri(R,203,kTIME8) kOrdCum(3,605,kTIME2),
// -------------------------------------
// 減1回報成功.
"+1." kTwfRpt(ExchangeAccepted,C,     4, 3,   kTIMEB) kPriLmt(R,204),
kChkOrder(PartFilled,ExchangeAccepted,4, 3, 5,kTIMEB) kOrdPri(R,204,kTIMEB) kOrdCum(3,605,kTIME2),
// -------------------------------------
// 減2回報成功(補單).
"+1." kTwfRpt(ExchangeAccepted,C,     6, 4,   kTIME4) kPriLmt(R,203),
kChkOrder(PartFilled,ExchangeAccepted,6, 4, 3,kTIME4) kOrdPri(R,204,kTIMEB) kOrdCum(3,605,kTIME2),
// -------------------------------------
// 改價204成功(補單).
"+1." kTwfRpt(ExchangeAccepted,P,      4,4,   kTIMEA) kPriLmt(R,204),
kChkOrder(ReportStale,ExchangeAccepted,4,4, 3,kTIMEA) kOrdPri(R,204,kTIMEB) kOrdCum(3,605,kTIME2),
// 改價205成功.
"+1." kTwfRpt(ExchangeAccepted,P,      3,3,   kTIMEC) kPriLmt(R,205),
kChkOrder(PartFilled,ExchangeAccepted, 3,3, 3,kTIMEC) kOrdPri(R,205,kTIMEC) kOrdCum(3,605,kTIME2),
// -------------------------------------
// 刪單失敗, 等「LeavesQty==0」後處理.
"+1." kTwfRpt(ExchangeRejected,D,        0,0,  kTIMEE)                                             kErrCode_NoLeaves,
kChkOrder(ReportPending,ExchangeNoLeaves,0,0,3,kTIMEE) kOrdPri(R,205,kTIMEC) kOrdCum(3,605,kTIME2) kErrCode_NoLeaves,
// 刪單成功, 但會等「LeavesQty=BeforeQty=2」才處理.
"+1" kTwfRpt(ExchangeAccepted,D,         2,0,  kTIMED) kPriLmt(R,205),
kChkOrder(ReportPending,ExchangeAccepted,2,0,3,kTIMED) kOrdPri(R,205,kTIMEC) kOrdCum(3,605,kTIME2) kErrCode_0,
// -------------------------------------
// 減1回報成功(補單).
"+1." kTwfRpt(ExchangeAccepted,C,        6,5,  kTIME4) kPriLmt(R,203),
kChkOrder(PartFilled,ExchangeAccepted,   6,5,2,kTIME4) kOrdPri(R,205,kTIMEC) kOrdCum(3,605,kTIME2) kErrCode_0,
kChkOrder(UserCanceled,ExchangeAccepted, 2,0,0,kTIMED) kOrdPri(R,205,kTIMEC) kOrdCum(3,605,kTIME2) kErrCode_0,
kChkOrder(UserCanceled,ExchangeNoLeaves, 0,0,0,kTIMEE) kOrdPri(R,205,kTIMEC) kOrdCum(3,605,kTIME2) kErrCode_NoLeaves,
   };
   RunTestList(core, "Out-of-order(TestNormalReport)", cstrTestList);
}
//--------------------------------------------------------------------------//
// 先收到成交, 後收到減量回報(造成 LeavesQty=0).
void TestPartCancel(TestCoreSP& core) {
   const char* cstrTestList[] = {
">AllocOrdNo=f-N-8610-A",
// 新單回報.
"+" kTwfRpt(ExchangeAccepted,N,       10,10,   kTIME0) kPriLmt(R,200),
kChkOrderST(ExchangeAccepted,          0,10,10,kTIME0) kOrdPri(R,200,kTIME0),
// 成交.
"+1." kTwfFil(1,                                                                     1,201,kTIME5),
kChkOrderST(PartFilled,               10, 9, 9,kTIME0) kOrdPri(R,200,kTIME0) kOrdCum(1,201,kTIME5),
"+1." kTwfFil(2,                                                                     2,202,kTIME6),
kChkOrderST(PartFilled,                9, 7, 7,kTIME0) kOrdPri(R,200,kTIME0) kOrdCum(3,605,kTIME6),
// 減3(補單).
"+1." kTwfRpt(ExchangeAccepted,C,     10, 7,   kTIME1) kPriLmt(R,200),
kChkOrder(PartFilled,ExchangeAccepted,10, 7, 4,kTIME1) kOrdPri(R,200,kTIME0) kOrdCum(3,605,kTIME6),
// 減4(補單).
"+1." kTwfRpt(ExchangeAccepted,C,      7, 3,   kTIME2) kPriLmt(R,200),
kChkOrder(AsCanceled,ExchangeAccepted, 7, 3, 0,kTIME2) kOrdPri(R,200,kTIME0) kOrdCum(3,605,kTIME6),
   };
   RunTestList(core, "Out-of-order(PartCancel)", cstrTestList);
}
//--------------------------------------------------------------------------//
void TestErrCodeAct_ReNew(TestCoreSP& core) {
   const char* cstrTestList[] = {
kTwfNew,
kChkOrderNewSending,
// ----- ReNew 3 次: Resend 時的 BfQty, AfQty, LeavesQty 不可改變, 否則會造成風控重複計算.
"-L1=" kTwfR03(N,                     kTIME0)                   kErrCode_Rerun3,
kChkOrderST(Sending,         10,10,10,kTIME0) kOrdPri(R,200,"") kErrCode_Rerun3, // 新單重送, LeavesQty=AfterQty=BeforeQty; 這樣才不會造成風控重複計算.
"-L1=" kTwfR03(N,                     kTIME1)                   kErrCode_Rerun3,
kChkOrderST(Sending,         10,10,10,kTIME1) kOrdPri(R,200,"") kErrCode_Rerun3,
"-L1=" kTwfR03(N,                     kTIME2)                   kErrCode_Rerun3,
kChkOrderST(Sending,         10,10,10,kTIME2) kOrdPri(R,200,"") kErrCode_Rerun3,
"-L1=" kTwfR03(N,                     kTIME3)                   kErrCode_Rerun3,
kChkOrderST(ExchangeRejected,10, 0, 0,kTIME3) kOrdPri(R,200,"") kErrCode_Rerun3, // 最終失敗.

// ----- ReNew 過程中 Delete.
kTwfNew,
kChkOrderNewSending,
// New(Re:1)
"-L11=" kTwfR03(N,                    kTIME0)                   kErrCode_Rerun3,
kChkOrderST(Sending,         10,10,10,kTIME0) kOrdPri(R,200,"") kErrCode_Rerun3,

// 然後收到刪單要求.
"*1.TwfChg" kOrdKey "|Qty=0",
kChkOrderST(Sending,         10,10,10,kTIME0) kOrdPri(R,200,"") kErrCode_0,
// 刪單要求回報失敗:Rerun4 => Chg(Re:1).
"-L15=" kTwfR03(D,                    kTIME1)                   kErrCode_Rerun4_AtNewDone,
kChkOrderST(Sending,         10,10,10,kTIME1) kOrdPri(R,200,"") kErrCode_Rerun4_AtNewDone,

// New(Re:1) 的回報, 因 NewSending 之後收到 DeleteRequest, 所以不用再重送新單了.
"-L11=" kTwfR03(N,                    kTIME3)                   kErrCode_Rerun3,
kChkOrderST(ExchangeRejected,10, 0, 0,kTIME3) kOrdPri(R,200,"") kErrCode_Rerun3,
// Rerun Delete 的回報, 因新單失敗, 所以不用重送 Delete.
"-L15=" kTwfR03(D,                    kTIME5)                   kErrCode_Rerun4_AtNewDone,
kChkOrderST(ExchangeRejected, 0, 0, 0,kTIME5) kOrdPri(R,200,"") kErrCode_Rerun4_AtNewDone,
   };
   RunTestList(core, "ErrCodeAct.ReNew", cstrTestList);
}
//--------------------------------------------------------------------------//
void TestErrCodeAct_ReChg(TestCoreSP& core) {
   const char* cstrTestList[] = {
kTwfNew,
kChkOrderNewSending,
// ----- NewSending => Delete 失敗 Rerun2(AtNewDone=Y)
"*1.TwfChg" kOrdKey "|Qty=0",
kChkOrder(Sending,Sending,               10,10,10,"")     kOrdPri(R,200,""),
"-L3=" kTwfR03(D,                                 kTIME1)                   kErrCode_Rerun2_AtNewDone,
kChkOrder(Sending,Sending,               10,10,10,kTIME1) kOrdPri(R,200,"") kErrCode_Rerun2_AtNewDone,
"-L3=" kTwfR03(D,                                 kTIME2)                   kErrCode_Rerun2_AtNewDone,
kChkOrder(Sending,Sending,               10,10,10,kTIME2) kOrdPri(R,200,"") kErrCode_Rerun2_AtNewDone,
"-L3=" kTwfR03(D,                                 kTIME3)                   kErrCode_Rerun2_AtNewDone,
kChkOrder(ReportPending,ExchangeRejected,10,10,10,kTIME3) kOrdPri(R,200,"") kErrCode_Rerun2_AtNewDone,

// ----- 刪單.
"*1.TwfChg" kOrdKey "|Qty=0",
kChkOrder(Sending,Sending,                  10,10,10,kTIME3) kOrdPri(R,200,"") kErrCode_0,
// ----- 刪單(NoRerun)一般失敗 => 等新單回報後處理.
"-2=" kTwfR03(D,                                     kTIME4)                   kErrCode_NoRerun,
kChkOrder(ReportPending,ExchangeRejected,   10,10,10,kTIME4) kOrdPri(R,200,"") kErrCode_NoRerun,

// ----- New.Accepted.
"-L1=" kTwfRpt(ExchangeAccepted,N,          10,10,   kTIME5) kPriLmt(R,200),
kChkOrderST(ExchangeAccepted,               10,10,10,kTIME5) kOrdPri(R,200,kTIME5),
// 新單成功, 處理 Rerun2(AtNewDone=Y) 的失敗回報: Rerun.
kChkOrder(ExchangeAccepted,Sending,         10,10,10,kTIME3) kOrdPri(R,200,kTIME5) kErrCode_Rerun2_AtNewDone,
// 新單成功, 處理 (NoRerun)一般失敗 的 ReportPending 回報.
kChkOrder(ExchangeAccepted,ExchangeRejected,10,10,10,kTIME4) kOrdPri(R,200,kTIME5) kErrCode_NoRerun,
// Rerun2(AtNewDone=Y) 之後, 又收到 Rerun2 的失敗, 因為有 AtNewDone=Y, 所以不會再 Rerun.
"-L3=" kTwfR03(D,                                    kTIME6)                       kErrCode_Rerun2_AtNewDone,
kChkOrder(ExchangeAccepted,ExchangeRejected,10,10,10,kTIME6) kOrdPri(R,200,kTIME5) kErrCode_Rerun2_AtNewDone,

// ----- New.ExchangeCanceled.
"-L1=" kTwfRpt(ExchangeCanceled,N,          10, 0,   kTIME9) kPriLmt(R,200),
kChkOrderST(ExchangeCanceled,               10, 0, 0,kTIME9) kOrdPri(R,200,kTIME9),
   };
   RunTestList(core, "ErrCodeAct.ReChg", cstrTestList);
}
void TestErrCodeAct_ReChg2(TestCoreSP& core) {
   const char* cstrTestList[] = {
kTwfNew,
kChkOrderNewSending,
// ----- NewSending => Delete 失敗重送4次, 然後失敗.
"*1.TwfChg" kOrdKey "|Qty=0",
kChkOrder(Sending,Sending,                  10,10,10,"")     kOrdPri(R,200,"") kErrCode_0,
// ----- NewSending => Delete 失敗重送1次 => New.Accepted 之後應重送一次.
"*1.TwfChg" kOrdKey "|Qty=0",
kChkOrder(Sending,Sending,                  10,10,10,"")     kOrdPri(R,200,"") kErrCode_0,

"-L3=" kTwfR03(D,                                    kTIME1)                   kErrCode_Rerun4_AtNewDone,
kChkOrder(Sending,Sending,                  10,10,10,kTIME1) kOrdPri(R,200,"") kErrCode_Rerun4_AtNewDone,

"-L3=" kTwfR03(D,                                    kTIME2)                   kErrCode_Rerun4_AtNewDone,
kChkOrder(Sending,Sending,                  10,10,10,kTIME2) kOrdPri(R,200,"") kErrCode_Rerun4_AtNewDone,
"-L3=" kTwfR03(D,                                    kTIME3)                   kErrCode_Rerun4_AtNewDone,
kChkOrder(Sending,Sending,                  10,10,10,kTIME3) kOrdPri(R,200,"") kErrCode_Rerun4_AtNewDone,
"-L3=" kTwfR03(D,                                    kTIME4)                   kErrCode_Rerun4_AtNewDone,
kChkOrder(Sending,Sending,                  10,10,10,kTIME4) kOrdPri(R,200,"") kErrCode_Rerun4_AtNewDone,
"-L3=" kTwfR03(D,                                    kTIME5)                   kErrCode_Rerun4_AtNewDone,
kChkOrder(ReportPending,ExchangeRejected,   10,10,10,kTIME5) kOrdPri(R,200,"") kErrCode_Rerun4_AtNewDone,

"-L5=" kTwfR03(D,                                    kTIME6)                   kErrCode_Rerun4_AtNewDone,
kChkOrder(Sending,Sending,                  10,10,10,kTIME6) kOrdPri(R,200,"") kErrCode_Rerun4_AtNewDone,

// ----- New.Accepted.
"-L1=" kTwfRpt(ExchangeAccepted,N,          10,10,   kTIME8) kPriLmt(R,200),
kChkOrderST(ExchangeAccepted,               10,10,10,kTIME8) kOrdPri(R,200,kTIME8) kErrCode_0,
// 新單成功後再送一次刪單.
kChkOrder(ExchangeAccepted,Sending,         10,10,10,kTIME5) kOrdPri(R,200,kTIME8) kErrCode_Rerun4_AtNewDone,
"-L3=" kTwfR03(D,                                    kTIMEA)                       kErrCode_Rerun4_AtNewDone,
kChkOrder(ExchangeAccepted,ExchangeRejected,10,10,10,kTIMEA) kOrdPri(R,200,kTIME8) kErrCode_Rerun4_AtNewDone,
// Ln#5 的刪單回報 => 再送一次刪單.
"-L5=" kTwfR03(D,                                    kTIMEB)                       kErrCode_Rerun4_AtNewDone,
kChkOrder(ExchangeAccepted,Sending,         10,10,10,kTIMEB) kOrdPri(R,200,kTIME8) kErrCode_Rerun4_AtNewDone,
"-L5=" kTwfR03(D,                                    kTIMEC)                       kErrCode_Rerun4_AtNewDone,
kChkOrder(ExchangeAccepted,ExchangeRejected,10,10,10,kTIMEC) kOrdPri(R,200,kTIME8) kErrCode_Rerun4_AtNewDone,
   };
   RunTestList(core, "ErrCodeAct.ReChg2", cstrTestList);
}
//--------------------------------------------------------------------------//
void TestCombOrder(TestCoreSP& core) {
#undef  kSYMID
#define kSYMID    kSYMID_H kSYMID_1 "/" kSYMID_2
   const char* cstrTestList[] = {
kTwfNew,
kChkOrderNewSending,
"-2." kTwfRpt(ExchangeAccepted,N,     10,10,   kTIME0) kPriLmt(R,200),
kChkOrderST(ExchangeAccepted,         10,10,10,kTIME0) kOrdPri(R,200,kTIME0),
"+1." kTwfFil2(1,                                                                    1,201,210,kTIME1),
kChkOrder(PartFilled,PartFilled,      10, 9, 9,kTIME0) kOrdPri(R,200,kTIME0) kOrdCum(1,  9,    kTIME1),
"+1." kTwfFil2(2,                                                                    2,202,210,kTIME2),
kChkOrder(PartFilled,PartFilled,       9, 7, 7,kTIME0) kOrdPri(R,200,kTIME0) kOrdCum(3, 25,    kTIME2),
   };
   RunTestList(core, "Combo-order", cstrTestList);
#undef  kSYMID
#define kSYMID    kSYMID_H kSYMID_1
}
//--------------------------------------------------------------------------//
int main(int argc, const char* argv[]) {
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif

   InitReportUT(argc, argv);
   fon9::AutoPrintTestInfo utinfo{"OmsTwfReport"};
   TestCoreSP core;

   TestCombOrder(core);
   utinfo.PrintSplitter();

   TestNormalOrder(core);
   TestSpecialFilled(core);
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

   TestErrCodeAct_ReNew(core);
   utinfo.PrintSplitter();

   TestErrCodeAct_ReChg(core);
   utinfo.PrintSplitter();

   TestErrCodeAct_ReChg2(core);
   utinfo.PrintSplitter();
}
