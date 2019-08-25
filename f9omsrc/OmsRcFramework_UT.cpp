// \file f9omsrc/OmsRcFramework_UT.cpp
//
// - 在此 cpp 建立 Server端 測試環境
//   - TcpServer: "6601";
//   - user: "admin", "fonwin"
// - 然後呼叫 C 的測試程式(f9omsrc/OmsRcFramework_UT_C.c):
//    int OmsRcFramework_UT_C_main();
// - 沒有測試送交易所.
//
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9omsrc/OmsRc.h"
#include "f9omstw/OmsPoIvListAgent.hpp"
#include "f9omstw/OmsPoUserRightsAgent.hpp"

#include "fon9/TestTools.hpp"
#include "fon9/framework/Framework.hpp"
#include "fon9/framework/IoManagerTree.hpp"
#include "fon9/auth/SaslScramSha256Server.hpp"
//--------------------------------------------------------------------------//
struct UserDef {
   const char* UserId_;
   const char* Password_;
   const char* RoleId_;
   const char* IvList_;
   const char* OrdTeams_;
   const char* FcReqCount_;
   const char* FcReqMS_;
};
void MakeUser(fon9::auth::UserMgr& userMgr, const UserDef& userdef) {
   userMgr.GetSapling()->OnTreeOp([&userdef](const fon9::seed::TreeOpResult&, fon9::seed::TreeOp* opTree) {
      opTree->Add(fon9::StrView_cstr(userdef.UserId_),
                  [&userdef](const fon9::seed::PodOpResult& res, fon9::seed::PodOp* opPod) {
         auto tab = res.Sender_->LayoutSP_->GetTab(0);
         opPod->BeginWrite(*tab, [&userdef](const fon9::seed::SeedOpResult& opr, const fon9::seed::RawWr* wr) {
            opr.Tab_->Fields_.Get("RoleId")->StrToCell(*wr, fon9::StrView_cstr(userdef.RoleId_));
            opr.Tab_->Fields_.Get("Flags")->PutNumber(*wr, 0, 0);
         });
         std::string repwCmd = "repw ";
         repwCmd += userdef.Password_;
         opPod->OnSeedCommand(tab, &repwCmd, [](const fon9::seed::SeedOpResult&, fon9::StrView) {});
      });
   });
}
void MakePoIvacList(f9omstw::OmsPoIvListAgent& poIvListAgent, const UserDef& userdef) {
   poIvListAgent.GetSapling()->OnTreeOp([&userdef](const fon9::seed::TreeOpResult&, fon9::seed::TreeOp* opAgent) {
      opAgent->Add(fon9::StrView_cstr(userdef.RoleId_),
                   [&userdef](const fon9::seed::PodOpResult& resPolicy, fon9::seed::PodOp* opPolicy) {
         auto tabPolicy = resPolicy.Sender_->LayoutSP_->GetTab(0);
         opPolicy->MakeSapling(*tabPolicy)->OnTreeOp([&userdef](const fon9::seed::TreeOpResult&, fon9::seed::TreeOp* opDetail) {
            auto fnSetIvRights = [&userdef](const fon9::seed::PodOpResult& resItem, fon9::seed::PodOp* opItem) {
               auto tabItem = resItem.Sender_->LayoutSP_->GetTab(0);
               opItem->BeginWrite(*tabItem, [](const fon9::seed::SeedOpResult& res, const fon9::seed::RawWr* wr) {
                  f9omstw::OmsIvRight rights = (res.KeyText_ == "*" ? f9omstw::OmsIvRight::AllowAll : f9omstw::OmsIvRight{});
                  res.Tab_->Fields_.Get("Rights")->PutNumber(*wr, static_cast<uint8_t>(rights), 0);
               });
            };
            fon9::StrView ivList = fon9::StrView_cstr(userdef.IvList_);
            while (!fon9::StrTrimHead(&ivList).empty())
               opDetail->Add(fon9::StrFetchTrim(ivList, '|'), fnSetIvRights);
         });
      });
   });
}
void MakePoUserRights(f9omstw::OmsPoUserRightsAgent& poUserRightsAgent, const UserDef& userdef) {
   poUserRightsAgent.GetSapling()->OnTreeOp([&userdef](const fon9::seed::TreeOpResult&, fon9::seed::TreeOp* opAgent) {
      opAgent->Add(fon9::StrView_cstr(userdef.RoleId_),
                   [&userdef](const fon9::seed::PodOpResult& resPolicy, fon9::seed::PodOp* opPolicy) {
         auto tabPolicy = resPolicy.Sender_->LayoutSP_->GetTab(0);
         opPolicy->BeginWrite(*tabPolicy, [&userdef](const fon9::seed::SeedOpResult& opr, const fon9::seed::RawWr* wr) {
            opr.Tab_->Fields_.Get("OrdTeams")->StrToCell(*wr, fon9::StrView_cstr(userdef.OrdTeams_));
            opr.Tab_->Fields_.Get("FcReqCount")->StrToCell(*wr, fon9::StrView_cstr(userdef.FcReqCount_));
            opr.Tab_->Fields_.Get("FcReqMS")->StrToCell(*wr, fon9::StrView_cstr(userdef.FcReqMS_));
         });
      });
   });
}

