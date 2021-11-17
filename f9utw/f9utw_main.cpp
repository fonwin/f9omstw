// \file f9utw/f9utw_main.cpp
// \author fonwinz@gmail.com
#include "fon9/framework/Framework.hpp"
#include "fon9/seed/Plugins.hpp"
#include "fon9/seed/SysEnv.hpp"
#include "f9omstw/OmsPoIvListAgent.hpp"
#include "f9omstw/OmsPoUserRightsAgent.hpp"
#include "proj_verinfo.h"

extern "C" fon9_API fon9::seed::PluginsDesc f9p_NamedIoManager;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_TcpServer;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_TcpClient;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_FileIO;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_Dgram;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_HttpSession; // & f9p_WsSeedVisitor & f9p_HttpStaticDispatcher;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_RcSessionServer;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_SeedImporter;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_RcSvServerAgent;
extern "C"          fon9::seed::PluginsDesc f9p_OmsRcServerAgent;

#ifdef F9CARD
extern "C"          fon9::seed::PluginsDesc f9p_F9Card;
#endif

void* ForceLinkSomething() {
//
// 提供 UtwOmsCore, 在 /MaPlugins 增加:
//    iOmsCore       UtwOmsCore
//
// 提供 OmsRc protocol:
// * 在 /MaPlugins 增加:
//    iOmsRcSvr       RcSessionServer   Name=OmsRcSvr|Desp=f9OmsRc Server|AuthMgr=MaAuth|AddTo=FpSession
//    iRcSvAgent      RcSvServerAgent   AddTo=FpSession/OmsRcSvr
//    iOmsRcSvrAgent  OmsRcServerAgent  OmsCore=omstws|Cfg=$TxLang={zh} $include:forms/ApiAll.cfg|AddTo=FpSession/OmsRcSvr
// * 在 /MaIo 增加:
//    iOmsRcSvr       OmsRcSvr    TcpServer   6601
//
   static const void* forceLinkList[]{
      #ifdef F9CARD
         &f9p_F9Card,
      #endif

      &f9p_HttpSession,
      &f9p_RcSessionServer, &f9p_OmsRcServerAgent, &f9p_RcSvServerAgent,
      &f9p_NamedIoManager, &f9p_TcpServer, &f9p_TcpClient, &f9p_FileIO, &f9p_Dgram,
      &f9p_SeedImporter
   };
   return forceLinkList;
}

int fon9sys_BeforeStart(fon9::Framework& fon9sys) {
   f9omstw::OmsPoIvListAgent::Plant(*fon9sys.MaAuth_);
   f9omstw::OmsPoUserRightsAgent::Plant(*fon9sys.MaAuth_);
   f9omstw::OmsPoUserDenysAgent::Plant(*fon9sys.MaAuth_);
   ForceLinkSomething();

   //
   // 在 /SysEnv/Version 設定版本資訊(proj_verinfo.h):
   //
   // - Windows 在「Build Events/Pre-Build Event/Command Line」設定:
   //    cd ..\..\..\f9utw
   //    call ..\..\fon9\make_proj_verinfo.bat
   //
   // - Linux 在 PROJECT_NAME/CMakeLists.txt 裡面執行:
   //    execute_process(COMMAND "../../fon9/make_proj_verinfo.sh" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
   //
   auto sysEnv = fon9sys.Root_->Get<fon9::seed::SysEnv>(fon9_kCSTR_SysEnv_DefaultName);
   fon9::seed::LogSysEnv(sysEnv->Add(new fon9::seed::SysEnvItem("Version", proj_VERSION)).get());

   return 0;
}

int main(int argc, char** argv) {
   return fon9::Fon9CoRun(argc, argv, &fon9sys_BeforeStart);
}
