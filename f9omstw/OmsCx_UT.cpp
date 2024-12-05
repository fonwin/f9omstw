// \file f9omstw/OmsCx_UT.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCx_UT.hpp"
// ======================================================================== //
// 底下的 R"( 為 Ln:1
void TestLP(TestCore& testCore) {
   testCore.TestCase(TestCore::TestFlag_AutoAll, "LP ==", R"(
SymbId:  1101
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: Waiting: LP==100
SymbEv:  MdDeal:  LastPrice=99|LastQty=1|TotalQty=1
ReqChk:  Waiting
SymbEv:  MdDeal:  LastPrice=101
ReqChk:  Waiting
SymbEv:  MdDeal:  LastPrice=100
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "LP !=", R"(
SymbId:  1101
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: Waiting: LP!=100
SymbEv:  MdDeal:  LastPrice=100|LastQty=1|TotalQty=1
ReqChk:  Waiting
SymbEv:  MdDeal:  LastPrice=101
ReqChk:  Sent
# ------
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: Waiting: LP!=100
SymbEv:  MdDeal:  LastPrice=99|LastQty=1|TotalQty=1
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "LP <=", R"(
SymbId:  1101
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: Waiting: LP<=100
SymbEv:  MdDeal:  LastPrice=101|LastQty=1|TotalQty=1
ReqChk:  Waiting
SymbEv:  MdDeal:  LastPrice=100
ReqChk:  Sent
# ------
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: Waiting: LP<=100
SymbEv:  MdDeal:  LastPrice=99|LastQty=1|TotalQty=1
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "LP >=", R"(
SymbId:  1101
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: Waiting: LP>=100
SymbEv:  MdDeal:  LastPrice=99|LastQty=1|TotalQty=1
ReqChk:  Waiting
SymbEv:  MdDeal:  LastPrice=100
ReqChk:  Sent
# ------
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: Waiting: LP>=100
SymbEv:  MdDeal:  LastPrice=101|LastQty=1|TotalQty=1
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "LP <", R"(
SymbId:  1101
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: Waiting: LP<100
SymbEv:  MdDeal:  LastPrice=101|LastQty=1|TotalQty=1
ReqChk:  Waiting
SymbEv:  MdDeal:  LastPrice=100
ReqChk:  Waiting
SymbEv:  MdDeal:  LastPrice=99
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "LP >", R"(
SymbId:  1101
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: Waiting: LP>100
SymbEv:  MdDeal:  LastPrice=99|LastQty=1|TotalQty=1
ReqChk:  Waiting
SymbEv:  MdDeal:  LastPrice=100
ReqChk:  Waiting
SymbEv:  MdDeal:  LastPrice=101
ReqChk:  Sent
)");
}
void TestBP(TestCore& testCore) {
   testCore.TestCase(TestCore::TestFlag_AutoAll, "BP ==", R"(
SymbId:  1101
SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: BP==100
SymbEv:  MdBS:    B1=99*1
ReqChk:  Waiting
# --------------- B1=Pri*Qty  若省略 *Qty 則 Qty 不變;
SymbEv:  MdBS:    B1=101
ReqChk:  Waiting
# --------------- B1=BM       買進市價
SymbEv:  MdBS:    B1=BM*2
ReqChk:  Waiting
SymbEv:  MdBS:    B1=100
ReqChk:  Sent
# --------------- BP==M       有買進報價,且[最佳買進報價]為市價;
MakeRun: Waiting: BP==M
SymbEv:  MdBS:    B1=BM
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "BP !=", R"(
SymbId:  1101
SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: BP!=100
SymbEv:  MdBS:    B1=100*1
ReqChk:  Waiting
SymbEv:  MdBS:    B1=101*1
ReqChk:  Sent
# ------
SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: BP!=100
SymbEv:  MdBS:    B1=99*1
ReqChk:  Sent
# ------
SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: BP!=100
SymbEv:  MdBS:    B1=BM*1
ReqChk:  Sent
# --------------- BP!=M       有買進報價,且[最佳買進報價]非市價;
SymbEv:  MdBS:    B1=BM|S1=
MakeRun: Waiting: BP!=M
SymbEv:  MdBS:    B1=BM*2
ReqChk:  Waiting
SymbEv:  MdBS:    B1=100*2
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "BP <=", R"(
SymbId:  1101
SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: BP<=100
SymbEv:  MdBS:    B1=101*1
ReqChk:  Waiting
SymbEv:  MdBS:    B1=100*1
ReqChk:  Sent
# ------
SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: BP<=100
SymbEv:  MdBS:    B1=BM
ReqChk:  Waiting
SymbEv:  MdBS:    B1=99*1
ReqChk:  Sent
# --------------- BP<=M       有任意[買進報價];
SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: BP<=M
SymbEv:  MdBS:    B1=BM*1
ReqChk:  Sent

SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: BP<=M
SymbEv:  MdBS:    B1=100*1
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "BP >=", R"(
SymbId:  1101
SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: BP>=100
SymbEv:  MdBS:    B1=99*1
ReqChk:  Waiting
SymbEv:  MdBS:    B1=100*1
ReqChk:  Sent
# ------
SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: BP>=100
SymbEv:  MdBS:    B1=101*1
ReqChk:  Sent

SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: BP>=100
SymbEv:  MdBS:    B1=BM*1
ReqChk:  Sent
# --------------- BP>=M       有買進報價,且[最佳買進報價]為市價;
SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: BP>=M
SymbEv:  MdBS:    B1=101*1
ReqChk:  Waiting
SymbEv:  MdBS:    B1=BM*1
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "BP <", R"(
SymbId:  1101
SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: BP<100
SymbEv:  MdBS:    B1=101*1
ReqChk:  Waiting
SymbEv:  MdBS:    B1=100*1
ReqChk:  Waiting
SymbEv:  MdBS:    B1=BM
ReqChk:  Waiting
SymbEv:  MdBS:    B1=99*1
ReqChk:  Sent
# --------------- BP<M        有買進報價,且[最佳買進報價]非市價;
SymbEv:  MdBS:    B1=BM|S1=
MakeRun: Waiting: BP<M
SymbEv:  MdBS:    B1=100*2
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "BP >", R"(
SymbId:  1101
SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: BP>100
SymbEv:  MdBS:    B1=99*1
ReqChk:  Waiting
SymbEv:  MdBS:    B1=100*1
ReqChk:  Waiting
SymbEv:  MdBS:    B1=101*1
ReqChk:  Sent
# ------
SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: BP>100
SymbEv:  MdBS:    B1=BM*1
ReqChk:  Sent
# --------------- BP>M        永不成立
MakeRun: Waiting: BP>M
SymbEv:  MdBS:    B1=BM*5
ReqChk:  Waiting
SymbEv:  MdBS:    B1=100*5
ReqChk:  Waiting
)");
}
void TestSP(TestCore& testCore) {
   testCore.TestCase(TestCore::TestFlag_AutoAll, "SP ==", R"(
SymbId:  1101
SymbEv:  MdBS:    B1=|S1=
MakeRun: Waiting: SP==100
SymbEv:  MdBS:    S1=99*1
ReqChk:  Waiting
# --------------- S1=Pri*Qty  若省略 *Qty 則 Qty 不變;
SymbEv:  MdBS:    S1=101
ReqChk:  Waiting
# --------------- S1=SM(賣出市價)
SymbEv:  MdBS:    S1=SM*2
ReqChk:  Waiting
SymbEv:  MdBS:    S1=100
ReqChk:  Sent
# --------------- SP==M       有賣出報價,且[最佳賣出報價]為市價;
MakeRun: Waiting: SP==M
SymbEv:  MdBS:    S1=SM
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "SP !=", R"(
SymbId:  1101
SymbEv:  MdBS:    S1=|S1=
MakeRun: Waiting: SP!=100
SymbEv:  MdBS:    S1=100*1
ReqChk:  Waiting
SymbEv:  MdBS:    S1=101*1
ReqChk:  Sent
# ------
SymbEv:  MdBS:    S1=|S1=
MakeRun: Waiting: SP!=100
SymbEv:  MdBS:    S1=99*1
ReqChk:  Sent
# ------
SymbEv:  MdBS:    S1=|S1=
MakeRun: Waiting: SP!=100
SymbEv:  MdBS:    S1=SM*1
ReqChk:  Sent
# --------------- SP!=M       有賣出報價,且[最佳賣出報價]非市價;
SymbEv:  MdBS:    S1=SM|S1=
MakeRun: Waiting: SP!=M
SymbEv:  MdBS:    S1=SM*2
ReqChk:  Waiting
SymbEv:  MdBS:    S1=100*2
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "SP <=", R"(
SymbId:  1101
SymbEv:  MdBS:    S1=|S1=
MakeRun: Waiting: SP<=100
SymbEv:  MdBS:    S1=101*1
ReqChk:  Waiting
SymbEv:  MdBS:    S1=100*1
ReqChk:  Sent
# ------
SymbEv:  MdBS:    S1=|S1=
MakeRun: Waiting: SP<=100
SymbEv:  MdBS:    S1=99*1
ReqChk:  Sent

SymbEv:  MdBS:    S1=|S1=
MakeRun: Waiting: SP<=100
SymbEv:  MdBS:    S1=SM*1
ReqChk:  Sent
# --------------- SP<=M       有賣出報價,且[最佳賣出報價]為市價;
SymbEv:  MdBS:    S1=|S1=
MakeRun: Waiting: SP<=M
SymbEv:  MdBS:    S1=101*1
ReqChk:  Waiting
SymbEv:  MdBS:S1=SM*1
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "SP >=", R"(
SymbId:  1101
SymbEv:  MdBS:    S1=|S1=
MakeRun: Waiting: SP>=100
SymbEv:  MdBS:    S1=99*1
ReqChk:  Waiting
SymbEv:  MdBS:    S1=100*1
ReqChk:  Sent
# ------
SymbEv:  MdBS:    S1=|S1=
MakeRun: Waiting: SP>=100
SymbEv:  MdBS:    S1=SM
ReqChk:  Waiting
SymbEv:  MdBS:    S1=101*1
ReqChk:  Sent
# --------------- SP>=M       有任意[賣出報價];
SymbEv:  MdBS:    S1=|S1=
MakeRun: Waiting: SP>=M
SymbEv:  MdBS:    S1=SM*1
ReqChk:  Sent

SymbEv:  MdBS:    S1=|S1=
MakeRun: Waiting: SP>=M
SymbEv:  MdBS:    S1=100*1
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "SP <", R"(
SymbId:  1101
SymbEv:  MdBS:    S1=|S1=
MakeRun: Waiting: SP<100
SymbEv:  MdBS:    S1=101*1
ReqChk:  Waiting
SymbEv:  MdBS:    S1=100*1
ReqChk:  Waiting
SymbEv:  MdBS:    S1=99*1
ReqChk:  Sent
# ------
SymbEv:  MdBS:    S1=|S1=
MakeRun: Waiting: SP<100
SymbEv:  MdBS:    S1=SM*1
ReqChk:  Sent
# --------------- SP<M        永不成立
MakeRun: Waiting: SP<M
SymbEv:  MdBS:    S1=SM*5
ReqChk:  Waiting
SymbEv:  MdBS:    S1=100*5
ReqChk:  Waiting
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "SP >", R"(
SymbId:  1101
SymbEv:  MdBS:    S1=|S1=
MakeRun: Waiting: SP>100
SymbEv:  MdBS:    S1=99*1
ReqChk:  Waiting
SymbEv:  MdBS:    S1=100*1
ReqChk:  Waiting
SymbEv:  MdBS:    S1=SM
ReqChk:  Waiting
SymbEv:  MdBS:    S1=101*1
ReqChk:  Sent
# --------------- SP>M        有賣出報價,且[最佳賣出報價]非市價;
SymbEv:  MdBS:    S1=SM|S1=
MakeRun: Waiting: SP>M
SymbEv:  MdBS:    S1=100*2
ReqChk:  Sent
)");
}
void TestLQ(TestCore& testCore) {
   // 使用 == 、 < 、 <= 、 >= 時, CondArgs 不可為 0;
   testCore.TestCase(TestCore::TestFlag_AutoAll, "LQ ==", R"(
SymbId:  1101
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: ec=11150:LQ==0
# ------
MakeRun: Waiting: LQ==100
SymbEv:  MdDeal:  LastPrice=20|LastQty=99|TotalQty=99
ReqChk:  Waiting
SymbEv:  MdDeal:  LastQty=101
ReqChk:  Waiting
SymbEv:  MdDeal:  LastQty=100
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "LQ !=", R"(
SymbId:  1101
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: Waiting: LQ!=0
SymbEv:  MdDeal:  LastPrice=20|LastQty=100
ReqChk:  Sent
# ------
MakeRun: Waiting: LQ!=100
SymbEv:  MdDeal:  LastPrice=21|LastQty=100
ReqChk:  Waiting
SymbEv:  MdDeal:  LastQty=101
ReqChk:  Sent
# ------
SymbEv:  MdDeal:  LastPrice=|LastQty=100|TotalQty=
MakeRun: Waiting: LQ!=100
SymbEv:  MdDeal:  LastPrice=20|LastQty=99
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "LQ <=", R"(
SymbId:  1101
SymbEv:  MdDeal:  LastPrice=|LastQty=101|TotalQty=
MakeRun: ec=11150:LQ<=0
# ------
MakeRun: Waiting: LQ<=100
SymbEv:  MdDeal:  LastPrice=21|LastQty=101
ReqChk:  Waiting
SymbEv:  MdDeal:  LastQty=100
ReqChk:  Sent
# ------
SymbEv:  MdDeal:  LastPrice=|LastQty=101|TotalQty=
MakeRun: Waiting: LQ<=100
SymbEv:  MdDeal:  LastPrice=20|LastQty=99
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "LQ >=", R"(
SymbId:  1101
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: ec=11150:LQ>=0
# ------
MakeRun: Waiting: LQ>=100
SymbEv:  MdDeal:  LastPrice=20|LastQty=99
ReqChk:  Waiting
SymbEv:  MdDeal:  LastQty=100
ReqChk:  Sent
# ------
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: Waiting: LQ>=100
SymbEv:  MdDeal:  LastPrice=20|LastQty=101
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "LQ <", R"(
SymbId:  1101
SymbEv:  MdDeal:  LastPrice=|LastQty=100|TotalQty=
MakeRun: ec=11150:LQ<0
# ------
MakeRun: Waiting: LQ<100
SymbEv:  MdDeal:  LastPrice=20|LastQty=101
ReqChk:  Waiting
SymbEv:  MdDeal:  LastQty=100
ReqChk:  Waiting
SymbEv:  MdDeal:  LastQty=99
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "LQ >", R"(
SymbId:  1101
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: Waiting: LQ>0
SymbEv:  MdDeal:  LastPrice=20|LastQty=1
ReqChk:  Sent
# ------
MakeRun: Waiting: LQ>100
SymbEv:  MdDeal:  LastPrice=20|LastQty=99
ReqChk:  Waiting
SymbEv:  MdDeal:  LastQty=100
ReqChk:  Waiting
SymbEv:  MdDeal:  LastQty=101
ReqChk:  Sent
)");
}
void TestTQ(TestCore& testCore) {
   // 僅支援 > 、 >=
   testCore.TestCase(TestCore::TestFlag_AutoAll, "TQ ==", R"(
SymbId:  1101
MakeRun: ec=11999:TQ==0
MakeRun: ec=11999:TQ==100
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "TQ !=", R"(
SymbId:  1101
MakeRun: ec=11999:TQ!=0
MakeRun: ec=11999:TQ!=100
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "TQ <=", R"(
SymbId:  1101
MakeRun: ec=11999:TQ<=0
MakeRun: ec=11999:TQ<=100
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "TQ <", R"(
SymbId:  1101
MakeRun: ec=11999:TQ<0
MakeRun: ec=11999:TQ<100
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "TQ >=", R"(
SymbId:  1101
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: ec=11150:TQ>=0
# ------
MakeRun: Waiting: TQ>=100
SymbEv:  MdDeal:  LastPrice=20|TotalQty=99
ReqChk:  Waiting
SymbEv:  MdDeal:TotalQty=100
ReqChk:  Sent
# ------
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: Waiting: TQ>=100
SymbEv:  MdDeal:  LastPrice=20|TotalQty=101
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "TQ >", R"(
SymbId:  1101
SymbEv:  MdDeal:  LastPrice=|LastQty=|TotalQty=
MakeRun: Waiting: TQ>0
SymbEv:  MdDeal:  LastPrice=20|TotalQty=1
ReqChk:  Sent
# ------
MakeRun: Waiting: TQ>100
SymbEv:  MdDeal:  LastPrice=20|TotalQty=99
ReqChk:  Waiting
SymbEv:  MdDeal:  TotalQty=100
ReqChk:  Waiting
SymbEv:  MdDeal:  TotalQty=101
ReqChk:  Sent
)");
}
void TestBQ(TestCore& testCore) {
   // 不支援 == 、 !=
   testCore.TestCase(TestCore::TestFlag_AutoAll, "BQ ==", R"(
SymbId:  1101
MakeRun: ec=11999:BQ==0
MakeRun: ec=11999:BQ==100
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "BQ !=", R"(
SymbId:  1101
MakeRun: ec=11999:BQ!=0
MakeRun: ec=11999:BQ!=100
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "BQ <=", R"(
SymbId:  1101
# --------------- Qty 不能為 0
MakeRun: ec=11150:BQ<=100,0
# --------------- 必須有 Pri
MakeRun: ec=11103:BQ<=
# --------------- BQ<=100,49     Pri:100,Qty:49
SymbEv:  MdBS:    B1= 101*1
MakeRun: Waiting: BQ<=100,49
SymbEv:  MdBS:    B1= BM*499
ReqChk:  Waiting
SymbEv:  MdBS:    B1=
ReqChk:  Sent

SymbEv:  MdBS:    B1= 101*1
MakeRun: Waiting: BQ<=100,49
SymbEv:  MdBS:    B1=  99*1
ReqChk:  Sent

SymbEv:  MdBS:    B1= 101*1
MakeRun: Waiting: BQ<=100,49
SymbEv:  MdBS:    B1= 100*50
ReqChk:  Waiting
SymbEv:  MdBS:    B1= 100*49
ReqChk:  Sent

SymbEv:  MdBS:    B1= 101*1
MakeRun: Waiting: BQ<=100,49
SymbEv:  MdBS:    B1= 100*48
ReqChk:  Sent
# --------------- BQ<= M,49      Pri:市價,Qty:49
SymbEv:  MdBS:    B1= BM*499
MakeRun: Waiting: BQ<= M,49
SymbEv:  MdBS:    B1= BM*50
ReqChk:  Waiting
SymbEv:  MdBS:    B1= BM*49
ReqChk:  Sent

SymbEv:  MdBS:    B1= BM*499
MakeRun: Waiting: BQ<= M,49
SymbEv:  MdBS:    B1= BM*48
ReqChk:  Sent

SymbEv:  MdBS:    B1=BM*499
MakeRun: Waiting: BQ<=M,49
SymbEv:  MdBS:    B1=100*499
ReqChk:  Sent

SymbEv:  MdBS:    B1= BM*499
MakeRun: Waiting: BQ<= M,49
SymbEv:  MdBS:    B1=
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "BQ <", R"(
SymbId:  1101
# --------------- Qty 不能為 0
MakeRun: ec=11150:BQ<100,0
# --------------- 必須有 Pri
MakeRun: ec=11103:BQ<
# --------------- BQ< 100.49     Pri:100,Qty:49
SymbEv:  MdBS:    B1= 101*1
MakeRun: Waiting: BQ< 100,49
SymbEv:  MdBS:    B1= BM*499
ReqChk:  Waiting
SymbEv:  MdBS:    B1=
ReqChk:  Sent

SymbEv:  MdBS:    B1= 101*1
MakeRun: Waiting: BQ< 100,49
SymbEv:  MdBS:    B1=  99*1
ReqChk:  Sent

SymbEv:  MdBS:    B1= 101*1
MakeRun: Waiting: BQ< 100,49
SymbEv:  MdBS:    B1= 100*50
ReqChk:  Waiting
SymbEv:  MdBS:    B1= 100*49
ReqChk:  Waiting
SymbEv:  MdBS:    B1= 100*48
ReqChk:  Sent
# --------------- BQ< M,49       Pri:市價,Qty:49
SymbEv:  MdBS:    B1=BM*499
MakeRun: Waiting: BQ< M,49
SymbEv:  MdBS:    B1=BM*50
ReqChk:  Waiting
SymbEv:  MdBS:    B1=BM*49
ReqChk:  Waiting
SymbEv:  MdBS:    B1=BM*48
ReqChk:  Sent

SymbEv:  MdBS:    B1=BM*499
MakeRun: Waiting: BQ< M,49
SymbEv:  MdBS:    B1=100*499
ReqChk:  Sent

SymbEv:  MdBS:    B1=BM*499
MakeRun: Waiting: BQ< M,49
SymbEv:  MdBS:    B1=
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "BQ >=", R"(
SymbId:  1101
# --------------- Qty 不能為 0
MakeRun: ec=11150:BQ>=100,0
# --------------- 必須有 Pri
MakeRun: ec=11103:BQ>=
# --------------- BQ>=100,49     Pri:100,Qty:49
SymbEv:  MdBS:    B1=  99*1
MakeRun: Waiting: BQ>=100,49
SymbEv:  MdBS:    B1=
ReqChk:  Waiting
SymbEv:  MdBS:    B1= BM*1
ReqChk:  Sent

SymbEv:  MdBS:    B1= 99*1
MakeRun: Waiting: BQ>=100,49
SymbEv:  MdBS:    B1= 101*1
ReqChk:  Sent

SymbEv:  MdBS:    B1=  99*1
MakeRun: Waiting: BQ>=100,49
SymbEv:  MdBS:    B1= 100*48
ReqChk:  Waiting
SymbEv:  MdBS:    B1= 100*49
ReqChk:  Sent

SymbEv:  MdBS:    B1=  99*1
MakeRun: Waiting: BQ>=100,49
SymbEv:  MdBS:    B1= 100*50
ReqChk:  Sent
# --------------- BQ>= M,49      Pri:市價,Qty:49
SymbEv:  MdBS:    B1= BM*5
MakeRun: Waiting: BQ>= M,49
SymbEv:  MdBS:    B1= BM*48
ReqChk:  Waiting
SymbEv:  MdBS:    B1= BM*49
ReqChk:  Sent

SymbEv:  MdBS:    B1= BM*5
MakeRun: Waiting: BQ>= M,49
SymbEv:  MdBS:    B1=
ReqChk:  Waiting
SymbEv:  MdBS:    B1=100*50
ReqChk:  Waiting
SymbEv:  MdBS:    B1= BM*50
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "BQ >", R"(
SymbId:  1101
# --------------- Qty 不能為 0
MakeRun: ec=11150:BQ> 100,0
# --------------- 必須有 Pri
MakeRun: ec=11103:BQ>
# --------------- BQ> 100,49     Pri:100,Qty:49
SymbEv:  MdBS:    B1=  99*1
MakeRun: Waiting: BQ> 100,49
SymbEv:  MdBS:    B1=
ReqChk:  Waiting
SymbEv:  MdBS:    B1=  BM*1
ReqChk:  Sent

SymbEv:  MdBS:    B1=  99*1
MakeRun: Waiting: BQ> 100,49
SymbEv:  MdBS:    B1= 100*49
ReqChk:  Waiting
SymbEv:  MdBS:    B1= 100*50
ReqChk:  Sent

SymbEv:  MdBS:    B1=  99*1
MakeRun: Waiting: BQ> 100,49
SymbEv:  MdBS:    B1= 101*1
ReqChk:  Sent
# --------------- BQ>  M,49       Pri:市價,Qty:49
SymbEv:  MdBS:    B1= BM*5
MakeRun: Waiting: BQ>  M,49
SymbEv:  MdBS:    B1=
ReqChk:  Waiting
SymbEv:  MdBS:    B1=100*50
ReqChk:  Waiting
SymbEv:  MdBS:    B1= BM*49
ReqChk:  Waiting
SymbEv:  MdBS:    B1= BM*50
ReqChk:  Sent
)");
}
void TestSQ(TestCore& testCore) {
   // 不支援 == 、 !=
   testCore.TestCase(TestCore::TestFlag_AutoAll, "SQ ==", R"(
SymbId:  1101
MakeRun: ec=11999:SQ==0
MakeRun: ec=11999:SQ==100
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "SQ !=", R"(
SymbId:  1101
MakeRun: ec=11999:SQ!=0
MakeRun: ec=11999:SQ!=100
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "SQ <=", R"(
SymbId:  1101
# --------------- Qty 不能為 0
MakeRun: ec=11150:SQ<=100,0
# --------------- 必須有 Pri
MakeRun: ec=11103:SQ<=
# --------------- SQ<=100,49     Pri:100,Qty:49
SymbEv:  MdBS:    S1= 101*1
MakeRun: Waiting: SQ<=100,49
SymbEv:  MdBS:    S1=
ReqChk:  Waiting
SymbEv:  MdBS:    S1= SM*1
ReqChk:  Sent

SymbEv:  MdBS:    S1= 101*1
MakeRun: Waiting: SQ<=100,49
SymbEv:  MdBS:    S1=  99*1
ReqChk:  Sent
# --------------- SQ<=Pri,Qty:   最佳賣價=設定價, 則:(最價賣價數量>=Qty);
SymbEv:  MdBS:    S1= 101*1
MakeRun: Waiting: SQ<=100,49
SymbEv:  MdBS:    S1= 100*48
ReqChk:  Waiting
SymbEv:  MdBS:    S1= 100*50
ReqChk:  Sent

SymbEv:  MdBS:    S1= 101*1
MakeRun: Waiting: SQ<=100,49
SymbEv:  MdBS:    S1= 100*49
ReqChk:  Sent
# --------------- SQ<= M,49      Pri:市價,Qty:49
SymbEv:  MdBS:    S1= SM*48
MakeRun: Waiting: SQ<= M,49
SymbEv:  MdBS:    S1=100*499
ReqChk:  Waiting
SymbEv:  MdBS:    S1=
ReqChk:  Waiting
SymbEv:  MdBS:    S1= SM*50
ReqChk:  Sent

SymbEv:  MdBS:    S1= SM*48
MakeRun: Waiting: SQ<= M,49
SymbEv:  MdBS:    S1= SM*49
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "SQ <", R"(
SymbId:  1101
# --------------- Qty 不能為 0
MakeRun: ec=11150:SQ<100,0
# --------------- 必須有 Pri
MakeRun: ec=11103:SQ<
# --------------- SQ< 100.49     Pri:100,Qty:49
SymbEv:  MdBS:    S1= 101*1
MakeRun: Waiting: SQ< 100,49
SymbEv:  MdBS:    S1=
ReqChk:  Waiting
SymbEv:  MdBS:    S1= SM*499
ReqChk:  Sent

SymbEv:  MdBS:    S1= 101*1
MakeRun: Waiting: SQ< 100,49
SymbEv:  MdBS:    S1=  99*1
ReqChk:  Sent
# --------------- SQ<Pri,Qty:    最佳賣價=設定價, 則:(最價賣價數量>Qty);
SymbEv:  MdBS:    S1= 101*1
MakeRun: Waiting: SQ< 100,49
SymbEv:  MdBS:    S1= 100*48
ReqChk:  Waiting
SymbEv:  MdBS:    S1= 100*49
ReqChk:  Waiting
SymbEv:  MdBS:    S1= 100*50
ReqChk:  Sent
# --------------- SQ< M,49       Pri:市價,Qty:49
SymbEv:  MdBS:    S1=SM*48
MakeRun: Waiting: SQ< M,49
SymbEv:  MdBS:    S1=SM*49
ReqChk:  Waiting
SymbEv:  MdBS:    S1=
ReqChk:  Waiting
SymbEv:  MdBS:    S1=100*499
ReqChk:  Waiting
SymbEv:  MdBS:    S1=SM*50
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "SQ >=", R"(
SymbId:  1101
# --------------- Qty 不能為 0
MakeRun: ec=11150:SQ>=100,0
# --------------- 必須有 Pri
MakeRun: ec=11103:SQ>=
# --------------- SQ>=100,49     Pri:100,Qty:49
SymbEv:  MdBS:    S1=  99*1
MakeRun: Waiting: SQ>=100,49
SymbEv:  MdBS:    S1=  SM*1
ReqChk:  Waiting
SymbEv:  MdBS:    S1=
ReqChk:  Sent

SymbEv:  MdBS:    S1=  99*1
MakeRun: Waiting: SQ>=100,49
SymbEv:  MdBS:    S1= 101*1
ReqChk:  Sent
# --------------- SQ>=Pri,Qty:   最佳賣價=Pri, 則:(最價賣價數量<=Qty);
SymbEv:  MdBS:    S1=  99*1
MakeRun: Waiting: SQ>=100,49
SymbEv:  MdBS:    S1= 100*50
ReqChk:  Waiting
SymbEv:  MdBS:    S1= 100*48
ReqChk:  Sent

SymbEv:  MdBS:    S1=  99*1
MakeRun: Waiting: SQ>=100,49
SymbEv:  MdBS:    S1= 100*49
ReqChk:  Sent
# --------------- SQ>= M,49      Pri:市價,Qty:49
SymbEv:  MdBS:    S1= SM*499
MakeRun: Waiting: SQ>= M,49
SymbEv:  MdBS:    S1= SM*50
ReqChk:  Waiting
SymbEv:  MdBS:    S1= SM*49
ReqChk:  Sent

SymbEv:  MdBS:    S1= SM*499
MakeRun: Waiting: SQ>= M,49
SymbEv:  MdBS:    S1= SM*48
ReqChk:  Sent

SymbEv:  MdBS:    S1= SM*499
MakeRun: Waiting: SQ>= M,49
SymbEv:  MdBS:    S1=
ReqChk:  Sent

SymbEv:  MdBS:    S1= SM*499
MakeRun: Waiting: SQ>= M,49
SymbEv:  MdBS:    S1=100*1
ReqChk:  Sent
)");
   testCore.TestCase(TestCore::TestFlag_AutoAll, "SQ >", R"(
SymbId:  1101
# --------------- Qty 不能為 0
MakeRun: ec=11150:SQ> 100,0
# --------------- 必須有 Pri
MakeRun: ec=11103:SQ>
# --------------- SQ> 100,49     Pri:100,Qty:49
SymbEv:  MdBS:    S1=  99*1
MakeRun: Waiting: SQ> 100,49
SymbEv:  MdBS:    S1=  SM*1
ReqChk:  Waiting
SymbEv:  MdBS:    S1=
ReqChk:  Sent

SymbEv:  MdBS:    S1=  99*1
MakeRun: Waiting: SQ> 100,49
SymbEv:  MdBS:    S1= 101*1
ReqChk:  Sent
# --------------- SQ>Pri,Qty:    最佳賣價=Pri, 則:(最價賣價數量<Qty);
SymbEv:  MdBS:    S1=  99*1
MakeRun: Waiting: SQ> 100,49
SymbEv:  MdBS:    S1= 100*49
ReqChk:  Waiting
SymbEv:  MdBS:    S1= 100*48
ReqChk:  Sent
# --------------- SQ>  M,49       Pri:市價,Qty:49
SymbEv:  MdBS:    S1= SM*499
MakeRun: Waiting: SQ>  M,49
SymbEv:  MdBS:    S1= SM*50
ReqChk:  Waiting
SymbEv:  MdBS:    S1= SM*49
ReqChk:  Waiting
SymbEv:  MdBS:    S1= SM*48
ReqChk:  Sent

SymbEv:  MdBS:    S1= SM*499
MakeRun: Waiting: SQ>  M,49
SymbEv:  MdBS:    S1=
ReqChk:  Sent

SymbEv:  MdBS:    S1= SM*499
MakeRun: Waiting: SQ>  M,49
SymbEv:  MdBS:    S1=100*1
ReqChk:  Sent
)");
}
// ======================================================================== //
int main(int argc, const char* argv[]) {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   //_CrtSetBreakAlloc(176);
#endif

   fon9::AutoPrintTestInfo utinfo{"OmsCx"};
   TestCore                testCore{argc, argv};

   f9omstw_Reg_Default_CxUnitCreator();
   //----------------
   TestLP(testCore);
   TestBP(testCore);
   TestSP(testCore);
   TestLQ(testCore);
   TestTQ(testCore);
   TestBQ(testCore);
   TestSQ(testCore);
   //----------------
   //測試 OmsCxExpr;
   testCore.TestCase(TestCore::TestFlag_AutoChkFireNow, "OmsCxExpr", R"(
SymbId:  1101
SymbSet: MdDeal:  LastPrice=|LastQty=|TotalQty=
SymbSet: MdBS:    B1=|S1=
MakeRun: Waiting: LP>100 | BP>90 & SP<100 & LQ==49 | BP>90 & SP<100 & TQ>29 | TQ>499
SymbEv:  MdDeal:  LastPrice=101|LastQty=1
ReqChk:  Sent

SymbSet: MdDeal:  LastPrice=|LastQty=|TotalQty=
SymbSet: MdBS:    B1=|S1=
MakeRun: Waiting: LP>100 | BP>90 & SP<100 & LQ==49 | BP>90 & SP<100 & TQ>29 | TQ>499
SymbEv:  MdDeal:  TotalQty=500
ReqChk:  Sent

SymbSet: MdDeal:  LastPrice=|LastQty=|TotalQty=
SymbSet: MdBS:    B1=|S1=
MakeRun: Waiting: LP>100 | BP>90 & SP<100 & LQ==49 | BP>90 & SP<100 & TQ>29 | TQ>499
SymbEv:  MdBS:    B1=91*1
ReqChk:  Waiting
SymbEv:  MdBS:    S1=99*1
ReqChk:  Waiting
SymbEv:  MdDeal:  LastQty=49
ReqChk:  Sent

SymbSet: MdDeal:  LastPrice=|LastQty=|TotalQty=
SymbSet: MdBS:    B1=|S1=
MakeRun: Waiting: LP>100 | BP>90 & SP<100 & LQ==49 | BP>90 & SP<100 & TQ>29 | TQ>499
SymbEv:  MdBS:    B1=91*1
ReqChk:  Waiting
SymbEv:  MdBS:    S1=99*1
ReqChk:  Waiting
SymbEv:  MdDeal:  TotalQty=30
ReqChk:  Sent
)");
   // 測試 CxExpr + UsrTrueLock
   testCore.TestCase(TestCore::TestFlag_AutoNone, "OmsCxExpr + UsrTrueLock", R"(
SymbId:  1101
SymbSet: MdDeal:  LastPrice=|LastQty=|TotalQty=
SymbSet: MdBS:    B1=|S1=
MakeRun: Waiting: LP>100 | BP>90 & SP<100 & LQ==49 | BP>90@ & SP<100 & TQ>29 | TQ>499
SymbEv:  MdBS:    B1=91*1
ReqChk:  Waiting
SymbEv:  MdBS:    B1=90*1
ReqChk:  Waiting
SymbEv:  MdBS:    S1=99*1
ReqChk:  Waiting
SymbEv:  MdDeal:  LastQty=49
ReqChk:  Waiting
SymbEv:  MdDeal:  TotalQty=30
ReqChk:  Sent
)");
   //----------------
   utinfo.PrintSplitter();
   return 0;
}
