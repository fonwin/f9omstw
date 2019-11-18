// \file f9omstw/OmsLogAnalyser.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9utw/UnitTestCore.hpp"
//--------------------------------------------------------------------------//
struct AnItem {
   const f9omstw::OmsRequestTrade* Request_;

   /// Client before send: OmsRcClient_UT.c#L330
   fon9::TimeStamp Ts0_{fon9::TimeStamp::Null()};
   fon9::TimeStamp Ts0() const { return this->Ts0_; }

   /// Server before parse: OmsRcServerFunc.cpp#L194
   fon9::TimeStamp Ts1() const { return this->Request_->CrTime(); }

   /// Server after first updated(Sending, Queuing, Reject...): OmsBackend.cpp#L202
   fon9::TimeStamp Ts2_{fon9::TimeStamp::Null()};
   fon9::TimeStamp Ts2() const { return this->Ts2_; }

   AnItem(const f9omstw::OmsRequestTrade* req, const f9omstw::OmsOrderRaw* ordupd)
      : Request_{req} {
      if (!req->UsrDef_.empty1st()) {
         this->Ts0_.SetOrigValue(fon9::StrTo(fon9::ToStrView(req->UsrDef_),
                                             this->Ts0_.GetOrigValue()));
      }
      if (ordupd)
         this->Ts2_ = ordupd->UpdateTime_;
   }
};
using AnItems = std::deque<AnItem>;

struct AnalyticCounter;
using GroupItems = std::map<fon9::StrView, AnalyticCounter>;

struct AnalyticCounter {
   uint32_t    Count_{0};
   uint32_t    CountAbandoned_{0};
   uint32_t    CountRejected_{0};
   uint32_t    CountSending_{0};
   uint32_t    CountQueuing_{0};
   char        padding___[4];
   AnItems     Items_;
   GroupItems  Groups_;

   void AddItem(const f9omstw::OmsRequestTrade* req, const f9omstw::OmsOrderRaw* ord) {
      ++this->Count_;
      if (req->IsAbandoned())
         ++this->CountAbandoned_;
      if (ord) {
         if (ord->RequestSt_ == f9fmkt_TradingRequestSt_Sending)
            ++this->CountSending_;
         else if (ord->RequestSt_ == f9fmkt_TradingRequestSt_Queuing)
            ++this->CountQueuing_;
         else if (f9fmkt_TradingRequestSt_IsFinishedRejected(ord->RequestSt_))
            ++this->CountRejected_;
      }
      this->Items_.emplace_back(req, ord);
   }
};

template <class FactoryBase>
struct FactoryWithCounter : public FactoryBase, public AnalyticCounter {
   fon9_NON_COPY_NON_MOVE(FactoryWithCounter);
   using FactoryBase::FactoryBase;
};
//--------------------------------------------------------------------------//
using Percent = fon9::Decimal<uint64_t, 6>;
static const Percent kPercentList[]{
   Percent{0.5},
   Percent{0.75},
   Percent{0.9},
   Percent{0.99},
   Percent{0.999},
   Percent{0.9999},
};
struct LatencyUS : public fon9::Decimal<fon9::TimeInterval::OrigType, 3> {
   using base = fon9::Decimal<OrigType, Scale>;
   using base::base;
   LatencyUS(fon9::TimeInterval src)
      : base(Make<6>(src.GetOrigValue() * 1000000)) {
   }
   LatencyUS(base src) : base(src) {
   }
   LatencyUS() {
   }
};
using Latencies = std::vector<LatencyUS>;

