// \file f9omstw/OmsTwfReportQuote_UT.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsReport_RunTest.hpp"
#include "f9omstwf/OmsTwfReport3.hpp"
using namespace f9omstw;
//--------------------------------------------------------------------------//
struct UomsRequestIni9RiskCheck : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(UomsRequestIni9RiskCheck);
   using base = OmsRequestRunStep;
   using base::base;

   void RunRequest(OmsRequestRunnerInCore&& runner) override {
      assert(dynamic_cast<const OmsTwfRequestIni9*>(runner.OrderRaw_.Order().Initiator()) != nullptr);
      auto& ordraw = *static_cast<OmsTwfOrderRaw9*>(&runner.OrderRaw_);
      auto* inireq = static_cast<const OmsTwfRequestIni9*>(ordraw.Order().Initiator());
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
//--------------------------------------------------------------------------//
#define kSYMID    "TXFL9"

void InitTestCore(TestCore& core) {
   auto  ordfac1 = core.Owner_->OrderFactoryPark().GetFactory("TwfOrd");
   auto  ordfac9 = core.Owner_->OrderFactoryPark().GetFactory("TwfOrdQ");
   core.Owner_->SetRequestFactoryPark(new OmsRequestFactoryPark(
      new OmsTwfRequestIni9Factory("TwfNewQ", ordfac9,
                                  OmsRequestRunStepSP{new UomsRequestIni9RiskCheck{
                                       OmsRequestRunStepSP{new UomsTwfExgSender}}}),
      new OmsTwfRequestChg9Factory("TwfChgQ", OmsRequestRunStepSP{new UomsTwfExgSender}),
      new OmsTwfFilled1Factory("TwfFil", ordfac1, ordfac9),
      new OmsTwfReport9Factory("TwfRptQ", ordfac9),
      new OmsTwfReport3Factory("TwfR03", nullptr)
      ));
   core.Owner_->ReloadErrCodeAct(
      "$x=x\n" // 用 $ 開頭, 表示此處提供的是設定內容, 不是檔案名稱.
      "40002:Rerun=2|Memo=TIME IS EARLY\n"
      "40005:Rerun=2|AtNewDone=Y|UseNewLine=Y|Memo=ORDER NOT FOUND\n"
      "40010:St=ExchangeNoLeavesQty\n"
   );
   core.GetResource().Symbs_->FetchOmsSymb(kSYMID)->PriceOrigDiv_ = 1;
   core.OpenReload(gArgc, gArgv, "OmsTwfReportQuote_UT.log");
}

//--------------------------------------------------------------------------//
#define kTIME1 "20191107090001.000000"
#define kTIME2 "20191107090002.000000"
#define kTIME3 "20191107090003.000000"
#define kTIME4 "20191107090004.000000"
#define kTIME5 "20191107090005.000000"
#define kTIME6 "20191107090006.000000"
#define kTIME7 "20191107090007.000000"
#define kTIME8 "20191107090008.000000"
#define kTIME9 "20191107090009.000000"
//--------------------------------------------------------------------------//
#define kOrdKey            "|Market=f|SessionId=N|BrkId=8610"
#define kRptKeySymb        kOrdKey "|Symbol=" kSYMID  // 一般回報、成交回報 都有的欄位.
#define kRptDef(st)        kRptVST(st) kRptKeySymb    // 一般回報的共用欄位.

#define kQTYS(bo,bf,af,le) "|" fon9_CTXTOCSTR(bo) "BeforeQty=" fon9_CTXTOCSTR(bf) \
                           "|" fon9_CTXTOCSTR(bo) "AfterQty="  fon9_CTXTOCSTR(af) \
                           "|" fon9_CTXTOCSTR(bo) "LeavesQty=" fon9_CTXTOCSTR(le)

#define kVAL_PartExchangeAccepted   a2
#define kVAL_PartExchangeRejected   ae
#define kVAL_PartExchangeCancel     a7
#define kVAL_ExchangeCanceling      f7

// [0]: '+'=Report (占用序號).
// [0]: '-'=Report (不占用序號).
// [0]: '*'=Request 透過 core.MoveToCore(OmsRequestRunner{}) 執行;
// [0]: '/'=檢查 OrderRaw 是否符合預期. (占用序號).
// [1..]: 參考資料「'L' + LineNo」或「序號-N」
#define kTwfNewQ \
        "* TwfNewQ" kOrdKey "|IvacNo=10|Symbol=" kSYMID \
        "|BidPri=8000|BidQty=50|OfferPri=8010|OfferQty=60"

#define kTwfChgQ(bidQty,offerQty) \
        "*1.TwfChgQ" kOrdKey "|BidQty=" #bidQty "|OfferQty=" #offerQty

#define kChkOrder(ordSt,reqSt,bidBfQty,bidAfQty,bidLeaves,offerBfQty,offerAfQty,offerLeaves,tm)          \
        "/ TwfOrdQ" kOrdReqST(kVAL_##ordSt, kVAL_##reqSt) kQTYS(Bid,bidBfQty,bidAfQty,bidLeaves)         \
                                                          kQTYS(Offer,offerBfQty,offerAfQty,offerLeaves) \
                                                          "|LastExgTime=" tm
#define kChkOrderST(st,bidBfQty,bidAfQty,bidLeaves,offerBfQty,offerAfQty,offerLeaves,tm) \
        kChkOrder(st,st,bidBfQty,bidAfQty,bidLeaves,offerBfQty,offerAfQty,offerLeaves,tm)

#define kChkOrderNewSending kChkOrder(Sending,Sending, 0,50,50, 0,60,60, "")

// 一般而言, 前方應加上 "-2" 表示: 回報本身不占序號, 回報的要求為前2個佔序號的下單要求.
#define kTwfRptQ_Bid(st,kind,bfQty,afQty,tm) \
        "TwfRptQ" kRptDef(st) "|Kind=" #kind "|Side=B|BidBeforeQty=" #bfQty "|BidQty=" #afQty "|ExgTime=" tm
// 一般而言, 前方應加上 "-3" 表示: 回報本身不占序號, 回報的要求為前3個佔序號的下單要求.
#define kTwfRptQ_Offer(st,kind,bfQty,afQty,tm) \
        "TwfRptQ" kRptDef(st) "|Kind=" #kind "|Side=S|OfferBeforeQty=" #bfQty "|OfferQty=" #afQty "|ExgTime=" tm

// 一般而言, 前方應加上 "+1" 表示: 回報本身占序號, 回報的參考為1個佔序號的下單要求.
#define kTwfFil(matKey,side,qty,pri,tm) \
        "TwfFil" kRptKeySymb "|MatchKey=" #matKey "|PosEff=9|Side=" #side \
                 "|Qty=" #qty "|Pri=" #pri "|Time=" tm
#define kChkCum(bidCumQty,bidCumAmt,offerCumQty,offerCumAmt,tm) \
        "|OfferCumQty=" #offerCumQty "|OfferCumAmt=" #offerCumAmt  \
        "|BidCumQty=" #bidCumQty "|BidCumAmt=" #bidCumAmt "|LastFilledTime=" tm

// 一般而言, 前方應加上 "-2" 表示: 回報本身不占序號, 回報的要求為前2個佔序號的下單要求.
#define kTwfR03(st,kind,side,errc,tm) \
        "TwfR03" kOrdKey kRptVST(st) "|Kind=" #kind "|Side=" #side \
                 "|ErrCode=" #errc "|ExgTime=" tm

// 一次回報雙邊, 定義回報前, 必須先由測試程序分配一個委託書號.
// ">AllocOrdNo=f-N-8610-A",
// "+ " kNewReportBothSide,
#define kNewReportBothSide \
        "TwfRptQ" kRptDef(ExchangeAccepted) "|Kind=N|Side=|ExgTime=" kTIME1 \
                  "|BidBeforeQty=50|BidQty=50|OfferBeforeQty=60|OfferQty=60"

//--------------------------------------------------------------------------//
// 一般正常順序下單回報.
void TestNormalOrder(TestCoreSP& core) {
   const char* cstrTestList[] = {
kTwfNewQ,
kChkOrderNewSending,
// 單邊(Bid)首次回報的 ReportSt 應設定為 PartExchangeAccepted
"-2." kTwfRptQ_Bid(PartExchangeAccepted,N,           50,50,              kTIME1),
kChkOrder(PartExchangeAccepted,PartExchangeAccepted, 50,50,50, 60,60,60, kTIME1),
// 另邊(Offer)回報的 ReportSt 應設定為 ExchangeAccepted
"-3." kTwfRptQ_Offer(ExchangeAccepted,N,                       60,60,    kTIME2),
kChkOrder(ExchangeAccepted,ExchangeAccepted,         50,50,50, 60,60,60, kTIME2),

// 測試 Bid 部分成交.
"+1." kTwfFil(1,                                                             B,3,8000,     kTIME3),
kChkOrder(PartFilled,PartFilled,           50,47,47, 60,60,60, kTIME2) kChkCum(3,24000,0,0,kTIME3),
// 測試 Offer 部分成交.
"+1." kTwfFil(2,                                                                     S,9,8010, kTIME4),
kChkOrder(PartFilled,PartFilled,           47,47,47, 60,51,51, kTIME2) kChkCum(3,24000,9,72090,kTIME4),

// 減量.
kTwfChgQ(7,1),
kChkOrder(PartFilled,Sending,              47,47,47, 51,51,51, kTIME2) kChkCum(3,24000,9,72090,kTIME4),
"-2." kTwfRptQ_Bid(PartExchangeAccepted,C, 47,40,              kTIME5),
kChkOrder(PartFilled,PartExchangeAccepted, 47,40,40, 51,51,51, kTIME5) kChkCum(3,24000,9,72090,kTIME4),
"-3." kTwfRptQ_Offer(ExchangeAccepted,C,             51,50,    kTIME6),
kChkOrder(PartFilled,ExchangeAccepted,     40,40,40, 51,50,50, kTIME6) kChkCum(3,24000,9,72090,kTIME4),

// 期交所使用「成交回報數量=0」回報自動刪除:
// 所以當回報線路收到「成交量=0」時, 應轉成 TwfRptQ 且 origReq = 原始新單(所以使用 "-L1").
"-L1." kTwfRptQ_Bid(ExchangeCanceling,N,       40,0,             kTIME8),
kChkOrder(ExchangeCanceling,ExchangeCanceling, 40,0,0, 50,50,50, kTIME8),
"-L1." kTwfRptQ_Offer(ExchangeCanceled,N,              50,0,     kTIME9),
kChkOrder(ExchangeCanceled,ExchangeCanceled,   0,0,0,  50,0,0,   kTIME9),

// -- 新單失敗 --------------------------
kTwfNewQ,
kChkOrderNewSending,
"-2." kTwfR03(PartExchangeRejected,N,                B,40001,          kTIME1),
kChkOrder(PartExchangeRejected,PartExchangeRejected, 50,0,0, 60,60,60, kTIME1),
"-3." kTwfR03(ExchangeRejected,N,                     S,/*no ErrCode*/,kTIME2),
kChkOrder(ExchangeRejected,ExchangeRejected,         0,0,0,  60,0,0,   kTIME2),

// -- Bid 新單合併成交: 先 ReportPending, 等 Offer; -----
kTwfNewQ,
kChkOrderNewSending,
"+1." kTwfFil(1,B,3,8000,kTIME3),
kChkOrder(ReportPending,PartFilled, 50,50,50, 60,60,60, "")     kChkCum(0,0,0,0,""),
"-3." kTwfRptQ_Offer(ExchangeAccepted,N,      60,60,    kTIME2),
kChkOrderST(ExchangeAccepted,       50,50,50, 60,60,60, kTIME2),
kChkOrderST(PartFilled,             50,47,47, 60,60,60, kTIME2) kChkCum(3,24000,0,0,kTIME3),

// -- Offer 新單合併成交 -----
kTwfNewQ,
kChkOrderNewSending,
"-2." kTwfRptQ_Bid(PartExchangeAccepted,N, 50,50,              kTIME1),
kChkOrderST(PartExchangeAccepted,          50,50,50, 60,60,60, kTIME1),
"v1." kTwfFil(2,                                                                 S,9,8010, kTIME4),
kChkOrderST(ExchangeAccepted,              50,50,50, 60,60,60, kTIME4),
kChkOrderST(PartFilled,                    50,50,50, 60,51,51, kTIME4) kChkCum(0,0,9,72090,kTIME4),
   };
   RunTestList(core, "Normal-order", cstrTestList);
}
//--------------------------------------------------------------------------//
// 下單後,亂序回報.
void TestOutofOrder(TestCoreSP& core) {
   const char* cstrTestList[] = {
// 新單-線路A => 減量-線路B => 先收到減量回報
kTwfNewQ,
kChkOrderNewSending,
// - 減量, 測試重送(因尚未收到新單回報). ----------
kTwfChgQ(7,1),
kChkOrder(Sending,Sending,              50,50,50, 60,60,60, /*No LastExgTime*/),
"-2." kTwfR03(PartExchangeRejected,C,   B,40005,            kTIME1),
kChkOrder(Sending,PartExchangeRejected, 0,0,50,   60,60,60, kTIME1),
// ↓加上「=」表示該行不要測試「重複回報」, 因為此行會造成 Rerun, 所以狀態變成 Sending, 此時無法判斷是否重複(此時系統會認為沒重複).
"-3=" kTwfR03(ExchangeRejected,C,                 S,40005,  kTIME2),
kChkOrder(Sending,Sending,              50,50,50, 0,0,60,   kTIME2),
// -- 第1次重送的結果:
"-4." kTwfR03(PartExchangeRejected,C,   B,40005,            kTIME3),
kChkOrder(Sending,PartExchangeRejected, 0,0,50,   60,60,60, kTIME3),
"-5=" kTwfR03(ExchangeRejected,C,                 S,40005,  kTIME4),
kChkOrder(Sending,Sending,              50,50,50, 0,0,60,   kTIME4),
// -- 第2次重送的結果:
// 已達要求的重送次數, 因為新單尚未回報, 所以 OrderSt = ReportPending; 等候新單回報在送一次(因為有 AtNewDone=Y).
"-6." kTwfR03(PartExchangeRejected,C,         B,40005,          kTIME5),
kChkOrder(ReportPending,PartExchangeRejected, 0,0,50, 60,60,60, kTIME5),
"-7=" kTwfR03(ExchangeRejected,C,                     S,40005,  kTIME6),
kChkOrder(ReportPending,ExchangeRejected,     0,0,50, 0,0,60,   kTIME6),

// - 刪單成功: ReportPending ----------
kTwfChgQ(0,0),
kChkOrder(Sending,Sending,                  50,50,50, 60,60,60, kTIME6),
"-2." kTwfRptQ_Bid(PartExchangeCancel,D,    43,0,               kTIME9),
kChkOrder(ReportPending,PartExchangeCancel, 43,0,50,  60,60,60, kTIME9),
"-3." kTwfRptQ_Offer(ExchangeCanceled,D,              59,0,     kTIME5),
kChkOrder(ReportPending,ExchangeCanceled,   43,0,50,  59,0,60,  kTIME5),

// - 新單回報 ----------
"-L1." kTwfRptQ_Bid(PartExchangeAccepted,N,          50,50,              kTIME7),
kChkOrder(PartExchangeAccepted,PartExchangeAccepted, 50,50,50, 60,60,60, kTIME7),
"-L1." kTwfRptQ_Offer(ExchangeAccepted,N,                      60,60,    kTIME8),
kChkOrder(ExchangeAccepted,ExchangeAccepted,         50,50,50, 60,60,60, kTIME8),
// - 新單回報後, 處理 ReportPending ----------
// 因為 ErrCode=40005, 有設定 AtNewDone=Y, 所以在新單完成後, 再送一次.
kChkOrder(ExchangeAccepted,Sending,                  0,0,50,   0,0,60,   kTIME6),
"-L3." kTwfRptQ_Bid(PartExchangeAccepted,C,          50,43,              kTIME9),
kChkOrder(ExchangeAccepted,PartExchangeAccepted,     50,43,43, 60,60,60, kTIME9),
"-L3." kTwfRptQ_Offer(ExchangeAccepted,C,                      60,59,    kTIME7),
kChkOrder(ExchangeAccepted,ExchangeAccepted,         43,43,43, 60,59,59, kTIME7),
// 刪單成功 ReportPending 的後續.
kChkOrder(Canceled,ExchangeCanceled,                 43,0,0,   59,0,0,   kTIME5),
   };
   RunTestList(core, "Out-of-order", cstrTestList);
}
//--------------------------------------------------------------------------//
// 下單後,亂序(交錯)回報.
void TestInterlaced(TestCoreSP& core) {
   const char* cstrTestList[] = {
// 新單-線路A => 減量-線路B
// => 收到減量Bid回報
// => 收到新單Bid回報
// => 收到減量Offer回報
// => 收到新單Offer回報
kTwfNewQ,
kChkOrderNewSending,
// 減量要求.
kTwfChgQ(7,1),
kChkOrder(Sending,Sending,                           50,50,50, 60,60,60, /*No LastExgTime*/),
// 減量 Bid 回報, 還沒收到新單回報, 所以 ReportPending, 等候新單回報再處理. 因為是 ReportPending 所以 BidLeavesQty 仍為 50 不變;
"-2." kTwfRptQ_Bid(PartExchangeAccepted,C,           50,43,              kTIME1),
kChkOrder(ReportPending,PartExchangeAccepted,        50,43,50, 60,60,60, kTIME1),
// 新單 Bid 回報.
"-L1." kTwfRptQ_Bid(PartExchangeAccepted,N,          50,50,              kTIME2),
kChkOrder(PartExchangeAccepted,PartExchangeAccepted, 50,50,50, 60,60,60, kTIME2),
// 減量 Offer 回報, 還沒收到完整新單回報, 所以 ReportPending; 因為已收過 Bid 減量, 所以 Order.Bid* = 之前的 Bid 減量回報.
"-4." kTwfRptQ_Offer(ExchangeAccepted,C,                       60,59,    kTIME3),
kChkOrder(ReportPending,ExchangeAccepted,            50,43,50, 60,59,60, kTIME3),
// 新單 Offer 回報.
"-L1." kTwfRptQ_Offer(ExchangeAccepted,N,                      60,60,    kTIME4),
kChkOrder(ExchangeAccepted,ExchangeAccepted,         50,50,50, 60,60,60, kTIME4),
// 減量 ReportPending 的後續.
kChkOrder(ExchangeAccepted,ExchangeAccepted,         50,43,43, 60,59,59, kTIME3),
// -------------------------
// 刪單(在途成交)
// => 刪 Bid   -40.                 (在途成交3)
// => 刪 Offer 失敗(40010=NoLeaves).(在途成交59)
kTwfChgQ(0,0),
kChkOrder(ExchangeAccepted,Sending,           43,43,43, 59,59,59, kTIME3),
"-2." kTwfRptQ_Bid(PartExchangeAccepted,D,    40,0,               kTIME1),
kChkOrder(ReportPending,PartExchangeAccepted, 40,0,43,  59,59,59, kTIME1),
"-3." kTwfR03(ExchangeRejected,D,S,40010,                         kTIME2),
kChkOrder(ReportPending,ExchangeNoLeaves,     40,0,43,  0,0,59,   kTIME2),
// Bid 成交 3.
"+L1." kTwfFil(1,                                                      B,3,8000,     kTIME5),
kChkOrder(PartFilled,PartFilled,     43,40,40, 59,59,59, kTIME2) kChkCum(3,24000,0,0,kTIME5),
// Offer 成交 59.
"+L1." kTwfFil(2,                                                              S,59,8010,  kTIME4),
kChkOrder(PartFilled,PartFilled,     40,40,40, 59,0,0,   kTIME2) kChkCum(3,24000,59,472590,kTIME4),
// 刪單 ReportPending 的後續.
kChkOrder(Canceled,ExchangeNoLeaves, 40,0,0,   0,0,0,    kTIME2) kChkCum(3,24000,59,472590,kTIME4),
   };
   RunTestList(core, "Interlaced", cstrTestList);
}
//--------------------------------------------------------------------------//
// 部分失敗回報.
// Case | Bid                         | Offer                       | OrderSt
// -----|-----------------------------|-----------------------------|--------------------
//   1  | PartExchangeCancel          | ExchangeRejected(Leaves!=0) | ExchangeAccepted 等 Offer 的後續回報.(實務上會有此情況嗎?)
void TestPartR03Case1(TestCoreSP& core) {
   const char* cstrCase[] = {
kTwfNewQ,
kChkOrderNewSending,
// 新單一次回報(Bid+Offer)成功, 新單只可能 Bid+Offer 都成功, 或都失敗.
"-L1." kNewReportBothSide,
kChkOrder(ExchangeAccepted,ExchangeAccepted,   50,50,50, 60,60,60, kTIME1),
// 新單成功後, 測試刪改部分失敗: 刪: Bid 成功, Offer 失敗.
kTwfChgQ(0,0),
kChkOrder(ExchangeAccepted,Sending,            50,50,50, 60,60,60, kTIME1),
"-2." kTwfRptQ_Bid(PartExchangeCancel,D,       50,0,               kTIME2),
kChkOrder(ExchangeAccepted,PartExchangeCancel, 50,0,0,   60,60,60, kTIME2),
// 非 NoLeavesQty 的失敗, 直接更新 RequestSt, 不改變 OrderSt, 如果是 NoLeavesQty 的失敗, 就要用 ReportPending, 等後續的補單了.
"-3." kTwfR03(ExchangeRejected,D,                S,/*No ErrCode*/, kTIME3),
kChkOrder(ExchangeAccepted,ExchangeRejected,   0,0,0,    60,60,60, kTIME3),
};
   RunTestList(core, "PartR03(Case1)", cstrCase);
}

// Case | Bid                         | Offer                       | OrderSt
// -----|-----------------------------|-----------------------------|--------------------
//   2  | ExchangeRejected(Leaves!=0) | Canceled                    | ExchangeAccepted 等 Bid 的後續回報.(實務上會有此情況嗎?)
void TestPartR03Case2(TestCoreSP& core) {
   const char* cstrCase[] = {
">AllocOrdNo=f-N-8610-A",
"+ " kNewReportBothSide,
kChkOrder(ExchangeAccepted,ExchangeAccepted,     0,50,50,  0,60,60,  kTIME1),
kTwfChgQ(0,0),
kChkOrder(ExchangeAccepted,Sending,              50,50,50, 60,60,60, kTIME1),
"-2." kTwfR03(PartExchangeRejected,D,            B,/*No ErrCode*/,   kTIME2),
kChkOrder(ExchangeAccepted,PartExchangeRejected, 50,50,50, 60,60,60, kTIME2),
"-3." kTwfRptQ_Offer(Canceled,D,                           60,0,     kTIME3),
kChkOrder(ExchangeAccepted,Canceled,             50,50,50, 60,0,0,   kTIME3),
};
   RunTestList(core, "PartR03(Case2)", cstrCase);
}

// Case | Bid                         | Offer                       | OrderSt
// -----|-----------------------------|-----------------------------|--------------------
//   3  | PartExchangeCancel          | ExchangeNoLeaves            | Canceled
//      |                             | (Already FullFilled)        |
void TestPartR03Case3(TestCoreSP& core) {
   const char* cstrCase[] = {
">AllocOrdNo=f-N-8610-A",
"+ " kNewReportBothSide,
kChkOrder(ExchangeAccepted,ExchangeAccepted, 0,50,50,  0,60,60, kTIME1),
"+1." kTwfFil(1,                                                                 S,60,8000,  kTIME5),
kChkOrder(PartFilled,PartFilled,             50,50,50, 60,0,0, kTIME1) kChkCum(0,0,60,480000,kTIME5),
kTwfChgQ(0,0),
kChkOrder(PartFilled,Sending,                50,50,50, 0,0,0,  kTIME1),
"-2." kTwfRptQ_Bid(PartExchangeCancel,D,     50,0,             kTIME2),
kChkOrder(Canceled,PartExchangeCancel,       50,0,0,   0,0,0,  kTIME2),
"-3." kTwfR03(ExchangeNoLeaves,D,                      S,40010,kTIME3),
kChkOrder(Canceled,ExchangeNoLeaves,         0,0,0,    0,0,0,  kTIME3),
};
   RunTestList(core, "PartR03(Case3)", cstrCase);
}

// Case | Bid                         | Offer                       | OrderSt
// -----|-----------------------------|-----------------------------|--------------------
//   4  | ExchangeNoLeaves            | Canceled                    | Canceled
//      | (Already FullFilled)        |                             |
void TestPartR03Case4(TestCoreSP& core) {
   const char* cstrCase[] = {
">AllocOrdNo=f-N-8610-A",
"+ " kNewReportBothSide,
kChkOrder(ExchangeAccepted,ExchangeAccepted, 0,50,50, 0,60,60,  kTIME1),
"+1." kTwfFil(2,                                                              B,50,8000,      kTIME5),
kChkOrder(PartFilled,PartFilled,             50,0,0,  60,60,60, kTIME1) kChkCum(50,400000,0,0,kTIME5),
kTwfChgQ(0,0),
kChkOrder(PartFilled,Sending,                0,0,0,   60,60,60, kTIME1),
"-2." kTwfR03(PartExchangeRejected,D,        B,40010,           kTIME2),
kChkOrder(PartFilled,ExchangeNoLeaves,       0,0,0,   60,60,60, kTIME2),
"-3." kTwfRptQ_Offer(Canceled,D,                      60,0,     kTIME3),
kChkOrder(Canceled,Canceled,                 0,0,0,   60,0,0,   kTIME3),
};
   RunTestList(core, "PartR03(Case4)", cstrCase);
}

// Case | Bid                         | Offer                       | OrderSt
// -----|-----------------------------|-----------------------------|--------------------
//   5  | PartExchangeCancel          | ExchangeNoLeaves(Lost qty)  | ReportPending (Offer)
//      |                             | => FullFilled               | FullFilled
void TestPartR03Case5(TestCoreSP& core) {
   const char* cstrCase[] = {
">AllocOrdNo=f-N-8610-A",
"+ " kNewReportBothSide,
kChkOrder(ExchangeAccepted,ExchangeAccepted,     0,50,50,  0,60,60,  kTIME1),
kTwfChgQ(0,0),
kChkOrder(ExchangeAccepted,Sending,              50,50,50, 60,60,60, kTIME1),
"-2." kTwfRptQ_Bid(PartExchangeCancel,D,         50,0,               kTIME2),
kChkOrder(ExchangeAccepted,PartExchangeCancel,   50,0,0,   60,60,60, kTIME2),
"-3." kTwfR03(ExchangeNoLeaves,D,                          S,40010,  kTIME3),
kChkOrder(ReportPending,ExchangeNoLeaves,        0,0,0,    0,0,60,   kTIME3),
"+1." kTwfFil(1,                                                                       S,60,8000,  kTIME5),
kChkOrder(FullFilled,PartFilled,                 0,0,0,    60,0,0,   kTIME3) kChkCum(0,0,60,480000,kTIME5),
kChkOrder(FullFilled,ExchangeNoLeaves,           0,0,0,    0,0,0,    kTIME3) kChkCum(0,0,60,480000,kTIME5),
};
   RunTestList(core, "PartR03(Case5)", cstrCase);
}

// Case | Bid                         | Offer                       | OrderSt
// -----|-----------------------------|-----------------------------|--------------------
//   6  | ExchangeNoLeaves(Lost qty)  | Canceled                    | ReportPending (Both)
//      | All Filled                  |                             | PartFilled
//      |-----------------------------|-----------------------------|--------------------
//      | ProcessPendingReport                                      | Canceled
void TestPartR03Case6(TestCoreSP& core) {
   const char* cstrCase[] = {
">AllocOrdNo=f-N-8610-A",
"+ " kNewReportBothSide,
kChkOrder(ExchangeAccepted,ExchangeAccepted, 0,50,50,  0,60,60,  kTIME1),
kTwfChgQ(0,0),
kChkOrder(ExchangeAccepted,Sending,          50,50,50, 60,60,60, kTIME1),
"-2." kTwfR03(ExchangeNoLeaves,D,            B,40010,            kTIME3),
kChkOrder(ReportPending,ExchangeNoLeaves,    0,0,50,   60,60,60, kTIME3),
"-3." kTwfRptQ_Offer(ExchangeCanceled,D,               60,0,     kTIME2),
kChkOrder(ReportPending,ExchangeCanceled,    0,0,50,   60,0,60,  kTIME2),
"+1." kTwfFil(1,                                                               B,50,8000,      kTIME5),
kChkOrder(PartFilled,PartFilled,             50,0,0,   60,60,60, kTIME2) kChkCum(50,400000,0,0,kTIME5),
kChkOrder(Canceled,ExchangeCanceled,         0,0,0,    60,0,0,   kTIME2) kChkCum(50,400000,0,0,kTIME5),
};
   RunTestList(core, "PartR03(Case6)", cstrCase);
}
void TestPartR03(TestCoreSP& core) {
   TestPartR03Case1(core);
   TestPartR03Case2(core);
   TestPartR03Case3(core);
   TestPartR03Case4(core);
   TestPartR03Case5(core);
   TestPartR03Case6(core);
}
//--------------------------------------------------------------------------//
int main(int argc, char* argv[]) {
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif

   InitReportUT(argc, argv);
   fon9::AutoPrintTestInfo utinfo{"OmsTwfReportQuote"};
   TestCoreSP core;

   TestNormalOrder(core);
   utinfo.PrintSplitter();

   TestOutofOrder(core);
   utinfo.PrintSplitter();

   TestInterlaced(core);
   utinfo.PrintSplitter();

   TestPartR03(core);
   utinfo.PrintSplitter();
}
