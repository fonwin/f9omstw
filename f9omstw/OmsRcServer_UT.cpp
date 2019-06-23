// \file f9omstw/OmsRcServer_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "apps/f9omstw/UnitTestCore.hpp"
#include "f9omstw/OmsRcServerFunc.hpp"
#include "f9omstw/OmsPoIvListAgent.hpp"
#include "f9omstw/OmsPoUserRightsAgent.hpp"
#include "f9omstw/OmsRcClient.hpp"

#include "fon9/ConfigLoader.hpp"
#include "fon9/io/TestDevice.hpp"
#include "fon9/io/SimpleManager.hpp"
#include "fon9/rc/RcFuncConn.hpp"
#include "fon9/auth/SaslScramSha256Server.hpp"
#include "fon9/DefaultThreadPool.hpp"
//--------------------------------------------------------------------------//
class OmsRcClientSession : public fon9::rc::RcSession {
   fon9_NON_COPY_NON_MOVE(OmsRcClientSession);
   using base = fon9::rc::RcSession;
   std::string Password_;
public:
   OmsRcClientSession(fon9::rc::RcFunctionMgrSP funcMgr, fon9::StrView userid, std::string passwd)
      : base(std::move(funcMgr), fon9::rc::RcSessionRole::User)
      , Password_{std::move(passwd)} {
      this->SetUserId(userid);
   }
   fon9::StrView GetAuthPassword() const override {
      return fon9::StrView{&this->Password_};
   }
};
//--------------------------------------------------------------------------//
fon9_WARN_DISABLE_PADDING;
struct RcFuncMgr : public fon9::intrusive_ref_counter<RcFuncMgr>, public fon9::rc::RcFunctionMgr {
   fon9_NON_COPY_NON_MOVE(RcFuncMgr);
   RcFuncMgr(f9omstw::ApiSesCfgSP sesCfg, fon9::auth::AuthMgrSP authMgr) {
      this->Add(fon9::rc::RcFunctionAgentSP{new fon9::rc::RcFuncConnServer("f9rcServer.0", "fon9 RcServer", authMgr)});
      this->Add(fon9::rc::RcFunctionAgentSP{new fon9::rc::RcFuncSaslServer{authMgr}});
      this->Add(fon9::rc::RcFunctionAgentSP{new f9omstw::OmsRcServerAgent{sesCfg}});
   }
   RcFuncMgr() {
      this->Add(fon9::rc::RcFunctionAgentSP{new fon9::rc::RcFuncConnClient("f9rcCli.0", "fon9 RcClient Tester")});
      this->Add(fon9::rc::RcFunctionAgentSP{new fon9::rc::RcFuncSaslClient{}});
      this->Add(fon9::rc::RcFunctionAgentSP{new f9omstw::OmsRcClientAgent{}});
   }
   unsigned AddRef() override {
      return intrusive_ptr_add_ref(static_cast<fon9::intrusive_ref_counter<RcFuncMgr>*>(this));
   }
   unsigned Release() override {
      return intrusive_ptr_release(static_cast<fon9::intrusive_ref_counter<RcFuncMgr>*>(this));
   }
};
fon9_WARN_POP;
//--------------------------------------------------------------------------//
int main(int argc, char* argv[]) {
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
      SetConsoleCP(CP_UTF8);
      SetConsoleOutputCP(CP_UTF8);
      // setvbuf(stdout, nullptr, _IOFBF, 10000); // for std::cout + UTF8;
   #endif
   fon9::AutoPrintTestInfo utinfo{"OmsRcServer"};
   //---------------------------------------------
   auto  core = TestCore::MakeCoreMgr(argc, argv);
   auto  coreMgr = core->Owner_;
   coreMgr->SetRequestFactoryPark(new f9omstw::OmsRequestFactoryPark(
      new OmsRequestTwsIniFactory("TwsNew", coreMgr->OrderFactoryPark().GetFactory("TwsOrd"),
                                  f9omstw::OmsRequestRunStepSP{new UomsTwsExgSender}),
      new OmsRequestTwsChgFactory("TwsChg", f9omstw::OmsRequestRunStepSP{new UomsTwsExgSender}),
      new OmsRequestTwsFilledFactory("TwsFilled", nullptr)
   ));
   const std::string fnDefault = "OmsRcServer.log";
   core->OpenReload(argc, argv, fnDefault);
   std::this_thread::sleep_for(std::chrono::milliseconds{100});
   //---------------------------------------------
   f9omstw::ApiSesCfgSP sesCfg;
   try {
      sesCfg = f9omstw::MakeApiSesCfg(coreMgr, "$TxLang={zh} $include:../../forms/ApiAll.cfg");
      // puts(sesCfg->ApiSesCfgStr_.c_str());
   }
   catch (fon9::ConfigLoader::Err& err) {
      std::cout << fon9::RevPrintTo<std::string>(err) << std::endl;
      abort();
   }
   utinfo.PrintSplitter();
   //---------------------------------------------
   #define kUSERID   "fonwin"
   #define kPASSWD   "PENCIL"
   #define kROLEID   "trader"
   // AuthMgr 必須有一個「有效的 InnDbf」儲存資料, 否則無法運作.
   static const char kUT_Dbf_FileName[] = "UnitTest.f9dbf";
   ::remove(kUT_Dbf_FileName);
   fon9::InnDbfSP authStorage{new fon9::InnDbf("userdbf", nullptr)};
   authStorage->Open(kUT_Dbf_FileName);
   // 建立 AuthMgr 驗證 Policy 設定.
   auto authMgr = fon9::auth::AuthMgr::Plant(coreMgr, authStorage, "AuthMgr");
   auto userMgr = fon9::auth::PlantScramSha256(*authMgr);
   // 建立一個可登入的 kUSERID
   userMgr->GetSapling()->OnTreeOp([](const fon9::seed::TreeOpResult&, fon9::seed::TreeOp* opTree) {
      opTree->Add(kUSERID, [](const fon9::seed::PodOpResult& res, fon9::seed::PodOp* opPod) {
         auto tab = res.Sender_->LayoutSP_->GetTab(0);
         opPod->BeginWrite(*tab, [](const fon9::seed::SeedOpResult& opr, const fon9::seed::RawWr* wr) {
            opr.Tab_->Fields_.Get("RoleId")->StrToCell(*wr, kROLEID);
            opr.Tab_->Fields_.Get("Flags")->PutNumber(*wr, 0, 0);
         });
         opPod->OnSeedCommand(tab, "repw " kPASSWD, [](const fon9::seed::SeedOpResult&, fon9::StrView) {});
      });
   });
   // 建立可用帳號 PolicyAgent.
   auto poIvListAgent = f9omstw::OmsPoIvListAgent::Plant(*authMgr);
   poIvListAgent->GetSapling()->OnTreeOp([](const fon9::seed::TreeOpResult&, fon9::seed::TreeOp* opAgent) {
      opAgent->Add(kROLEID, [](const fon9::seed::PodOpResult& resPolicy, fon9::seed::PodOp* opPolicy) {
         auto tabPolicy = resPolicy.Sender_->LayoutSP_->GetTab(0);
         opPolicy->MakeSapling(*tabPolicy)->OnTreeOp([](const fon9::seed::TreeOpResult&, fon9::seed::TreeOp* opDetail) {
            auto fnSetIvRights = [](const fon9::seed::PodOpResult& resItem, fon9::seed::PodOp* opItem) {
               auto tabItem = resItem.Sender_->LayoutSP_->GetTab(0);
               opItem->BeginWrite(*tabItem, [](const fon9::seed::SeedOpResult& res, const fon9::seed::RawWr* wr) {
                  f9omstw::OmsIvRight rights = (res.KeyText_ == "*" ? f9omstw::OmsIvRight::AllowTradingAll : f9omstw::OmsIvRight{});
                  res.Tab_->Fields_.Get("Rights")->PutNumber(*wr, static_cast<uint8_t>(rights), 0);
               });
            };
            opDetail->Add("*", fnSetIvRights);
            opDetail->Add("8610-10", fnSetIvRights);
         });
      });
   });
   // 建立可用櫃號 PolicyAgent.
   auto poUserRightsAgent = f9omstw::OmsPoUserRightsAgent::Plant(*authMgr);
   poUserRightsAgent->GetSapling()->OnTreeOp([](const fon9::seed::TreeOpResult&, fon9::seed::TreeOp* opAgent) {
      opAgent->Add(kROLEID, [](const fon9::seed::PodOpResult& resPolicy, fon9::seed::PodOp* opPolicy) {
         auto tabPolicy = resPolicy.Sender_->LayoutSP_->GetTab(0);
         opPolicy->BeginWrite(*tabPolicy, [](const fon9::seed::SeedOpResult& opr, const fon9::seed::RawWr* wr) {
            opr.Tab_->Fields_.Get("OrdTeams")->StrToCell(*wr, "A");
         });
      });
   });
   //---------------------------------------------
   using ApiSessionSP = fon9::intrusive_ptr<f9omstw::ApiSession>;
   using TestDevSP = fon9::io::TestDevice2::PeerSP;
   using RcFuncMgrSP = fon9::rc::RcFunctionMgrSP;
   auto           iomgr = fon9::io::ManagerCSP{new fon9::io::SimpleManager};
   RcFuncMgrSP    rcFuncMgrSvr{new RcFuncMgr(sesCfg, authMgr)};
   ApiSessionSP   sesSvr{new f9omstw::ApiSession(rcFuncMgrSvr, fon9::rc::RcSessionRole::Host)};
   TestDevSP      devSvr{new fon9::io::TestDevice2(sesSvr, iomgr)};
   devSvr->Initialize();
   devSvr->AsyncOpen("dev.server");
   devSvr->WaitGetDeviceId();
   //---------------------------------------------
   // Client 登入.
   RcFuncMgrSP    rcFuncMgrCli{new RcFuncMgr};
   ApiSessionSP   sesCli{new OmsRcClientSession(rcFuncMgrCli, kUSERID, kPASSWD)};
   TestDevSP      devCli{new fon9::io::TestDevice2(sesCli, iomgr)};
   devCli->Initialize();
   devCli->SetPeer(devSvr);
   devCli->AsyncOpen("dev.client");
   // 等候 Client 登入完成.
   while (sesCli->GetSessionSt() != fon9::rc::RcSessionSt::ApReady
       || sesSvr->GetSessionSt() != fon9::rc::RcSessionSt::ApReady) {
      std::this_thread::yield();
   }
   //---------------------------------------------
   // 測試 TDayChanged.
   const fon9::TimeStamp tday = fon9::TimeStampResetHHMMSS(fon9::UtcNow() + fon9::GetLocalTimeZoneOffset());
   unsigned forceTDay = 0;
   auto fnMakeCore = [&core, coreMgr, &forceTDay, &argc, argv, &fnDefault]() {
      core.reset(new TestCore(argc, argv, fon9::RevPrintTo<std::string>("ut_", ++forceTDay), coreMgr));
      core->OpenReload(argc, argv, fnDefault, forceTDay);
      coreMgr->Add(&core->GetResource());
   };
   fnMakeCore();
   fnMakeCore();
   // 測試在 TDayChanged event 時, 再次設定新的 core;
   #define kLastForceTDay  5
   fon9::SubConn subrTDay;
   coreMgr->TDayChangedEvent_.Subscribe(&subrTDay, [&fnMakeCore, &forceTDay](f9omstw::OmsCore&) {
      if (forceTDay < kLastForceTDay)
         fnMakeCore();
   });
   fnMakeCore();
   coreMgr->TDayChangedEvent_.Unsubscribe(&subrTDay);
   fon9::TimeStamp lastTDay = fon9::TimeStampResetHHMMSS(tday) + fon9::TimeInterval_Second(forceTDay);
   if (coreMgr->CurrentCore()->TDay() != lastTDay || kLastForceTDay != forceTDay) {
      std::cout << "Last CurrentCore.TDay not match|forceTDay=" << forceTDay
         << "|lastTDay=" << (lastTDay - tday).GetIntPart()
         << std::endl;
      abort();
   }
   //---------------------------------------------
   // 等候 Client 收到最後的 TDay config.
   f9omstw::OmsRcClientNote* cliNote = sesCli->GetNote<f9omstw::OmsRcClientNote>(fon9::rc::RcFunctionCode::OmsApi);
   while (cliNote->Config().TDay_ != lastTDay) {
      std::this_thread::yield();
   }
   if (cliNote->Config().LayoutsStr_ != sesCfg->ApiSesCfgStr_
       || cliNote->Config().HostId_ != fon9::LocalHostId_
       || cliNote->Config().OmsSeedPath_ != core->SeedPath_) {
      fon9_LOG_FATAL("OmsRcClientConfig error");
      abort();
   }
   //------------------------------------------------
   // 測試 Recover & Subscribe.
   const f9omstw::OmsRxItem* lastRxItem = nullptr;
   auto fnRptHandler = [&lastRxItem](f9omstw::OmsCore&, const f9omstw::OmsRxItem& item) {
      if (lastRxItem && lastRxItem->RxSNO() + 1 != item.RxSNO()) {
         fon9_LOG_FATAL("Report|expected.SNO=", lastRxItem->RxSNO() + 1, "|curr=", item.RxSNO());
         abort();
      }
      lastRxItem = &item;
   };
   core->ReportRecover(0, [&fnRptHandler, &lastRxItem](f9omstw::OmsCore& omsCore, const f9omstw::OmsRxItem* item)
                       -> f9omstw::OmsRxSNO {
      if (item == nullptr) {
         fon9_LOG_INFO("Recover end|LastSNO=", lastRxItem ? lastRxItem->RxSNO() : 0);
         omsCore.ReportSubject().Subscribe(fnRptHandler);
         return 0u;
      }
      if (lastRxItem && lastRxItem->RxSNO() + 1 != item->RxSNO()) {
         fon9_LOG_FATAL("Report|expected.SNO=", lastRxItem->RxSNO() + 1, "|curr=", item->RxSNO());
         abort();
      }
      lastRxItem = item;
      return item->RxSNO() + 1;
   });
   //------------------------------------------------
   #define PUT_FIELD_VAL(vect, fldIndex, val)       \
      if (fldIndex >= 0)                            \
         vect[static_cast<size_t>(fldIndex)] = val; \
   //------------------------------------------------
   const auto&                reqLayouts = cliNote->ReqLayouts();
   f9omstw::OmsRcLayout*      layout = reqLayouts.Get("TwsNew");
   std::vector<fon9::StrView> fldValues(layout->Fields_.size());
   size_t                     clOrdId = core->GetResource().Backend_.LastSNO();
   PUT_FIELD_VAL(fldValues, layout->Kind_,       "");
   PUT_FIELD_VAL(fldValues, layout->Market_,     "T");
   PUT_FIELD_VAL(fldValues, layout->SessionId_,  "N");
   PUT_FIELD_VAL(fldValues, layout->BrkId_,      "8610");
   PUT_FIELD_VAL(fldValues, layout->IvacNo_,     "10");
   PUT_FIELD_VAL(fldValues, layout->SubacNo_,    "");
   PUT_FIELD_VAL(fldValues, layout->IvacNoFlag_, "");
   PUT_FIELD_VAL(fldValues, layout->UsrDef_,     "UD001");
   PUT_FIELD_VAL(fldValues, layout->ClOrdId_,    "CL123");
   PUT_FIELD_VAL(fldValues, layout->Side_,       "B");
   PUT_FIELD_VAL(fldValues, layout->OType_,      "0");
   PUT_FIELD_VAL(fldValues, layout->Symbol_,     "2317");
   PUT_FIELD_VAL(fldValues, layout->PriType_,    "L");
   PUT_FIELD_VAL(fldValues, layout->Pri_,        "200");
   PUT_FIELD_VAL(fldValues, layout->Qty_,        "8000");
   PUT_FIELD_VAL(fldValues, layout->OrdNo_,      "");
   PUT_FIELD_VAL(fldValues, layout->PosEff_,     "");
   PUT_FIELD_VAL(fldValues, layout->TIF_,        "");
   PUT_FIELD_VAL(fldValues, layout->IniSNO_,     "");

   fon9::NumOutBuf bufClOrdId;
   auto fnSendReq = [&fldValues, sesCli, layout, &clOrdId, &bufClOrdId]() {
      fldValues[static_cast<unsigned>(layout->ClOrdId_)]
         .Reset(fon9::UIntToStrRev(bufClOrdId.end(), ++clOrdId), bufClOrdId.end());

      fon9::RevBufferList rbuf{128};
      auto ifldEnd = fldValues.cend();
      auto ifldBeg = fldValues.cbegin();
      for (;;) {
         fon9::RevPrint(rbuf, *--ifldEnd);
         if (ifldBeg == ifldEnd)
            break;
         fon9::RevPrint(rbuf, *fon9_kCSTR_CELLSPL);
      }
      fon9::ByteArraySizeToBitvT(rbuf, fon9::CalcDataSize(rbuf.cfront()));
      fon9::ToBitv(rbuf, static_cast<unsigned>(layout->GetIndex()) + 1u);
      sesCli->Send(fon9::rc::RcFunctionCode::OmsApi, std::move(rbuf));
   };
   auto fnWaitSendReq = [&lastRxItem, layout, &fldValues]() {
      // 等候全部測試結束: 「訂閱 or 回補」到最後一筆.
      for (;;) {
         if (const auto* ord = dynamic_cast<const f9omstw::OmsOrderRaw*>(lastRxItem)) {
            if (const auto* req = dynamic_cast<const f9omstw::OmsRequestTrade*>(ord->Request_))
               if (fon9::ToStrView(req->ClOrdId_) == fldValues[static_cast<unsigned>(layout->ClOrdId_)])
                  break;
         }
         std::this_thread::yield();
      }
   };
   // 測試瞬間大量下單的 throughput.
   for (auto L = core->TestCount_; L > 0; --L) {
      fnSendReq();
   }
   fnWaitSendReq();
   // 測試慢速下單的 latency.
   fon9::RevBufferList rbuf{128};
   fon9::RevPrint(rbuf, "----- Test latency sleep: 10ms -----\n");
   core->LogAppend(std::move(rbuf));
   for (auto L = 5u; L > 0; --L) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      fnSendReq();
   }
   fnWaitSendReq();

   fon9::RevPrint(rbuf, "----- Test latency sleep: 100ms -----\n");
   core->LogAppend(std::move(rbuf));
   for (auto L = 5u; L > 0; --L) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      fnSendReq();
   }
   fnWaitSendReq();
   //---------------------------------------------
   fon9_LOG_INFO("Test END.");
   devCli->ResetPeer();
   devCli->AsyncClose("dev.client.close");
   devSvr->AsyncClose("dev.server.close");
   authMgr->Storage_->Close();
   coreMgr->OnParentSeedClear();
   core->Backend_WaitForEndNow();
}