//--------------------------------------------------------------------------//
const UserDef  usrAdmin = {"admin", "AdminPass", "adm", "*|8610-10", "A", "", ""};
const UserDef  usrFonwin = {"fonwin", "FonPass", "trader", "8610-10|8610-21", "", "5", "1000"};
//--------------------------------------------------------------------------//
static const char kRcUT_Dbf_FileName[] = "OmsRcUT.f9dbf";
extern "C" fon9_API fon9::seed::PluginsDesc f9p_NamedIoManager;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_TcpServer;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_RcSessionServer;
extern "C"          fon9::seed::PluginsDesc f9p_UtwsOmsCore;
extern "C"          fon9::seed::PluginsDesc f9p_OmsRcServerAgent;
void* ForceLinkSomething() {
   static const void* forceLinkList[]{
      &f9p_NamedIoManager, &f9p_TcpServer, &f9p_RcSessionServer,
      &f9p_UtwsOmsCore, &f9p_OmsRcServerAgent
   };
   return forceLinkList;
}

struct OmsRcFramework : private fon9::Framework {
   OmsRcFramework() {
      ForceLinkSomething();
      this->Initialize();
   }
   ~OmsRcFramework() {
      this->Dispose();
   }
   void Dispose() {
      this->DisposeForAppQuit();
      this->RemoveTestFiles();
   }
   static void RemoveTestFiles() {
      ::remove(kRcUT_Dbf_FileName);
   }
private:
   void Initialize() {
      this->Root_.reset(new fon9::seed::MaTree{"OmsRcUT"});
      // 開檔前先移除上次的測試檔, 避免遺留的資料影響測試結果.
      this->RemoveTestFiles();
      // AuthMgr 必須有一個「有效的 InnDbf」儲存資料, 否則無法運作.
      fon9::InnDbfSP authStorage{new fon9::InnDbf("userdbf", nullptr)};
      authStorage->Open(kRcUT_Dbf_FileName);
      // f9rc server 需要 AuthMgr 提供使用者驗證.
      this->MaAuth_ = fon9::auth::AuthMgr::Plant(this->Root_, authStorage, "AuthMgr");
      auto  userMgr = fon9::auth::PlantScramSha256(*this->MaAuth_);
      // 建立 users.
      MakeUser(*userMgr, usrAdmin);
      MakeUser(*userMgr, usrFonwin);
      // 建立可用帳號.
      auto poIvListAgent = f9omstw::OmsPoIvListAgent::Plant(*this->MaAuth_);
      MakePoIvacList(*poIvListAgent, usrAdmin);
      MakePoIvacList(*poIvListAgent, usrFonwin);
      // 建立可用櫃號、流量管制...
      auto poUserRightsAgent = f9omstw::OmsPoUserRightsAgent::Plant(*this->MaAuth_);
      MakePoUserRights(*poUserRightsAgent, usrAdmin);
      MakePoUserRights(*poUserRightsAgent, usrFonwin);

      // 依照一般啟動流程, 啟動 Server 端.
      this->MaPlugins_.reset(new fon9::seed::PluginsMgr(this->Root_, "Plugins"));
      this->Root_->Add(this->MaPlugins_, "Plant.Plugins");
      #define _ "\x01"
      this->MaPlugins_->LoadConfigStr(
      "Id" _ "Enabled" _ "FileName" _ "EntryName"       _ "Args\n"
      "iDevTcps" _ "Y" _ ""         _ "TcpServer"       _ "Name=TcpServer|AddTo=FpDevice\n"
      "iMaIo"    _ "Y" _ ""         _ "NamedIoManager"  _ "Name=MaIo|DevFp=FpDevice|SesFp=FpSession|SvcCfg='ThreadCount=1|Capacity=100'\n"
      // 啟動 OmsCoreMgr=UtwsOmsCore, Name="omstws"
      "iOmsTws"  _ "Y" _ ""         _ "UtwsOmsCore"     _ "BrkId=8610\n"
      "iRcSv"    _ "Y" _ ""         _ "RcSessionServer" _ "Name=RcSv|Desp=f9OmsRc Tester|AuthMgr=AuthMgr|AddTo=FpSession\n"
      // 將 OmsRcServerAgent 加入 RcSv:
      "iRcSvOms" _ "Y" _ "" _ "OmsRcServerAgent" _ "OmsCore=omstws"
                                                   "|Cfg=$TxLang={zh} $include:../../f9omsrc/forms/ApiAll.cfg"
                                                   "|AddTo=FpSession/RcSv\n"
      );
      auto ioMgr = this->Root_->GetSapling<fon9::IoManagerTree>("MaIo");
      ioMgr->LoadConfigStr(
         "Id"   _ "Enabled" _ "Sch" _ "Session" _ "SessionArgs" _ "Device"    _ "DeviceArgs\n"
         "RcSv" _ "Y"       _ ""    _ "RcSv"    _ ""            _ "TcpServer" _ "6601\n"
         , fon9::TimeInterval_Second(1)
      );
      #undef _
   }
};
//--------------------------------------------------------------------------//
extern "C" int OmsRcFramework_UT_C_main(int argc, char** argv);
//--------------------------------------------------------------------------//
int main(int argc, char* argv[]) {
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
      SetConsoleCP(CP_UTF8);
      SetConsoleOutputCP(CP_UTF8);
   #endif
   fon9::AutoPrintTestInfo utinfo{"OmsRcFramework"};
   //---------------------------------------------
   f9OmsRc_Initialize(NULL);
   OmsRcFramework framework;
   // 等候1秒: IoMgr 啟動 TcpServer.
   std::this_thread::sleep_for(std::chrono::seconds(1));

   int retval = OmsRcFramework_UT_C_main(argc, argv);

   framework.Dispose();
   f9OmsRc_Finalize();
   //---------------------------------------------
   return retval;
}
