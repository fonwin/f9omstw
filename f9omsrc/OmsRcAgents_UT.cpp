// \file f9omsrc/OmsRcServer_UT.cpp
//
// - 測試 Server 端載入 OmsRc API 設定檔: "../../f9omsrc/forms/ApiAll.cfg"
// - 使用 TestDevice2 對接, Server/Client 正常登入.
// - 要測試的 class:
//   - OmsRcServerAgent/Note;
//   - OmsRcClientAgent/Note;
// - 測試項目:
//   - Config 查詢.
//   - TDayChanged/Confirm: 瞬間多次.
//   - 下單/回報.
//
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9utw/UnitTestCore.hpp"
#include "f9omsrc/OmsRcServerFunc.hpp"
#include "f9omsrc/OmsRcClient.hpp"
#include "f9omstw/OmsPoIvListAgent.hpp"
#include "f9omstw/OmsPoUserRightsAgent.hpp"

#include "fon9/ConfigLoader.hpp"
#include "fon9/io/TestDevice.hpp"
#include "fon9/io/SimpleManager.hpp"
#include "fon9/rc/RcFuncConnServer.hpp"
#include "fon9/auth/SaslScramSha256Server.hpp"
#include "fon9/DefaultThreadPool.hpp"
//--------------------------------------------------------------------------//
struct RcFuncMgr : public fon9::rc::RcFunctionMgrRefCounter {
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
};
//--------------------------------------------------------------------------//
f9OmsRc_CoreTDay  CoreTDay_;
f9OmsRc_SNO       LastSNO_;
fon9::CharVector  LastClOrdId_;
void OnClientLinkEv(f9rc_ClientSession* ses, f9io_State st, fon9_CStrView info) {
   (void)ses; (void)st; (void)info;
}
void OnClientConfig(f9rc_ClientSession* ses, const f9OmsRc_ClientConfig* cfg) {
   if (f9OmsRc_IsCoreTDayChanged(&CoreTDay_, &cfg->CoreTDay_)) {
      LastSNO_ = 0;
      CoreTDay_ = cfg->CoreTDay_;
   }
   f9OmsRc_SubscribeReport(ses, cfg, LastSNO_ + 1, f9OmsRc_RptFilter_AllPass);
}
void OnClientReport(f9rc_ClientSession*, const f9OmsRc_ClientReport* rpt) {
   LastSNO_ = rpt->ReportSNO_;
   if (rpt->Layout_ && rpt->Layout_->IdxClOrdId_ >= 0) {
      const auto& val = rpt->FieldArray_[rpt->Layout_->IdxClOrdId_];
      LastClOrdId_.assign(val.Begin_, val.End_);
   }
}
//--------------------------------------------------------------------------//
int main(int argc, char* argv[]) {
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
      SetConsoleCP(CP_UTF8);
      SetConsoleOutputCP(CP_UTF8);
      // setvbuf(stdout, nullptr, _IOFBF, 10000); // for std::cout + UTF8;
   #endif
   fon9::AutoPrintTestInfo utinfo{"OmsRcAgent"};
   //---------------------------------------------
   auto  core = TestCore::MakeCoreMgr(argc, argv);
   auto  coreMgr = core->Owner_;
   auto  twsOrdFac = coreMgr->OrderFactoryPark().GetFactory("TwsOrd");
   auto  twfOrd1Fac = coreMgr->OrderFactoryPark().GetFactory("TwfOrd");
   auto  twfOrd7Fac = coreMgr->OrderFactoryPark().GetFactory("TwfOrdQR");
   auto  twfOrd9Fac = coreMgr->OrderFactoryPark().GetFactory("TwfOrdQ");
   coreMgr->SetRequestFactoryPark(new f9omstw::OmsRequestFactoryPark(
      new OmsTwsRequestIniFactory("TwsNew", twsOrdFac,
                                  f9omstw::OmsRequestRunStepSP{new UomsTwsIniRiskCheck(
                                     f9omstw::OmsRequestRunStepSP{new UomsTwsExgSender})}),
      new OmsTwsRequestChgFactory("TwsChg", f9omstw::OmsRequestRunStepSP{new UomsTwsExgSender}),
      new OmsTwsFilledFactory("TwsFil", twsOrdFac),
      new OmsTwsReportFactory("TwsRpt", twsOrdFac),

      new OmsTwfRequestIni1Factory("TwfNew", twfOrd1Fac, f9omstw::OmsRequestRunStepSP{new UomsTwfExgSender}),
      new OmsTwfRequestChg1Factory("TwfChg", f9omstw::OmsRequestRunStepSP{new UomsTwfExgSender}),
      new OmsTwfFilled1Factory("TwfFil", twfOrd1Fac, twfOrd9Fac),
      new OmsTwfFilled2Factory("TwfFil2", twfOrd1Fac),
      new OmsTwfReport2Factory("TwfRpt", twfOrd1Fac),

      new OmsTwfRequestIni9Factory("TwfNewQ", twfOrd9Fac, f9omstw::OmsRequestRunStepSP{new UomsTwfExgSender}),
      new OmsTwfRequestChg9Factory("TwfChgQ", f9omstw::OmsRequestRunStepSP{new UomsTwfExgSender}),
      new OmsTwfReport9Factory("TwfRptQ", twfOrd9Fac),

      new OmsTwfRequestIni7Factory("TwfNewQR", twfOrd7Fac, f9omstw::OmsRequestRunStepSP{new UomsTwfExgSender}),
      new OmsTwfReport8Factory("TwfRptQR", twfOrd7Fac)
      ));
   const std::string fnDefault = "OmsRcServer.log";
   core->OpenReload(argc, argv, fnDefault);
   std::this_thread::sleep_for(std::chrono::milliseconds{100});
   //---------------------------------------------
   f9omstw::ApiSesCfgSP sesCfg;
   try {
      sesCfg = f9omstw::MakeApiSesCfg(coreMgr, "$TxLang={zh} $include:../../f9omsrc/forms/ApiAll.cfg");
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
                  f9omstw::OmsIvRight rights = (res.KeyText_ == "*" ? f9omstw::OmsIvRight::AllowAll : f9omstw::OmsIvRight{});
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
   f9rc_ClientSessionParams  f9rcCliParams;
   memset(&f9rcCliParams, 0, sizeof(f9rcCliParams));
   f9rcCliParams.UserId_    = kUSERID;
   f9rcCliParams.Password_  = kPASSWD;
   f9rcCliParams.DevName_   = "";
   f9rcCliParams.DevParams_ = "";
   f9rcCliParams.UserData_  = nullptr;
   fon9::StrView  arg = fon9::GetCmdArg(argc, argv, "l", "log");
   if (!arg.empty())
      f9rcCliParams.LogFlags_ = static_cast<f9rc_ClientLogFlag>(fon9::HIntStrTo(arg, 0u));
   else
      f9rcCliParams.LogFlags_ = f9rc_ClientLogFlag_Config;

   f9OmsRc_ClientSessionParams   omsRcParams;
   f9OmsRc_InitClientSessionParams(&f9rcCliParams, &omsRcParams);

   f9rcCliParams.FnOnLinkEv_ = &OnClientLinkEv;
   omsRcParams.FnOnConfig_ = &OnClientConfig;
   omsRcParams.FnOnReport_ = &OnClientReport;

   using CliSessionSP = fon9::intrusive_ptr<fon9::rc::RcClientSession>;
   RcFuncMgrSP    rcFuncMgrCli{new RcFuncMgr};
   CliSessionSP   sesCli{new fon9::rc::RcClientSession(rcFuncMgrCli, &f9rcCliParams)};
   TestDevSP      devCli{new fon9::io::TestDevice2(sesCli, iomgr)};
   devCli->Initialize();
   devCli->AsyncOpen("dev.client");
   devCli->SetPeer(devSvr);
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
      core->IsWaitQuit_ = false;
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
   f9omstw::OmsRcClientNote* cliNote = sesCli->GetNote<f9omstw::OmsRcClientNote>(f9rc_FunctionCode_OmsApi);
   while (cliNote->Config().TDay_ != lastTDay) {
      std::this_thread::yield();
   }
   if (cliNote->Config().LayoutsStr_ != sesCfg->ApiSesCfgStr_
       || cliNote->Config().CoreTDay_.HostId_ != fon9::LocalHostId_
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
   PUT_FIELD_VAL(fldValues, layout->IdxKind_,       "");
   PUT_FIELD_VAL(fldValues, layout->IdxMarket_,     "T");
   PUT_FIELD_VAL(fldValues, layout->IdxSessionId_,  "N");
   PUT_FIELD_VAL(fldValues, layout->IdxBrkId_,      "8610");
   PUT_FIELD_VAL(fldValues, layout->IdxIvacNo_,     "10");
   PUT_FIELD_VAL(fldValues, layout->IdxSubacNo_,    "");
   PUT_FIELD_VAL(fldValues, layout->IdxIvacNoFlag_, "");
   PUT_FIELD_VAL(fldValues, layout->IdxUsrDef_,     "UD001");
   PUT_FIELD_VAL(fldValues, layout->IdxClOrdId_,    "CL123");
   PUT_FIELD_VAL(fldValues, layout->IdxSide_,       "B");
   PUT_FIELD_VAL(fldValues, layout->IdxOType_,      "0");
   PUT_FIELD_VAL(fldValues, layout->IdxSymbol_,     "2317");
   PUT_FIELD_VAL(fldValues, layout->IdxPriType_,    "L");
   PUT_FIELD_VAL(fldValues, layout->IdxPri_,        "200");
   PUT_FIELD_VAL(fldValues, layout->IdxQty_,        "8000");
   PUT_FIELD_VAL(fldValues, layout->IdxOrdNo_,      "");
   PUT_FIELD_VAL(fldValues, layout->IdxPosEff_,     "");
   PUT_FIELD_VAL(fldValues, layout->IdxTIF_,        "");

   fon9::NumOutBuf bufClOrdId;
   auto fnSendReq = [&fldValues, sesCli, layout, &clOrdId, &bufClOrdId]() {
      fldValues[static_cast<unsigned>(layout->IdxClOrdId_)]
         .Reset(fon9::UIntToStrRev(bufClOrdId.end(), ++clOrdId), bufClOrdId.end());
      f9OmsRc_SendRequestFields(sesCli.get(), layout, &fldValues[0].ToCStrView());
   };
   auto fnMakeSendReqStr = [&fldValues, layout, &clOrdId, &bufClOrdId]() {
      fldValues[static_cast<unsigned>(layout->IdxClOrdId_)]
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
      return fon9::BufferTo<std::string>(rbuf.MoveOut());
   };
   auto fnWaitSendReq = [&lastRxItem, layout, &fldValues]() {
      // 等候全部測試結束: 「訂閱 or 回補」到最後一筆.
      for (;;) {
         const f9omstw::OmsRequestTrade* req = dynamic_cast<const f9omstw::OmsRequestTrade*>(lastRxItem);
         if (req == nullptr) {
            if (const auto* ordraw = dynamic_cast<const f9omstw::OmsOrderRaw*>(lastRxItem))
               req = dynamic_cast<const f9omstw::OmsRequestTrade*>(&ordraw->Request());
         }
         if (req && fon9::ToStrView(req->ClOrdId_) == fldValues[static_cast<unsigned>(layout->IdxClOrdId_)])
            break;
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

   fon9::RevPrint(rbuf, "----- Test Abandon -----\n");
   // 在 user thread 的失敗: ErrCode=OmsErrCode_OrdNoMustEmpty#203, RxSNO()==0;
   PUT_FIELD_VAL(fldValues, layout->IdxOrdNo_, "A");
   fnSendReq();
   // 在 core thread 的失敗: ErrCode=OmsErrCode_Bad_BrkId#100
   PUT_FIELD_VAL(fldValues, layout->IdxOrdNo_, "");
   PUT_FIELD_VAL(fldValues, layout->IdxBrkId_, "9999");
   fnSendReq();
   fnWaitSendReq();
   //---------------------------------------------
   std::string reqStr = fnMakeSendReqStr();
   f9OmsRc_SendRequestString(sesCli.get(), layout, fon9_ToCStrView(reqStr));

   // 等 Client 收完回報.
   fon9_LOG_INFO("Waiting Client|LastClOrdId=", LastClOrdId_, "|LastSNO=", lastRxItem->RxSNO(), "|WaitFor.ClOrdId=", clOrdId);
   while (fon9::ToStrView(LastClOrdId_) != fldValues[static_cast<unsigned>(layout->IdxClOrdId_)])
      std::this_thread::yield();
   //---------------------------------------------
   fon9_LOG_INFO("Test END      |LastClOrdId=", LastClOrdId_);
   devCli->ResetPeer();
   devCli->AsyncClose("dev.client.close");
   devSvr->AsyncClose("dev.server.close");
   authMgr->Storage_->Close();
   coreMgr->OnParentSeedClear();
   core->Backend_OnBeforeDestroy();
   fon9::WaitDefaultThreadPoolQuit(fon9::GetDefaultThreadPool());
}
