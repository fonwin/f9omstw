// \file f9omstw/OmsIoSessionFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsIoSessionFactory_hpp__
#define __f9omstw_OmsIoSessionFactory_hpp__
#include "fon9/framework/IoFactory.hpp"
#include "f9omstw/OmsCoreMgr.hpp"

namespace f9omstw {

class OmsIoSessionFactory : public fon9::SessionFactory {
   fon9_NON_COPY_NON_MOVE(OmsIoSessionFactory);
   using base = fon9::SessionFactory;
   fon9::SubConn  SubConnTDayChanged_{};

protected:
   virtual void OnTDayChanged(OmsCore& omsCore);
   /// 需由衍生者主動呼叫.
   /// 預設: this->OmsCoreMgr_->TDayChangedEvent_.Subscribe();
   void OnAfterCtor();

public:
   const OmsCoreMgrSP   OmsCoreMgr_;

   OmsIoSessionFactory(std::string name, OmsCoreMgrSP&& omsCoreMgr);
   ~OmsIoSessionFactory();

   // fon9::io::SessionSP CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& iocfg, std::string& errReason) override;
   // fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager&, const fon9::IoConfigItem&, std::string& errReason) override;
};
//--------------------------------------------------------------------------//
class OmsIoSessionFactoryConfigParser : public fon9::SessionFactoryConfigParser {
   fon9_NON_COPY_NON_MOVE(OmsIoSessionFactoryConfigParser);
   using base = SessionFactoryConfigParser;
   fon9::StrView  OmsCoreName_;

public:
   fon9::seed::PluginsHolder& PluginsHolder_;

   OmsIoSessionFactoryConfigParser(fon9::seed::PluginsHolder& pluginsHolder, std::string defaultName = std::string{})
      : base{std::move(defaultName)}
      , PluginsHolder_{pluginsHolder} {
   }
   ~OmsIoSessionFactoryConfigParser();

   bool Parse(const fon9::StrView& args) {
      return base::Parse(this->PluginsHolder_, args);
   }
   bool OnUnknownTag(fon9::seed::PluginsHolder& holder, fon9::StrView tag, fon9::StrView value) override;
   OmsCoreMgrSP GetOmsCoreMgr();
};

} // namespaces
#endif//__f9omstw_OmsIoSessionFactory_hpp__