inline LatencyUS GetLatencyUS(const Latencies& latencies, size_t idx) {
   if (idx >= latencies.size())
      return LatencyUS::Null();
   return latencies[idx];
}
std::string OutputLatenciesSummary(fon9::StrView head, Latencies& latencies) {
   std::sort(latencies.begin(), latencies.end());
   fon9::RevBufferList rbuf{128};

   const size_t   iterations = latencies.size();
   size_t         L;
   LatencyUS      latus;
   for (L = 0; L < 3; ++L) {
      if (!(latus = GetLatencyUS(latencies, iterations - L - 1)).IsNull())
         fon9::RevPrint(rbuf, latus, '|');
   }
   fon9::RevPrint(rbuf, "Worst=");
   L = fon9::numofele(kPercentList);
   while(L > 0) {
      fon9::RevPrint(rbuf, '|');
      const auto per = kPercentList[--L];
      if (!(latus = GetLatencyUS(latencies, (iterations * per).GetIntPart())).IsNull())
         fon9::RevPrint(rbuf, latus);
      fon9::RevPrint(rbuf, fon9::Decimal<size_t, 6>(per * 100), "%=");
   }
   fon9::RevPrint(rbuf, head, '|');
   std::string res = fon9::BufferTo<std::string>(rbuf.MoveOut());
   puts(res.c_str());
   res.push_back('\n');
   return res;
}
void OutputTmDiff(fon9::RevBuffer& rbuf, Latencies& vect, fon9::TimeStamp af, fon9::TimeStamp bf) {
   if (!af.IsNull() && !bf.IsNull()) {
      vect.emplace_back(af - bf);
      fon9::RevPrint(rbuf, vect.back());
   }
   fon9::RevPrint(rbuf, '|');
}


