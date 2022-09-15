// \file f9omstw/OmsReport_UT.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsReport_RunTest.hpp"
//--------------------------------------------------------------------------//
template <class BaseT>
struct ReportQtyOddLot : public BaseT {
   fon9_NON_COPY_NON_MOVE(ReportQtyOddLot);
   static f9omstw::OmsRequestSP MakeReportIn(f9omstw::OmsRequestFactory& creator, f9fmkt_RxKind reqKind) {
      auto retval = BaseT::MakeReportIn(creator, reqKind);
      static_cast<BaseT*>(retval.get())->QtyStyle_ = f9omstw::OmsReportQtyStyle::OddLot;
      return retval;
   }
};
using TwsFilled = ReportQtyOddLot<f9omstw::OmsTwsFilled>;
using TwsReport = ReportQtyOddLot<f9omstw::OmsTwsReport>;
using TwsFilledFactory = f9omstw::OmsReportFactoryT<TwsFilled>;
using TwsReportFactory = f9omstw::OmsReportFactoryT<TwsReport>;
//--------------------------------------------------------------------------//
void InitTestCore(TestCore& core) {
   auto  ordfac = core.Owner_->OrderFactoryPark().GetFactory("TwsOrd");
   core.Owner_->SetRequestFactoryPark(new f9omstw::OmsRequestFactoryPark(
      new OmsTwsRequestIniFactory("TwsNew", ordfac,
                                  f9omstw::OmsRequestRunStepSP{new UomsTwsIniRiskCheck{
      f9omstw::OmsRequestRunStepSP{new UomsTwsExgSender}}}),
      new OmsTwsRequestChgFactory("TwsChg", f9omstw::OmsRequestRunStepSP{new UomsTwsExgSender}),

      new TwsFilledFactory("TwsFil", ordfac),
      new TwsReportFactory("TwsRpt", ordfac)
      ));
   core.Owner_->ReloadErrCodeAct(
      "$x=x\n" // 用 $ 開頭, 表示此處提供的是設定內容, 不是檔案名稱.
      "20050:St=ExchangeNoLeavesQty\n"
      "20002:Rerun=3|Memo=TIME IS EARLY\n"
      "20005:Rerun=4|AtNewDone=Y|UseNewLine=Y|Memo=ORDER NOT FOUND\n"
      "30005:Rerun=2|NewSending=1.5|AtNewDone=Y|Memo=ORDER NOT FOUND\n"
   );
   core.OpenReload(gArgc, gArgv, "OmsReport_UT.log");

#define kErrCode_0                  "|ErrCode=0"
#define kErrCode_NoLeaves           "|ErrCode=20050"
#define kErrCode_Rerun2_AtNewDone   "|ErrCode=30005"
#define kErrCode_Rerun3             "|ErrCode=20002"
#define kErrCode_Rerun4_AtNewDone   "|ErrCode=20005"
#define kErrCode_NoRerun            "|ErrCode=20001"
}
//--------------------------------------------------------------------------//
#define kTIME0    "09:00:00"
#define kTIME1    "09:00:00.1"
#define kTIME2    "09:00:00.2"
#define kTIME3    "09:00:00.3"
#define kTIME4    "09:00:00.4"
#define kTIME5    "09:00:00.5"
#define kTIME6    "09:00:00.6"
#define kTIME7    "09:00:00.7"
#define kTIME8    "09:00:00.8"
#define kTIME9    "09:00:00.9"
#define kTIMEA    "09:00:01"
#define kTIMEB    "09:00:02"
#define kTIMEC    "09:00:03"
#define kTIMED    "09:00:04"
#define kTIMEE    "09:00:05"
//--------------------------------------------------------------------------//
#define kOrdKey            "|Market=T|SessionId=N|BrkId=8610"
#define kReqBase           kOrdKey "|IvacNo=10|Symbol=2317|Side=B"
#define kPriLmt(pri)       "|Pri=" fon9_CTXTOCSTR(pri) "|PriType=L"
//#define kPriMkt            "|Pri=|PriType=M"

// [0]: '+'=Report (占用序號).
// [0]: '-'=Report (不占用序號).
// [0]: '*'=Request 透過 core.MoveToCore(OmsRequestRunner{}) 執行;
// [0]: '/'=檢查 OrderRaw 是否符合預期. (占用序號).
// [1..]: 參考資料「'L' + LineNo」或「序號-N」

// 新單要求測試內容:Pri=200,Qty=10000
#define kTwsNew   "* TwsNew" kReqBase "|Qty=10000" kPriLmt(200)
// 新單要求 core.MoveToCore(OmsRequestRunner{}) 的結果.
#define kChkOrderNewSending \
        kChkOrderST(Sending,0,10000,10000,/*No exgTime*/) kOrdPri(200,/*No LastPriTime*/)

