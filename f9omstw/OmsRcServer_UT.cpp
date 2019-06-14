// \file f9omstw/OmsRcServer_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "apps/f9omstw/UnitTestCore.hpp"
#include "f9omstw/OmsRcServerFunc.hpp"
#include "f9omstw/OmsPoIvListAgent.hpp"
#include "f9omstw/OmsPoUserRightsAgent.hpp"
#include "fon9/ConfigLoader.hpp"
#include "fon9/io/TestDevice.hpp"
#include "fon9/rc/RcFuncConn.hpp"
#include "fon9/auth/SaslScramSha256Server.hpp"
#include "fon9/DefaultThreadPool.hpp"
//--------------------------------------------------------------------------//
fon9_WARN_DISABLE_PADDING;
struct RcFuncMgr : public fon9::intrusive_ref_counter<RcFuncMgr>, public fon9::rc::RcFunctionMgr {
   fon9_NON_COPY_NON_MOVE(RcFuncMgr);
   RcFuncMgr(f9omstw::ApiSesCfgSP sesCfg) {
      this->Add(fon9::rc::RcFunctionAgentSP{new f9omstw::RcFuncOmsRequest{sesCfg}});
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
   #endif
   fon9::AutoPrintTestInfo utinfo{"OmsRcServer"};
   //---------------------------------------------
   f9omstw::OmsCoreMgrSP   coreMgr = TestCore::MakeCoreMgr(argc, argv);
   coreMgr->SetRequestFactoryPark(new f9omstw::OmsRequestFactoryPark(
      new OmsRequestTwsIniFactory("TwsNew", coreMgr->OrderFactoryPark().GetFactory("TwsOrd"),
                                  f9omstw::OmsRequestRunStepSP{new UomsTwsExgSender}),
      new OmsRequestTwsChgFactory("TwsChg", f9omstw::OmsRequestRunStepSP{new UomsTwsExgSender}),
      new OmsRequestTwsFilledFactory("TwsFilled", nullptr)
   ));
   static_cast<TestCore*>(coreMgr->GetCurrentCore().get())->OpenReload(argc, argv, "OmsRcServer.log");
   std::this_thread::sleep_for(std::chrono::milliseconds{100});
   //---------------------------------------------
   f9omstw::ApiSesCfgSP sesCfg;
   try {
      sesCfg = f9omstw::MakeApiSesCfg(coreMgr, "$TxLang={zh} $include:../../forms/ApiAll.cfg");
      puts(sesCfg->ApiSesCfgStr_.c_str());
   }
   catch (fon9::ConfigLoader::Err& err) {
      std::cout << fon9::RevPrintTo<std::string>(err) << std::endl;
      abort();
   }
   utinfo.PrintSplitter();
   //---------------------------------------------
   using ApiSessionSP = fon9::intrusive_ptr<f9omstw::ApiSession>;
   fon9::rc::RcFunctionMgrSP  rcFuncMgr{new RcFuncMgr{sesCfg}};
   ApiSessionSP               apiSes{new f9omstw::ApiSession(rcFuncMgr, fon9::rc::RcSessionRole::Host)};
   fon9::io::TestDeviceSP     dev{new fon9::io::TestDevice{apiSes}};
   dev->Initialize();
   dev->AsyncOpen("devopen");
   dev->WaitGetDeviceId();
   //---------------------------------------------
   #define kUSERID   "fonwin"
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
         });
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
            opDetail->Add("*",       fnSetIvRights);
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
   struct SaslNote : public fon9::rc::RcServerNote_SaslAuth {
      fon9_NON_COPY_NON_MOVE(SaslNote);
      using base = fon9::rc::RcServerNote_SaslAuth;
      using base::base;
      void UpdateRoleConfig() {
         auto& authr = this->AuthSession_->GetAuthResult();
         authr.AuthcId_.assign(kUSERID);
         authr.RoleId_.assign(kROLEID);
         authr.UpdateRoleConfig();
      }
   };
   SaslNote* saslNote = new SaslNote(*apiSes, *authMgr, "SCRAM-SHA-256", std::string{});
   apiSes->ResetNote(fon9::rc::RcFunctionCode::SASL, fon9::rc::RcFunctionNoteSP{saslNote});
   saslNote->UpdateRoleConfig();
   //---------------------------------------------
   // 關閉 Hb timer.
   dev->CommonTimerRunAfter(fon9::TimeInterval_Day(1));
   // OnSaslDone(): 直接觸發 OnSessionApReady() 事件, 測試 Policy 的取得及設定.
   apiSes->OnSaslDone(fon9_Auth_Success, kUSERID);
   // 模擬 Client:
   // - query: OmsCoreMgr(Name)、TDate、layout、可用帳號、可用櫃號、流量參數....
   // - recover report, subscribe report.
   // - send request.
   // - ...
   // getchar();
   //---------------------------------------------
   dev->AsyncClose("");
   authMgr->Storage_->Close();
   coreMgr->OnParentSeedClear();
}
