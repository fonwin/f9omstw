// \file f9omsrc/OmsRcSyn.hpp
//
// 用 oms rc 協定, 連線到另一台 f9oms 收取回報, 並匯入本機.
//
// \author fonwinz@gmail.com
#ifndef __f9omsrc_OmsRcSyn_hpp__
#define __f9omsrc_OmsRcSyn_hpp__
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsIoSessionFactory.hpp"
#include "f9omsrc/OmsRc.h"
#include "fon9/rc/RcClientSession.hpp"

namespace f9omstw {

struct RemoteReqMap : public fon9::intrusive_ref_counter<RemoteReqMap> {
   fon9_NON_COPY_NON_MOVE(RemoteReqMap);
   RemoteReqMap() = default;

   char  Panding___[4];

   using OmsRequestSP = fon9::intrusive_ptr<const OmsRequestBase>;
   using MapImpl = std::deque<OmsRequestSP>;
   using Map = fon9::MustLock<MapImpl>;
   Map                  MapSNO_;
   OmsRxSNO             LastSNO_{};
   f9rc_ClientSession*  Session_{};
};
using RemoteReqMapSP = fon9::intrusive_ptr<RemoteReqMap>;

class OmsRcSynClientSession : public fon9::rc::RcClientSession {
   fon9_NON_COPY_NON_MOVE(OmsRcSynClientSession);
   using base = fon9::rc::RcClientSession;

public:
   using base::base;

   RemoteReqMapSP RemoteMapSP_;
   OmsCoreSP      OmsCore_;
   fon9::HostId   HostId_;
   char           Padding____[4];

   using RptFields = std::vector<const fon9::seed::Field*>;
   struct RptDefine {
      OmsRequestFactory*   RptFactory_{};
      RptFields            Fields_;
   };
   using RptMap = std::vector<RptDefine>;
   RptMap   RptMap_;

   bool OnDevice_BeforeOpen(fon9::io::Device& dev, std::string& cfgstr) override;
   void OnDevice_StateChanged(fon9::io::Device& dev, const fon9::io::StateChangedArgs& e) override;
};
//--------------------------------------------------------------------------//
enum RcSynFactoryKind : uint8_t {
   /// 一般回報要求: TwsRpt, TwfRpt...
   Report,
   /// 成交回報要求: TwsFil, TwfFil, TwfFil2...
   Filled,
   /// 母單回報要求: Factory_ 建立出來的 req->IsParentRequest()==true;
   Parent,
};
static inline RcSynFactoryKind GetRequestFactoryKind(OmsRequestFactory& fac) {
   if (auto req = fac.MakeReportIn(f9fmkt_RxKind_RequestNew, fon9::TimeStamp{}))
      if (req->IsParentRequest())
         return RcSynFactoryKind::Parent;
   return RcSynFactoryKind::Report;
}
struct RcSynRequestFactory {
   OmsRequestFactory*   Factory_{};
   RcSynFactoryKind     Kind_{};
   char                 Padding____[7];
   void Set(OmsRequestFactory& fac, RcSynFactoryKind kind) {
      this->Factory_ = &fac;
      this->Kind_ = kind;
   }
   void Set(OmsRequestFactory& fac) {
      this->Set(fac, GetRequestFactoryKind(fac));
   }
};
//--------------------------------------------------------------------------//
class OmsRcSyn_SessionFactory : public OmsIoSessionFactory, public fon9::rc::RcFunctionMgr {
   fon9_NON_COPY_NON_MOVE(OmsRcSyn_SessionFactory);
   using base = OmsIoSessionFactory;

   using RptFactoryMap = fon9::SortedVector<fon9::CharVector, RcSynRequestFactory>;
   RptFactoryMap  RptFactoryMap_;

   using HostMapImpl = fon9::SortedVector<fon9::HostId, RemoteReqMapSP>;
   using HostMap = fon9::MustLock<HostMapImpl>;
   HostMap        HostMap_;
   fon9::SubConn  SubConnTDayChanged_{};
   fon9::File     LogMapSNO_;

   friend unsigned intrusive_ptr_add_ref(const OmsRcSyn_SessionFactory* p) BOOST_NOEXCEPT {
      return intrusive_ptr_add_ref(static_cast<const SessionFactory*>(p));
   }
   friend unsigned intrusive_ptr_release(const OmsRcSyn_SessionFactory* p) BOOST_NOEXCEPT {
      return intrusive_ptr_release(static_cast<const SessionFactory*>(p));
   }
   unsigned RcFunctionMgrAddRef() override;
   unsigned RcFunctionMgrRelease() override;

   void OnParentTreeClear(fon9::seed::Tree&) override;
   void OnTDayChanged(OmsCore& omsCore) override;

   void ReloadLogMapSNO(OmsCore& omsCore, OmsBackend::Locker& coreLocker);
   void ReloadLastSNO(OmsCoreSP core);

   void RerunParentInCore(fon9::HostId hostid, OmsResource& resource);
   void OnSeedCommand(fon9::seed::SeedOpResult& res, fon9::StrView cmdln, fon9::seed::FnCommandResultHandler resHandler,
                      fon9::seed::MaTreeBase::Locker&& ulk, fon9::seed::SeedVisitor* visitor) override;

public:
   const f9OmsRc_RptFilter RptFilter_;
   bool                    IsOmsCoreRecovering_{};
   char                    Padding_____[6];

   OmsRcSyn_SessionFactory(std::string name, OmsCoreMgrSP&& omsCoreMgr, f9OmsRc_RptFilter rptFilter);

   fon9::io::SessionSP CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& iocfg, std::string& errReason) override;
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager&, const fon9::IoConfigItem&, std::string& errReason) override;

   RemoteReqMapSP FetchRemoteMap(fon9::HostId hostid);
   RemoteReqMapSP FindRemoteMap(fon9::HostId hostid);
   const RcSynRequestFactory* GetRptReportFactory(fon9::StrView layoutName) const;

   void AppendMapSNO(fon9::HostId srcHostId, OmsRxSNO srcSNO, const OmsRequestId& origId);

   class Creator : public OmsIoSessionFactoryConfigParser {
      fon9_NON_COPY_NON_MOVE(Creator);
      using base = OmsIoSessionFactoryConfigParser;
   protected:
      f9OmsRc_RptFilter RptFilter_{static_cast<f9OmsRc_RptFilter>(f9OmsRc_RptFilter_NoExternal | f9OmsRc_RptFilter_IncludeRcSynNew)};
      char              Padding___[7];
      /// 預設: return new OmsRcSyn_SessionFactory(this->Name_, std::move(omsCoreMgr), this->RptFilter_);
      virtual fon9::intrusive_ptr<OmsRcSyn_SessionFactory> CreateRcSynSessionFactory(OmsCoreMgrSP&& omsCoreMgr);
   public:
      using base::base;
      bool OnUnknownTag(fon9::seed::PluginsHolder& holder, fon9::StrView tag, fon9::StrView value) override;
      fon9::SessionFactorySP CreateSessionFactory() override;
   };
};

} // namespaces
#endif//__f9omsrc_OmsRcSyn_hpp__