// -----
#define kChkOrder(ordSt,reqSt,bfQty,afQty,leaves,exgTime) \
        "/ TwsOrd" kOrdReqST(kVAL_##ordSt, kVAL_##reqSt)  \
                   kOrdQtys(bfQty,afQty,leaves)           \
                   "|LastExgTime=" exgTime
#define kChkOrderST(st,bfQty,afQty,leaves,exgTime) \
        kChkOrder(st,st,bfQty,afQty,leaves,exgTime)

#define kOrdPri(pri,priTime) \
        "|LastPriTime=" priTime "|LastPri=" fon9_CTXTOCSTR(pri) "|LastPriType=L"
// -----
#define kTwsRpt(st,kind,bfQty,afQty,exgTime) \
        "TwsRpt" kRptVST(st) kReqBase "|Kind=" #kind \
                 "|BeforeQty=" #bfQty "|Qty=" #afQty "|ExgTime=" exgTime

#define kTwsFil(matKey,qty,pri,tm) \
        "TwsFil" kReqBase "|MatchKey=" #matKey "|Qty=" #qty "|Pri=" #pri "|Time=" tm
//--------------------------------------------------------------------------//
// 一般正常順序下單回報.
void TestNormalOrder(TestCoreSP& core) {
   const char* cstrTestList[] = {
kTwsNew,
kChkOrderNewSending,
"-2." kTwsRpt(ExchangeAccepted,N,            10000, 10000,       kTIME0) kPriLmt(200),
kChkOrder(ExchangeAccepted,ExchangeAccepted, 10000, 10000, 10000,kTIME0) kOrdPri(200,kTIME0),
// 成交.
"+1." kTwsFil(1,                                                                            1000,201,   kTIME1),
kChkOrder(PartFilled,PartFilled,      10000, 9000, 9000,kTIME0) kOrdPri(200,kTIME0) kOrdCum(1000,201000,kTIME1),
"+1." kTwsFil(2,                                                                            2000,202,   kTIME2),
kChkOrder(PartFilled,PartFilled,       9000, 7000, 7000,kTIME0) kOrdPri(200,kTIME0) kOrdCum(3000,605000,kTIME2),
// 減3
"*1.TwsChg" kOrdKey "|Qty=-3000",
kChkOrder(PartFilled,Sending,          7000, 7000, 7000,kTIME0) kOrdPri(200,kTIME0) kOrdCum(3000,605000,kTIME2),
"-2." kTwsRpt(ExchangeAccepted,C,      7000, 4000,      kTIME3),
kChkOrder(PartFilled,ExchangeAccepted, 7000, 4000, 4000,kTIME3) kOrdPri(200,kTIME0) kOrdCum(3000,605000,kTIME2),
// 改價$203
"*1.TwsChg" kOrdKey kPriLmt(203),
kChkOrder(PartFilled,Sending,          4000, 4000, 4000,kTIME3) kOrdPri(200,kTIME0) kOrdCum(3000,605000,kTIME2),
"-2." kTwsRpt(ExchangeAccepted,P,      4000, 4000,      kTIME4) kPriLmt(203),
kChkOrder(PartFilled,ExchangeAccepted, 4000, 4000, 4000,kTIME4) kOrdPri(203,kTIME4) kOrdCum(3000,605000,kTIME2),
// 成交4
"+1." kTwsFil(5,                                                                            4000,204,    kTIME5),
kChkOrder(FullFilled,PartFilled,       4000,    0,    0,kTIME4) kOrdPri(203,kTIME4) kOrdCum(7000,1421000,kTIME5),
// 刪單
"*1.TwsChg" kOrdKey "|Qty=0",
kChkOrder(FullFilled,Sending,          0, 0, 0,kTIME4) kOrdPri(203,kTIME4) kOrdCum(7000,1421000,kTIME5),
"-2." kTwsRpt(ExchangeRejected,D,      0, 0,   kTIME6)                                                  kErrCode_NoLeaves,
kChkOrder(FullFilled,ExchangeNoLeaves, 0, 0, 0,kTIME6) kOrdPri(203,kTIME4) kOrdCum(7000,1421000,kTIME5) kErrCode_NoLeaves,
// - 新單失敗 -----
kTwsNew,
kChkOrderNewSending,
"-2." kTwsRpt(ExchangeRejected,N,            10000, 0,   kTIME0),
kChkOrder(ExchangeRejected,ExchangeRejected, 10000, 0, 0,kTIME0) kOrdPri(200,""),
kTwsNew,
kChkOrderNewSending,
"-2." kTwsRpt(ExchangeRejected,N,                 , 0,   kTIME0), // 不填 "BeforeQty="
kChkOrder(ExchangeRejected,ExchangeRejected, 10000, 0, 0,kTIME0) kOrdPri(200,""),
   };
   RunTestList(core, "Normal-order", cstrTestList);
}
//--------------------------------------------------------------------------//
// 下單後,亂序回報.
void TestOutofOrder(TestCoreSP& core) {
   const char* cstrTestList[] = {
kTwsNew,
kChkOrderNewSending,
// 新單尚未回報, 先收到「刪、改、查、成交」 => 透過 OmsOrderRaw.UpdateOrderSt_ = ReportPending 保留.
"+1." kTwsFil(1,1000,201,kTIME1),
kChkOrder(ReportPending,PartFilled,      10000,10000,10000,"")     kOrdPri(200,"") kOrdCum(0,0,""),
"+1." kTwsFil(2,2000,202,kTIME2),
kChkOrder(ReportPending,PartFilled,      10000,10000,10000,"")     kOrdPri(200,"") kOrdCum(0,0,""),
// 減1成功.
"*1.TwsChg" kOrdKey "|Qty=-1000",
kChkOrder(Sending,Sending,               10000,10000,10000,"")     kOrdPri(200,""),
"-2." kTwsRpt(ExchangeAccepted,C,         7000, 6000,      kTIME3) kPriLmt(200),
kChkOrder(ReportPending,ExchangeAccepted, 7000, 6000,10000,kTIME3) kOrdPri(200,kTIME3),
// ** 減2沒回報(Ln#11)
"*1.TwsChg" kOrdKey "|Qty=-2000",
kChkOrder(Sending,Sending,               10000,10000,10000,kTIME3) kOrdPri(200,kTIME3),
// 改價$203成功
"*1.TwsChg" kOrdKey kPriLmt(203),
kChkOrder(Sending,Sending,               10000,10000,10000,kTIME3) kOrdPri(200,kTIME3),
"-2." kTwsRpt(ExchangeAccepted,P,         4000, 4000,      kTIME5) kPriLmt(203),
kChkOrder(ReportPending,ExchangeAccepted, 4000, 4000,10000,kTIME5) kOrdPri(203,kTIME5),
// >> 刪單未回報(Ln#17).
"*1.TwsChg" kOrdKey "|Qty=0",
kChkOrder(Sending,Sending,               10000,10000,10000,kTIME5) kOrdPri(203,kTIME5),
// 刪單成功, 但會等「減2成功」後才處理.
"*1.TwsChg" kOrdKey "|Qty=0",
kChkOrder(Sending,Sending,               10000,10000,10000,kTIME5) kOrdPri(203,kTIME5),
"-2." kTwsRpt(ExchangeAccepted,D,         4000,    0,      kTIME6) kPriLmt(203),
kChkOrder(ReportPending,ExchangeAccepted, 4000,    0,10000,kTIME6) kOrdPri(203,kTIME5),
// >> 刪單失敗(接續 Ln#17), 等刪單成功後處理.
"-L17." kTwsRpt(ExchangeRejected,D,          0,    0,      kTIME9)                     kErrCode_NoLeaves,
kChkOrder(ReportPending,ExchangeNoLeaves,    0,    0,10000,kTIME9) kOrdPri(203,kTIME5) kErrCode_NoLeaves,
// ** 減2回報成功(接續 Ln#11).
"-L11." kTwsRpt(ExchangeAccepted,C,       6000, 4000,      kTIME4) kPriLmt(200),
kChkOrder(ReportPending,ExchangeAccepted, 6000, 4000,10000,kTIME4) kOrdPri(203,kTIME5) kErrCode_0,
// -------------------------------------
// 新單回報(接續 Ln#1) => 處理等候中的回報.
"-L1." kTwsRpt(ExchangeAccepted,N,     10000,10000,      kTIME0) kPriLmt(200),
kChkOrderST(ExchangeAccepted,          10000,10000,10000,kTIME0) kOrdPri(200,kTIME0) kErrCode_0,
kChkOrder(PartFilled,PartFilled,       10000, 9000, 9000,kTIME0) kOrdPri(200,kTIME0) kOrdCum(1000,201000,kTIME1),
kChkOrder(PartFilled,PartFilled,        9000, 7000, 7000,kTIME0) kOrdPri(200,kTIME0) kOrdCum(3000,605000,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,  7000, 6000, 6000,kTIME3) kOrdPri(200,kTIME0) kOrdCum(3000,605000,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,  4000, 4000, 6000,kTIME5) kOrdPri(203,kTIME5) kOrdCum(3000,605000,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,  6000, 4000, 4000,kTIME4) kOrdPri(203,kTIME5) kOrdCum(3000,605000,kTIME2),
kChkOrder(UserCanceled,ExchangeAccepted,4000,    0,    0,kTIME6) kOrdPri(203,kTIME5) kOrdCum(3000,605000,kTIME2) kErrCode_0,
kChkOrder(UserCanceled,ExchangeNoLeaves,   0,    0,    0,kTIME9) kOrdPri(203,kTIME5) kOrdCum(3000,605000,kTIME2) kErrCode_NoLeaves,
   };
   RunTestList(core, "Out-of-order", cstrTestList);
}
//--------------------------------------------------------------------------//
// 委託不存在(尚未收到新單回報), 先收到成交及其他回報.
void TestNoNewFilled(TestCoreSP& core) {
   const char* cstrTestList[] = {
// 先收到刪改查成交 => 透過 OmsOrderRaw.UpdateOrderSt_ = ReportPending 保留.
">AllocOrdNo=T-N-8610-A",
"+"   kTwsFil(1,1000,201,kTIME1),
kChkOrder(ReportPending,PartFilled,0,0,0,"") kOrdCum(0,0,""),
"+1." kTwsFil(2,2000,202,kTIME2),
kChkOrder(ReportPending,PartFilled,0,0,0,"") kOrdCum(0,0,""),
// 減1成功.
"+1." kTwsRpt(ExchangeAccepted,C,        7000,6000,  kTIME3) kPriLmt(200),
kChkOrder(ReportPending,ExchangeAccepted,7000,6000,0,kTIME3) kOrdPri(200,kTIME3),
// 改價203成功,但「遺漏2」
"+1." kTwsRpt(ExchangeAccepted,P,        4000,4000,  kTIME5) kPriLmt(203),
kChkOrder(ReportPending,ExchangeAccepted,4000,4000,0,kTIME5) kOrdPri(203,kTIME5),
// 刪單失敗, 等「LeavesQty==0」後處理.
"+1." kTwsRpt(ExchangeRejected,D,           0,   0,  kTIME9)                     kErrCode_NoLeaves,
kChkOrder(ReportPending,ExchangeNoLeaves,   0,   0,0,kTIME9) kOrdPri(203,kTIME5) kErrCode_NoLeaves,
// 刪單成功, 但會等「遺漏2補齊」後才處理.
"+1." kTwsRpt(ExchangeAccepted,D,        4000,   0,  kTIME6) kPriLmt(203),
kChkOrder(ReportPending,ExchangeAccepted,4000,   0,0,kTIME6) kOrdPri(203,kTIME5) kErrCode_0,
// 減2回報成功.
"+1." kTwsRpt(ExchangeAccepted,C,        6000,4000,  kTIME4) kPriLmt(200),
kChkOrder(ReportPending,ExchangeAccepted,6000,4000,0,kTIME4) kOrdPri(203,kTIME5),
// -------------------------------------
// 新單回報
"+1." kTwsRpt(ExchangeAccepted,N,      10000,10000,      kTIME0) kPriLmt(200),
kChkOrderST(ExchangeAccepted,              0,10000,10000,kTIME0) kOrdPri(200,kTIME0),
kChkOrder(PartFilled,PartFilled,       10000, 9000, 9000,kTIME0) kOrdPri(200,kTIME0) kOrdCum(1000,201000,kTIME1),
kChkOrder(PartFilled,PartFilled,        9000, 7000, 7000,kTIME0) kOrdPri(200,kTIME0) kOrdCum(3000,605000,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,  7000, 6000, 6000,kTIME3) kOrdPri(200,kTIME0) kOrdCum(3000,605000,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,  4000, 4000, 6000,kTIME5) kOrdPri(203,kTIME5) kOrdCum(3000,605000,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,  6000, 4000, 4000,kTIME4) kOrdPri(203,kTIME5) kOrdCum(3000,605000,kTIME2),
kChkOrder(UserCanceled,ExchangeAccepted,4000,    0,    0,kTIME6) kOrdPri(203,kTIME5) kOrdCum(3000,605000,kTIME2) kErrCode_0,
kChkOrder(UserCanceled,ExchangeNoLeaves,   0,    0,    0,kTIME9) kOrdPri(203,kTIME5) kOrdCum(3000,605000,kTIME2) kErrCode_NoLeaves,
   };
   RunTestList(core, "Out-of-order(NoNewFilled)", cstrTestList);
}
//--------------------------------------------------------------------------//
// 委託不存在(尚未收到新單回報), 先收到ChgQty及其他回報.
void TestNoNewChgQty(TestCoreSP& core) {
   const char* cstrTestList[] = {
">AllocOrdNo=T-N-8610-A",
// 減1成功.
"+" kTwsRpt(ExchangeAccepted,C,          7000,6000,  kTIME3) kPriLmt(200),
kChkOrder(ReportPending,ExchangeAccepted,7000,6000,0,kTIME3) kOrdPri(200,kTIME3),
// 成交.
"+1." kTwsFil(1,1000,201,kTIME1),
kChkOrder(ReportPending,PartFilled,         0,   0,0,kTIME3) kOrdPri(200,kTIME3) kOrdCum(0,0,""),
"+1." kTwsFil(2,2000,202,kTIME2),
kChkOrder(ReportPending,PartFilled,         0,   0,0,kTIME3) kOrdPri(200,kTIME3) kOrdCum(0,0,""),
// 改價203成功,但「遺漏2」
"+1." kTwsRpt(ExchangeAccepted,P,        4000,4000,  kTIME5) kPriLmt(203),
kChkOrder(ReportPending,ExchangeAccepted,4000,4000,0,kTIME5) kOrdPri(203,kTIME5),
// 刪單失敗, 等「LeavesQty==0」後處理.
"+1." kTwsRpt(ExchangeRejected,D,           0,   0,  kTIME9)                     kErrCode_NoLeaves,
kChkOrder(ReportPending,ExchangeNoLeaves,   0,   0,0,kTIME9) kOrdPri(203,kTIME5) kErrCode_NoLeaves,
// 刪單成功, 但會等「遺漏2補齊」後才處理.
"+1." kTwsRpt(ExchangeAccepted,D,        4000,   0,  kTIME6) kPriLmt(203),
kChkOrder(ReportPending,ExchangeAccepted,4000,   0,0,kTIME6) kOrdPri(203,kTIME5) kErrCode_0,
// 減2回報成功.
"+1." kTwsRpt(ExchangeAccepted,C,        6000,4000,  kTIME4) kPriLmt(200),
kChkOrder(ReportPending,ExchangeAccepted,6000,4000,0,kTIME4) kOrdPri(203,kTIME5) kErrCode_0,
// -------------------------------------
// 新單回報
"+1." kTwsRpt(ExchangeAccepted,N,      10000,10000,      kTIME0) kPriLmt(200),
kChkOrderST(ExchangeAccepted,              0,10000,10000,kTIME0) kOrdPri(200,kTIME0),
kChkOrderST(ExchangeAccepted,           7000, 6000, 9000,kTIME3) kOrdPri(200,kTIME0),
kChkOrder(PartFilled,PartFilled,        9000, 8000, 8000,kTIME3) kOrdPri(200,kTIME0) kOrdCum(1000,201000,kTIME1),
kChkOrder(PartFilled,PartFilled,        8000, 6000, 6000,kTIME3) kOrdPri(200,kTIME0) kOrdCum(3000,605000,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,  4000, 4000, 6000,kTIME5) kOrdPri(203,kTIME5) kOrdCum(3000,605000,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,  6000, 4000, 4000,kTIME4) kOrdPri(203,kTIME5) kOrdCum(3000,605000,kTIME2),
kChkOrder(UserCanceled,ExchangeAccepted,4000,    0,    0,kTIME6) kOrdPri(203,kTIME5) kOrdCum(3000,605000,kTIME2) kErrCode_0,
kChkOrder(UserCanceled,ExchangeNoLeaves,   0,    0,    0,kTIME9) kOrdPri(203,kTIME5) kOrdCum(3000,605000,kTIME2) kErrCode_NoLeaves,
   };
   RunTestList(core, "Out-of-order(NoNewChgQty)", cstrTestList);
}
//--------------------------------------------------------------------------//
// 委託不存在(尚未收到新單回報), 先收到Delete及其他回報.
void TestNoNewDelete(TestCoreSP& core) {
   const char* cstrTestList[] = {
">AllocOrdNo=T-N-8610-A",
// 刪單成功(↑分配委託書號), 但會等「LeavesQty=BeforeQty=4」才處理.
"+" kTwsRpt(ExchangeAccepted,D,          4000,   0,  kTIME6) kPriLmt(203),
kChkOrder(ReportPending,ExchangeAccepted,4000,   0,0,kTIME6) kOrdPri(203,kTIME6),
// 減1成功.
"+1" kTwsRpt(ExchangeAccepted,C,         7000,6000,  kTIME3) kPriLmt(200),
kChkOrder(ReportPending,ExchangeAccepted,7000,6000,0,kTIME3) kOrdPri(203,kTIME6),
// 成交.
"+1." kTwsFil(1,1000,201,kTIME1),
kChkOrder(ReportPending,PartFilled,         0,   0,0,kTIME3) kOrdPri(203,kTIME6) kOrdCum(0,0,""),
"+1." kTwsFil(2,2000,202,kTIME2),
kChkOrder(ReportPending,PartFilled,         0,   0,0,kTIME3) kOrdPri(203,kTIME6) kOrdCum(0,0,""),
// 改價203成功.
"+1." kTwsRpt(ExchangeAccepted,P,        4000,4000,  kTIME5) kPriLmt(203),
kChkOrder(ReportPending,ExchangeAccepted,4000,4000,0,kTIME5) kOrdPri(203,kTIME6) kOrdCum(0,0,""),
// 刪單失敗, 等「LeavesQty==0」後處理.
"+1." kTwsRpt(ExchangeRejected,D,           0,   0,  kTIME9)                     kErrCode_NoLeaves,
kChkOrder(ReportPending,ExchangeNoLeaves,   0,   0,0,kTIME9) kOrdPri(203,kTIME6) kErrCode_NoLeaves,
// 減2回報成功.
"+1." kTwsRpt(ExchangeAccepted,C,        6000,4000,  kTIME4) kPriLmt(200),
kChkOrder(ReportPending,ExchangeAccepted,6000,4000,0,kTIME4) kOrdPri(203,kTIME6) kErrCode_0,
// -------------------------------------
// 新單回報
"+1." kTwsRpt(ExchangeAccepted,N,      10000,10000,      kTIME0) kPriLmt(200),
kChkOrderST(ExchangeAccepted,              0,10000,10000,kTIME0) kOrdPri(200,kTIME0),
kChkOrderST(ExchangeAccepted,           7000, 6000, 9000,kTIME3) kOrdPri(203,kTIME6),
kChkOrder(PartFilled,PartFilled,        9000, 8000, 8000,kTIME3) kOrdPri(203,kTIME6) kOrdCum(1000,201000,kTIME1),
kChkOrder(PartFilled,PartFilled,        8000, 6000, 6000,kTIME3) kOrdPri(203,kTIME6) kOrdCum(3000,605000,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,  4000, 4000, 6000,kTIME5) kOrdPri(203,kTIME6) kOrdCum(3000,605000,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,  6000, 4000, 4000,kTIME4) kOrdPri(203,kTIME6) kOrdCum(3000,605000,kTIME2),
kChkOrder(UserCanceled,ExchangeAccepted,4000,    0,    0,kTIME6) kOrdPri(203,kTIME6) kOrdCum(3000,605000,kTIME2) kErrCode_0,
kChkOrder(UserCanceled,ExchangeNoLeaves,   0,    0,    0,kTIME9) kOrdPri(203,kTIME6) kOrdCum(3000,605000,kTIME2) kErrCode_NoLeaves,
   };
   RunTestList(core, "Out-of-order(NoNewDelete)", cstrTestList);
}
//--------------------------------------------------------------------------//
// 委託不存在(尚未收到新單回報), 先收到Query及其他回報.
void TestNoNewQuery(TestCoreSP& core) {
   const char* cstrTestList[] = {
">AllocOrdNo=T-N-8610-A",
"+" kTwsRpt(ExchangeAccepted,Q,          4000,4000,  kTIME8) kPriLmt(203),
kChkOrder(Initialing,ExchangeAccepted,   4000,4000,0,kTIME8) kOrdPri(203,kTIME8),
// 刪單成功, 但會等「LeavesQty=BeforeQty=4」才處理.
"+1" kTwsRpt(ExchangeAccepted,D,         4000,   0,  kTIME6) kPriLmt(203),
kChkOrder(ReportPending,ExchangeAccepted,4000,   0,0,kTIME6) kOrdPri(203,kTIME8),
// 減1成功.
"+1" kTwsRpt(ExchangeAccepted,C,         7000,6000,  kTIME3) kPriLmt(200),
kChkOrder(ReportPending,ExchangeAccepted,7000,6000,0,kTIME3) kOrdPri(203,kTIME8),
// 成交.
"+1." kTwsFil(1,1000,201,kTIME1),
kChkOrder(ReportPending,PartFilled,         0,   0,0,kTIME3) kOrdPri(203,kTIME8) kOrdCum(0,0,""),
"+1." kTwsFil(2,2000,202,kTIME2),
kChkOrder(ReportPending,PartFilled,         0,   0,0,kTIME3) kOrdPri(203,kTIME8) kOrdCum(0,0,""),
// 改價203成功.
"+1." kTwsRpt(ExchangeAccepted,P,        4000,4000,  kTIME5) kPriLmt(203),
kChkOrder(ReportPending,ExchangeAccepted,4000,4000,0,kTIME5) kOrdPri(203,kTIME8),
// 刪單失敗, 等「LeavesQty==0」後處理.
"+1." kTwsRpt(ExchangeRejected,D,           0,   0,  kTIME9)                     kErrCode_NoLeaves,
kChkOrder(ReportPending,ExchangeNoLeaves,   0,   0,0,kTIME9) kOrdPri(203,kTIME8) kErrCode_NoLeaves,
// 減2回報成功.
"+1." kTwsRpt(ExchangeAccepted,C,        6000,4000,  kTIME4) kPriLmt(200),
kChkOrder(ReportPending,ExchangeAccepted,6000,4000,0,kTIME4) kOrdPri(203,kTIME8) kErrCode_0,
// -------------------------------------
// 新單回報
"+1." kTwsRpt(ExchangeAccepted,N,      10000,10000,      kTIME0) kPriLmt(200),
kChkOrderST(ExchangeAccepted,              0,10000,10000,kTIME0) kOrdPri(200,kTIME0),
kChkOrderST(ExchangeAccepted,           7000, 6000, 9000,kTIME3) kOrdPri(203,kTIME8),
kChkOrder(PartFilled,PartFilled,        9000, 8000, 8000,kTIME3) kOrdPri(203,kTIME8) kOrdCum(1000,201000,kTIME1),
kChkOrder(PartFilled,PartFilled,        8000, 6000, 6000,kTIME3) kOrdPri(203,kTIME8) kOrdCum(3000,605000,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,  4000, 4000, 6000,kTIME5) kOrdPri(203,kTIME8) kOrdCum(3000,605000,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,  6000, 4000, 4000,kTIME4) kOrdPri(203,kTIME8) kOrdCum(3000,605000,kTIME2),
kChkOrder(UserCanceled,ExchangeAccepted,4000,    0,    0,kTIME6) kOrdPri(203,kTIME8) kOrdCum(3000,605000,kTIME2) kErrCode_0,
kChkOrder(UserCanceled,ExchangeNoLeaves,   0,    0,    0,kTIME9) kOrdPri(203,kTIME8) kOrdCum(3000,605000,kTIME2) kErrCode_NoLeaves,
   };
   RunTestList(core, "Out-of-order(NoNewQuery)", cstrTestList);
}
//--------------------------------------------------------------------------//
// 有收到新單回報, 及後續回報.
void TestNormalReport(TestCoreSP& core) {
   const char* cstrTestList[] = {
">AllocOrdNo=T-N-8610-A",
"+" kTwsRpt(ExchangeAccepted,Q,          4000,4000,  kTIME8) kPriLmt(203),
kChkOrder(Initialing,ExchangeAccepted,   4000,4000,0,kTIME8) kOrdPri(203,kTIME8),
// 減1成功.
"+1" kTwsRpt(ExchangeAccepted,C,         7000,6000,  kTIME3) kPriLmt(200),
kChkOrder(ReportPending,ExchangeAccepted,7000,6000,0,kTIME3) kOrdPri(203,kTIME8),
// 成交.
"+1." kTwsFil(1,1000,201,kTIME1),
kChkOrder(ReportPending,PartFilled,         0,   0,0,kTIME3) kOrdPri(203,kTIME8) kOrdCum(0,0,""),
"+1." kTwsFil(2,2000,202,kTIME2),
kChkOrder(ReportPending,PartFilled,         0,   0,0,kTIME3) kOrdPri(203,kTIME8) kOrdCum(0,0,""),
// 改價203成功.
"+1." kTwsRpt(ExchangeAccepted,P,        4000,4000,  kTIME5) kPriLmt(203),
kChkOrder(ReportPending,ExchangeAccepted,4000,4000,0,kTIME5) kOrdPri(203,kTIME8),
// -------------------------------------
// 新單回報
"+1." kTwsRpt(ExchangeAccepted,N,    10000,10000,      kTIME0) kPriLmt(200),
kChkOrderST(ExchangeAccepted,            0,10000,10000,kTIME0) kOrdPri(200,kTIME0),
kChkOrderST(ExchangeAccepted,         7000, 6000, 9000,kTIME3) kOrdPri(203,kTIME8),
kChkOrder(PartFilled,PartFilled,      9000, 8000, 8000,kTIME3) kOrdPri(203,kTIME8) kOrdCum(1000,201000,kTIME1),
kChkOrder(PartFilled,PartFilled,      8000, 6000, 6000,kTIME3) kOrdPri(203,kTIME8) kOrdCum(3000,605000,kTIME2),
kChkOrder(PartFilled,ExchangeAccepted,4000, 4000, 6000,kTIME5) kOrdPri(203,kTIME8) kOrdCum(3000,605000,kTIME2),
// -------------------------------------
// 減1回報成功.
"+1." kTwsRpt(ExchangeAccepted,C,     4000, 3000,      kTIMEB) kPriLmt(204),
kChkOrder(PartFilled,ExchangeAccepted,4000, 3000, 5000,kTIMEB) kOrdPri(204,kTIMEB) kOrdCum(3000,605000,kTIME2),
// -------------------------------------
// 減2回報成功(補單).
"+1." kTwsRpt(ExchangeAccepted,C,     6000, 4000,      kTIME4) kPriLmt(203),
kChkOrder(PartFilled,ExchangeAccepted,6000, 4000, 3000,kTIME4) kOrdPri(204,kTIMEB) kOrdCum(3000,605000,kTIME2),
// -------------------------------------
// 改價204成功(補單).
"+1." kTwsRpt(ExchangeAccepted,P,      4000,4000,      kTIMEA) kPriLmt(204),
kChkOrder(ReportStale,ExchangeAccepted,4000,4000, 3000,kTIMEA) kOrdPri(204,kTIMEB) kOrdCum(3000,605000,kTIME2),
// 改價205成功.
"+1." kTwsRpt(ExchangeAccepted,P,      3000,3000,      kTIMEC) kPriLmt(205),
kChkOrder(PartFilled,ExchangeAccepted, 3000,3000, 3000,kTIMEC) kOrdPri(205,kTIMEC) kOrdCum(3000,605000,kTIME2),
// -------------------------------------
// 刪單失敗, 等「LeavesQty==0」後處理.
"+1." kTwsRpt(ExchangeRejected,D,         0,   0,      kTIMEE)                                                 kErrCode_NoLeaves,
kChkOrder(ReportPending,ExchangeNoLeaves, 0,   0, 3000,kTIMEE) kOrdPri(205,kTIMEC) kOrdCum(3000,605000,kTIME2) kErrCode_NoLeaves,
// 刪單成功, 但會等「LeavesQty=BeforeQty=2」才處理.
"+1" kTwsRpt(ExchangeAccepted,D,         2000, 0,      kTIMED) kPriLmt(205),
kChkOrder(ReportPending,ExchangeAccepted,2000, 0, 3000,kTIMED) kOrdPri(205,kTIMEC) kOrdCum(3000,605000,kTIME2) kErrCode_0,
// -------------------------------------
// 減1回報成功(補單).
"+1." kTwsRpt(ExchangeAccepted,C,       6000,5000,      kTIME4) kPriLmt(203),
kChkOrder(PartFilled,ExchangeAccepted,  6000,5000, 2000,kTIME4) kOrdPri(205,kTIMEC) kOrdCum(3000,605000,kTIME2) kErrCode_0,
kChkOrder(UserCanceled,ExchangeAccepted,2000,   0,    0,kTIMED) kOrdPri(205,kTIMEC) kOrdCum(3000,605000,kTIME2) kErrCode_0,
kChkOrder(UserCanceled,ExchangeNoLeaves,   0,   0,    0,kTIMEE) kOrdPri(205,kTIMEC) kOrdCum(3000,605000,kTIME2) kErrCode_NoLeaves,
   };
   RunTestList(core, "Out-of-order(TestNormalReport)", cstrTestList);
}
//--------------------------------------------------------------------------//
// 先收到成交, 後收到減量回報(造成 LeavesQty=0).
void TestPartCancel(TestCoreSP& core) {
   const char* cstrTestList[] = {
">AllocOrdNo=T-N-8610-A",
// 新單回報.
"+" kTwsRpt(ExchangeAccepted,N,       10000,10000,      kTIME0) kPriLmt(200),
kChkOrderST(ExchangeAccepted,             0,10000,10000,kTIME0) kOrdPri(200,kTIME0),
// 成交.
"+1." kTwsFil(1,                                                                     1000,201,   kTIME5),
kChkOrderST(PartFilled,               10000, 9000, 9000,kTIME0) kOrdPri(200,kTIME0) kOrdCum(1000,201000,kTIME5),
"+1." kTwsFil(2,                                                                     2000,202,   kTIME6),
kChkOrderST(PartFilled,                9000, 7000, 7000,kTIME0) kOrdPri(200,kTIME0) kOrdCum(3000,605000,kTIME6),
// 減3(補單).
"+1." kTwsRpt(ExchangeAccepted,C,     10000, 7000,      kTIME1) kPriLmt(200),
kChkOrder(PartFilled,ExchangeAccepted,10000, 7000, 4000,kTIME1) kOrdPri(200,kTIME0) kOrdCum(3000,605000,kTIME6),
// 減4(補單).
"+1." kTwsRpt(ExchangeAccepted,C,      7000, 3000,      kTIME2) kPriLmt(200),
kChkOrder(AsCanceled,ExchangeAccepted, 7000, 3000,    0,kTIME2) kOrdPri(200,kTIME0) kOrdCum(3000,605000,kTIME6),
   };
   RunTestList(core, "Out-of-order(PartCancel)", cstrTestList);
}
//--------------------------------------------------------------------------//
void TestErrCodeAct_ReNew(TestCoreSP& core) {
   const char* cstrTestList[] = {
kTwsNew,
kChkOrderNewSending,
// ----- ReNew 3 次: Resend 時的 BfQty, AfQty, LeavesQty 不可改變, 否則會造成風控重複計算.
"-L1=" kTwsRpt(ExchangeRejected,N,     0,    0,      kTIME0)                 kErrCode_Rerun3,
kChkOrderST(Sending,               10000,10000,10000,kTIME0) kOrdPri(200,"") kErrCode_Rerun3,
"-L1=" kTwsRpt(ExchangeRejected,N, 10000,    0,      kTIME1)                 kErrCode_Rerun3, // TSEC.FIX 新單失敗, Bf=OrderQty, Af=LeavesQty=0;
kChkOrderST(Sending,               10000,10000,10000,kTIME1) kOrdPri(200,"") kErrCode_Rerun3, // 新單重送, LeavesQty=AfterQty=BeforeQty; 這樣才不會造成風控重複計算.
"-L1=" kTwsRpt(ExchangeRejected,N,     0,    0,      kTIME2)                 kErrCode_Rerun3,
kChkOrderST(Sending,               10000,10000,10000,kTIME2) kOrdPri(200,"") kErrCode_Rerun3,
"-L1=" kTwsRpt(ExchangeRejected,N,     0,    0,      kTIME3)                 kErrCode_Rerun3,
kChkOrderST(ExchangeRejected,      10000,    0,    0,kTIME3) kOrdPri(200,"") kErrCode_Rerun3, // 最終失敗.

// ----- ReNew 過程中 Delete.
kTwsNew,
kChkOrderNewSending,
// New(Re:1)
"-L11=" kTwsRpt(ExchangeRejected,N,    0,    0,      kTIME0)                 kErrCode_Rerun3,
kChkOrderST(Sending,               10000,10000,10000,kTIME0) kOrdPri(200,"") kErrCode_Rerun3,

// 然後收到刪單要求.
"*1.TwsChg" kOrdKey "|Qty=0",
kChkOrderST(Sending,               10000,10000,10000,kTIME0) kOrdPri(200,"") kErrCode_0,
// 刪單要求回報失敗:Rerun4 => Chg(Re:1).
"-L15=" kTwsRpt(ExchangeRejected,D,    0,    0,      kTIME1) kPriLmt(200)    kErrCode_Rerun4_AtNewDone,
kChkOrderST(Sending,                   0,    0,10000,kTIME1) kOrdPri(200,"") kErrCode_Rerun4_AtNewDone,

// New(Re:1) 的回報, 因 NewSending 之後收到 DeleteRequest, 所以不用再重送新單了.
"-L11=" kTwsRpt(ExchangeRejected,N,    0,10000,      kTIME3)                 kErrCode_Rerun3,
kChkOrderST(ExchangeRejected,      10000,    0,    0,kTIME3) kOrdPri(200,"") kErrCode_Rerun3,
// Rerun Delete 的回報, 因新單失敗, 所以不用重送 Delete.
"-L15=" kTwsRpt(ExchangeRejected,D,    0,    0,      kTIME5) kPriLmt(200)    kErrCode_Rerun4_AtNewDone,
kChkOrderST(ExchangeRejected,          0,    0,    0,kTIME5) kOrdPri(200,"") kErrCode_Rerun4_AtNewDone,
   };
   RunTestList(core, "ErrCodeAct.ReNew", cstrTestList);
}
//--------------------------------------------------------------------------//
void TestErrCodeAct_ReChg(TestCoreSP& core) {
   const char* cstrTestList[] = {
kTwsNew,
kChkOrderNewSending,
// ----- NewSending => Delete 失敗 Rerun2(AtNewDone=Y)
"*1.TwsChg" kOrdKey "|Qty=0",
kChkOrder(Sending,Sending,           10000,10000,10000,"")     kOrdPri(200,""),
"-L3=" kTwsRpt(ExchangeRejected,D,       0,    0,      kTIME1)                 kErrCode_Rerun2_AtNewDone,
kChkOrder(Sending,Sending,               0,    0,10000,kTIME1) kOrdPri(200,"") kErrCode_Rerun2_AtNewDone,
"-L3=" kTwsRpt(ExchangeRejected,D,       0,    0,      kTIME2)                 kErrCode_Rerun2_AtNewDone,
kChkOrder(Sending,Sending,               0,    0,10000,kTIME2) kOrdPri(200,"") kErrCode_Rerun2_AtNewDone,
"-L3=" kTwsRpt(ExchangeRejected,D,       0,    0,      kTIME3)                 kErrCode_Rerun2_AtNewDone,
kChkOrder(ReportPending,ExchangeRejected,0,    0,10000,kTIME3) kOrdPri(200,"") kErrCode_Rerun2_AtNewDone,

// ----- 刪單.
"*1.TwsChg" kOrdKey "|Qty=0",
kChkOrder(Sending,Sending,                  10000,10000,10000,kTIME3) kOrdPri(200,"") kErrCode_0,
// ----- 刪單(NoRerun)一般失敗 => 等新單回報後處理.
"-2=" kTwsRpt(ExchangeRejected,D,               0,    0,      kTIME4)                 kErrCode_NoRerun,
kChkOrder(ReportPending,ExchangeRejected,       0,    0,10000,kTIME4) kOrdPri(200,"") kErrCode_NoRerun,

// ----- New.Accepted.
"-L1=" kTwsRpt(ExchangeAccepted,N,          10000,10000,      kTIME5) kPriLmt(200),
kChkOrderST(ExchangeAccepted,               10000,10000,10000,kTIME5) kOrdPri(200,kTIME5),
// 新單成功, 處理 Rerun2(AtNewDone=Y) 的失敗回報: Rerun.
kChkOrder(ExchangeAccepted,Sending,             0,    0,10000,kTIME3) kOrdPri(200,kTIME5) kErrCode_Rerun2_AtNewDone,
// 新單成功, 處理 (NoRerun)一般失敗 的 ReportPending 回報.
kChkOrder(ExchangeAccepted,ExchangeRejected,    0,    0,10000,kTIME4) kOrdPri(200,kTIME5) kErrCode_NoRerun,
// Rerun2(AtNewDone=Y) 之後, 又收到 Rerun2 的失敗, 因為有 AtNewDone=Y, 所以不會再 Rerun.
"-L3=" kTwsRpt(ExchangeRejected,D,              0,    0,      kTIME6)                     kErrCode_Rerun2_AtNewDone,
kChkOrder(ExchangeAccepted,ExchangeRejected,10000,10000,10000,kTIME6) kOrdPri(200,kTIME5) kErrCode_Rerun2_AtNewDone,

// ----- New.ExchangeCanceled.
"-L1=" kTwsRpt(ExchangeCanceled,N,          10000,    0,      kTIME9) kPriLmt(200),
kChkOrderST(ExchangeCanceled,               10000,    0,    0,kTIME9) kOrdPri(200,kTIME9),
   };
   RunTestList(core, "ErrCodeAct.ReChg", cstrTestList);
}
void TestErrCodeAct_ReChg2(TestCoreSP& core) {
   const char* cstrTestList[] = {
kTwsNew,
kChkOrderNewSending,
// ----- NewSending => Delete 失敗重送4次, 然後失敗.
"*1.TwsChg" kOrdKey "|Qty=0",
kChkOrder(Sending,Sending,                  10000,10000,10000,"")     kOrdPri(200,"") kErrCode_0,
// ----- NewSending => Delete 失敗重送1次 => New.Accepted 之後應重送一次.
"*1.TwsChg" kOrdKey "|Qty=0",
kChkOrder(Sending,Sending,                  10000,10000,10000,"")     kOrdPri(200,"") kErrCode_0,

"-L3=" kTwsRpt(ExchangeRejected,D,              0,    0,      kTIME1)                 kErrCode_Rerun4_AtNewDone,
kChkOrder(Sending,Sending,                      0,    0,10000,kTIME1) kOrdPri(200,"") kErrCode_Rerun4_AtNewDone,

"-L3=" kTwsRpt(ExchangeRejected,D,              0,    0,      kTIME2)                 kErrCode_Rerun4_AtNewDone,
kChkOrder(Sending,Sending,                      0,    0,10000,kTIME2) kOrdPri(200,"") kErrCode_Rerun4_AtNewDone,
"-L3=" kTwsRpt(ExchangeRejected,D,              0,    0,      kTIME3)                 kErrCode_Rerun4_AtNewDone,
kChkOrder(Sending,Sending,                      0,    0,10000,kTIME3) kOrdPri(200,"") kErrCode_Rerun4_AtNewDone,
"-L3=" kTwsRpt(ExchangeRejected,D,              0,    0,      kTIME4)                 kErrCode_Rerun4_AtNewDone,
kChkOrder(Sending,Sending,                      0,    0,10000,kTIME4) kOrdPri(200,"") kErrCode_Rerun4_AtNewDone,
"-L3=" kTwsRpt(ExchangeRejected,D,              0,    0,      kTIME5)                 kErrCode_Rerun4_AtNewDone,
kChkOrder(ReportPending,ExchangeRejected,       0,    0,10000,kTIME5) kOrdPri(200,"") kErrCode_Rerun4_AtNewDone,

"-L5=" kTwsRpt(ExchangeRejected,D,              0,    0,      kTIME6)                 kErrCode_Rerun4_AtNewDone,
kChkOrder(Sending,Sending,                      0,    0,10000,kTIME6) kOrdPri(200,"") kErrCode_Rerun4_AtNewDone,

// ----- New.Accepted.
"-L1=" kTwsRpt(ExchangeAccepted,N,          10000,10000,      kTIME8) kPriLmt(200),
kChkOrderST(ExchangeAccepted,               10000,10000,10000,kTIME8) kOrdPri(200,kTIME8) kErrCode_0,
// 新單成功後再送一次刪單.
kChkOrder(ExchangeAccepted,Sending,             0,    0,10000,kTIME5) kOrdPri(200,kTIME8) kErrCode_Rerun4_AtNewDone,
"-L3=" kTwsRpt(ExchangeRejected,D,              0,    0,      kTIMEA)                     kErrCode_Rerun4_AtNewDone,
kChkOrder(ExchangeAccepted,ExchangeRejected,10000,10000,10000,kTIMEA) kOrdPri(200,kTIME8) kErrCode_Rerun4_AtNewDone,
// Ln#5 的刪單回報 => 再送一次刪單.
"-L5=" kTwsRpt(ExchangeRejected,D,              0,    0,      kTIMEB) kPriLmt(200)        kErrCode_Rerun4_AtNewDone,
kChkOrder(ExchangeAccepted,Sending,         10000,10000,10000,kTIMEB) kOrdPri(200,kTIME8) kErrCode_Rerun4_AtNewDone,
"-L5=" kTwsRpt(ExchangeRejected,D,              0,    0,      kTIMEC)                     kErrCode_Rerun4_AtNewDone,
kChkOrder(ExchangeAccepted,ExchangeRejected,10000,10000,10000,kTIMEC) kOrdPri(200,kTIME8) kErrCode_Rerun4_AtNewDone,
   };
   RunTestList(core, "ErrCodeAct.ReChg2", cstrTestList);
}
//--------------------------------------------------------------------------//
int main(int argc, char* argv[]) {
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif

   InitReportUT(argc, argv);
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

   TestErrCodeAct_ReNew(core);
   utinfo.PrintSplitter();

   TestErrCodeAct_ReChg(core);
   utinfo.PrintSplitter();

   TestErrCodeAct_ReChg2(core);
   utinfo.PrintSplitter();
}