void OutputAnalytic(const AnalyticCounter& ac, const std::string& resFileName, bool isGroup) {
   puts("------------------------------------------------------------------------");
   std::string head = fon9::RevPrintTo<std::string>(
      resFileName, "\n"
      "Count=", ac.Count_,
      "|Abandoned=", ac.CountAbandoned_,
      "|Rejected=", ac.CountRejected_,
      "|Sending=", ac.CountSending_,
      "|Queuing=", ac.CountQueuing_
      );
   puts(head.c_str());
   head.push_back('\n');
   fon9::File  fd;
   auto res = fd.Open(resFileName,
                      fon9::FileMode::Append | fon9::FileMode::Trunc | fon9::FileMode::OpenAlways);
   if (res.IsError()) {
      puts(fon9::RevPrintTo<std::string>("Open: ", resFileName, "\n", res).c_str());
      return;
   }
   fd.Append(&head);
   if (isGroup)
      fd.Append("ReqSNO|T0|T1|T2|(T1-T0)|(T2-T1)|T0-|T1-|T2-\n");
   else
      fd.Append("ReqSNO|T0|T1|T2|(T1-T0)|(T2-T1)\n");
   const auto  szItems = ac.Items_.size();
   Latencies t21, t10, t0g, t1g, t2g;
   t21.reserve(szItems);
   t10.reserve(szItems);
   if (isGroup) {
      t0g.reserve(szItems);
      t1g.reserve(szItems);
      t2g.reserve(szItems);
   }
   fon9::RevBufferFixedSize<1024 * 4> rbuf;
   // 可能因為 abandon, 造成沒有 t2, 所以額外記錄最後一個有效的 t2.
   fon9::TimeStamp   t2Last(fon9::TimeStamp::Null());
   size_t            t2LastIdx = 0;
   const AnItem*     prevItem = nullptr;
   for (size_t idx = 0; idx < szItems; ++idx) {
      const auto& curItem = ac.Items_[idx];
      rbuf.Rewind();
      const auto t0 = curItem.Ts0();
      const auto t1 = curItem.Ts1();
      const auto t2 = curItem.Ts2();
      if (!t2.IsNull()) {
         t2Last = t2;
         t2LastIdx = idx;
      }
      fon9::RevPrint(rbuf, '\n');
      if (isGroup && prevItem) {
         OutputTmDiff(rbuf, t2g, t2, prevItem->Ts2());
         OutputTmDiff(rbuf, t1g, t1, prevItem->Ts1());
         OutputTmDiff(rbuf, t0g, t0, prevItem->Ts0());
      }
      prevItem = &curItem;

      OutputTmDiff(rbuf, t21, t2, t1);
      OutputTmDiff(rbuf, t10, t1, t0);
      fon9::RevPrint(rbuf, curItem.Request_->RxSNO(), '|', t0, '|', t1, '|', t2);
      fd.Append(rbuf.GetCurrent(), rbuf.GetUsedSize());
   }
   if (isGroup) {
      rbuf.Rewind();
      const auto& ibeg = *ac.Items_.begin();
      const auto& iback = ac.Items_.back();
      const auto  sz = fon9::signed_cast(szItems);
      LatencyUS   spT0{iback.Ts0() - ibeg.Ts0()};
      LatencyUS   spT1{iback.Ts1() - ibeg.Ts1()};
      LatencyUS   spT2{t2Last - ibeg.Ts2()};
      LatencyUS   spT10{iback.Ts1() - ibeg.Ts0()};
      LatencyUS   spT20{t2Last - ibeg.Ts0()};
      LatencyUS   spT21{t2Last - ibeg.Ts1()};
      const auto  szT2 = fon9::signed_cast(t2LastIdx + 1);
      fon9::RevPrint(rbuf,
         "Spend(us)|", spT0,      '|', spT1,      '|', spT2, "\n"
         "Avg(us)|",   spT0 / sz, '|', spT1 / sz, '|', spT2 / szT2, "\n"
         "T1(last)-T0(first)=", spT10, '/', sz,   "|Avg=", spT10 / sz, "\n"
         "T2(last)-T0(first)=", spT20, '/', szT2, "|Avg=", spT20 / szT2, "\n"
         "T2(last)-T1(first)=", spT21, '/', szT2, "|Avg=", spT21 / szT2, "\n"
         );
      fd.Append(rbuf.GetCurrent(), rbuf.GetUsedSize());
      *const_cast<char*>(rbuf.GetMemEnd() - 1) = '\0'; // 使用 puts() 尾端不用換行.
      puts(rbuf.GetCurrent());
   }
   fd.Append(fon9::ToStrView(OutputLatenciesSummary("T1-T0", t10)));
   fd.Append(fon9::ToStrView(OutputLatenciesSummary("T2-T1", t21)));
   if (isGroup) {
      fd.Append(fon9::ToStrView(OutputLatenciesSummary("T0-", t0g)));
      fd.Append(fon9::ToStrView(OutputLatenciesSummary("T1-", t1g)));
      fd.Append(fon9::ToStrView(OutputLatenciesSummary("T2-", t2g)));
   }
}
//--------------------------------------------------------------------------//
int main(int argc, char* argv[]) {
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif
   if (argc <= 1) {
      puts("Usage: logFileName");
      return 3;
   }
   std::string logFileName = argv[1];
   // ------------------------------------------------------------------------
   struct OmsCoreMgrSP : public f9omstw::OmsCoreMgrSP {
      using base = f9omstw::OmsCoreMgrSP;
      using base::base;
      ~OmsCoreMgrSP() {
         this->get()->OnParentSeedClear();
      }
   };
   OmsCoreMgrSP         coreMgr{new f9omstw::OmsCoreMgr{nullptr}};
   OmsTwsOrderFactory*  twsOrdFactory = new OmsTwsOrderFactory("TwsOrd");
   OmsTwfOrder1Factory* twfOrd1Factory = new OmsTwfOrder1Factory("TwfOrd");
   OmsTwfOrder7Factory* twfOrd7Factory = new OmsTwfOrder7Factory("TwfOrdQR");
   OmsTwfOrder9Factory* twfOrd9Factory = new OmsTwfOrder9Factory("TwfOrdQ");
   coreMgr->SetOrderFactoryPark(new f9omstw::OmsOrderFactoryPark(
      twsOrdFactory, twfOrd1Factory, twfOrd9Factory, twfOrd7Factory
   ));
   f9omstw::OmsRequestFactoryPark* facpark;
   coreMgr->SetRequestFactoryPark(facpark = new f9omstw::OmsRequestFactoryPark(
      new FactoryWithCounter<OmsTwsRequestIniFactory>("TwsNew", twsOrdFactory, nullptr),
      new FactoryWithCounter<OmsTwsRequestChgFactory>("TwsChg", nullptr),
      new OmsTwsFilledFactory("TwsFil", twsOrdFactory),
      new OmsTwsReportFactory("TwsRpt", twsOrdFactory),

      new FactoryWithCounter<OmsTwfRequestIni1Factory>("TwfNew", twfOrd1Factory, nullptr),
      new FactoryWithCounter<OmsTwfRequestChg1Factory>("TwfChg", nullptr),
      new OmsTwfFilled1Factory("TwfFil", twfOrd1Factory, twfOrd9Factory),
      new OmsTwfFilled2Factory("TwfFil2", twfOrd1Factory),
      new OmsTwfReport2Factory("TwfRpt", twfOrd1Factory),

      new FactoryWithCounter<OmsTwfRequestIni9Factory>("TwfNewQ", twfOrd9Factory, nullptr),
      new FactoryWithCounter<OmsTwfRequestChg9Factory>("TwfChgQ", nullptr),
      new OmsTwfReport9Factory("TwfRptQ", twfOrd9Factory),

      new FactoryWithCounter<OmsTwfRequestIni7Factory>("TwfNewQR", twfOrd7Factory, nullptr),
      new OmsTwfReport8Factory("TwfRptQR", twfOrd7Factory)
      ));
   coreMgr->SetEventFactoryPark(new f9omstw::OmsEventFactoryPark(
   ));
   // ------------------------------------------------------------------------
   struct OmsCore : public f9omstw::OmsCore {
      fon9_NON_COPY_NON_MOVE(OmsCore);
      using base = f9omstw::OmsCore;
      OmsCore(f9omstw::OmsCoreMgrSP coreMgr)
         : base(coreMgr, "seed/path", "core") {
         coreMgr->Add(this);
         using namespace f9omstw;
         this->Symbs_.reset(new OmsSymbTree(*this, UtwsSymb::MakeLayout(OmsSymbTree::DefaultTreeFlag()), &UtwsSymb::SymbMaker));
         this->Brks_.reset(new OmsBrkTree(*this, UtwsBrk::MakeLayout(OmsBrkTree::DefaultTreeFlag()), &OmsBrkTree::TwsBrkIndex1));
         // 分析 omslog 的效率, 不會用到「委託書號」對照表, 所以不用設定券商.
         this->Brks_->Initialize(&UtwsBrk::BrkMaker, "0000", 1u, &f9omstw_IncStrAlpha);
         // 建立委託書號表的關聯.
         this->Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwSEC);
         this->Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwOTC);
         this->Brks_->InitializeTwfOrdNoMap(f9fmkt_TradingMarket_TwFUT);
         this->Brks_->InitializeTwfOrdNoMap(f9fmkt_TradingMarket_TwOPT);
      }
      ~OmsCore() {
         this->Brks_->InThr_OnParentSeedClear();
      }
      bool MoveToCoreImpl(f9omstw::OmsRequestRunner&&) override { return true; }
      void RunCoreTask(f9omstw::OmsCoreTask&&) override {}
      f9omstw::OmsResource& GetResource() { return *this; }
      using base::Backend_;
   };
   OmsCore* core = new OmsCore{coreMgr};
   auto     res = core->Backend_.OpenReload(logFileName, core->GetResource(), fon9::FileMode::Read);
   if (res.IsError()) {
      puts(fon9::RevPrintTo<std::string>("Open OmsLog: ", logFileName, "\n", res).c_str());
      return 3;
   }
   // ------------------------------------------------------------------------
   const f9omstw::OmsRxSNO snoLast = core->Backend_.LastSNO();
   for (f9omstw::OmsRxSNO sno = 0; sno <= snoLast; ++sno) {
      const auto* req = dynamic_cast<const f9omstw::OmsRequestTrade*>(core->Backend_.GetItem(sno));
      if (req == nullptr)
         continue;
      AnalyticCounter* ac = dynamic_cast<AnalyticCounter*>(&req->Creator());
      if (ac == nullptr)
         continue;
      const auto* ord = dynamic_cast<const f9omstw::OmsOrderRaw*>(core->Backend_.GetItem(sno + 1));
      if(ord && req != &ord->Request())
         ord = nullptr;
      ac->AddItem(req, ord);
      fon9::StrView cid = fon9::ToStrView(req->ClOrdId_);
      if (const char* gend = cid.Find(':')) {
         cid.SetEnd(gend);
         if (!cid.empty()) {
            ac->Groups_[cid].AddItem(req, ord);
         }
      }
   }
   // ------------------------------------------------------------------------
   size_t idx = 0;
   while (const auto* fac = facpark->GetFactory(idx++)) {
      if (const auto* ac = dynamic_cast<const AnalyticCounter*>(fac)) {
         OutputAnalytic(*ac, logFileName + "." + fac->Name_ + ".txt", false);
         for(const GroupItems::value_type& ig : ac->Groups_) {
            OutputAnalytic(ig.second,
                           logFileName + "." + fac->Name_ + "." + ig.first.ToString() + ".txt",
                           true);
         }
      }
   }
   return 0;
}
