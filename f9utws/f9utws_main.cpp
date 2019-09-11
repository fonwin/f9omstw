// \file f9utws/f9utws_main.cpp
// \author fonwinz@gmail.com
#include "fon9/framework/Framework.hpp"
#include "fon9/seed/Plugins.hpp"
#include "f9omstw/OmsPoIvListAgent.hpp"
#include "f9omstw/OmsPoUserRightsAgent.hpp"

extern "C" fon9_API fon9::seed::PluginsDesc f9p_NamedIoManager;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_TcpServer;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_TcpClient;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_FileIO;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_Dgram;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_RcSessionServer;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_SeedImporter;
extern "C"          fon9::seed::PluginsDesc f9p_OmsRcServerAgent;
void* ForceLinkSomething() {
//
// 提供 UtwsOmsCore, 在 /MaPlugins 增加:
//    iOmsCore       UtwsOmsCore
//
// 提供 OmsRc protocol:
// * 在 /MaPlugins 增加:
//    iOmsRcSv       RcSessionServer   Name=OmsRcSvr|Desp=f9OmsRc Server|AuthMgr=MaAuth|AddTo=FpSession
//    iOmsRcSvAgent  OmsRcServerAgent  OmsCore=omstws|Cfg=$TxLang={zh} $include:forms/ApiAll.cfg|AddTo=FpSession/OmsRcSvr
// * 在 /MaIo 增加:
//    iOmsRcSv       OmsRcSvr    TcpServer   6601
//
   static const void* forceLinkList[]{
      &f9p_RcSessionServer, &f9p_OmsRcServerAgent,
      &f9p_NamedIoManager, &f9p_TcpServer, &f9p_TcpClient, &f9p_FileIO, &f9p_Dgram,
      &f9p_SeedImporter
   };
   return forceLinkList;
}

int fon9sys_BeforeStart(fon9::Framework& fon9sys) {
   f9omstw::OmsPoIvListAgent::Plant(*fon9sys.MaAuth_);
   f9omstw::OmsPoUserRightsAgent::Plant(*fon9sys.MaAuth_);
   ForceLinkSomething();
   return 0;
}

int main(int argc, char** argv) {
   return fon9::Fon9CoRun(argc, argv, &fon9sys_BeforeStart);
}
