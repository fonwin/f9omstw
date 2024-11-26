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
#include "fon9/rc/RcFuncConn.hpp"

namespace f9omstw {

struct RemoteReqMap : public fon9::intrusive_ref_counter<RemoteReqMap> {
   fon9_NON_COPY_NON_MOVE(RemoteReqMap);
   RemoteReqMap() = default;

   char  Panding___[2];
   /// 遠端是否為 TradingLine Asker 請求端?
   bool  IsRemoteAsker_{false};
   bool  IsReportRecovering_{true};

   using OmsRequestSP = fon9::intrusive_ptr<const OmsRequestBase>;
   using MapImpl = std::deque<OmsRequestSP>;
   using Map = fon9::MustLock<MapImpl>;
   using Locker = Map::Locker;
   Map                  MapSNO_;
   OmsRxSNO             LastSNO_{};
   f9rc_ClientSession*  Session_{};
};
using RemoteReqMapSP = fon9::intrusive_ptr<RemoteReqMap>;

using OmsRcSynRptFields = std::vector<const fon9::seed::Field*>;
struct OmsRcSynRptDefine {
   OmsRequestFactory*   RptFactory_{};
   OmsRcSynRptFields    Fields_;
};

class OmsRcSynClientSession : public fon9::rc::RcClientSession {
   fon9_NON_COPY_NON_MOVE(OmsRcSynClientSession);
   using base = fon9::rc::RcClientSession;
   fon9::HostId   HostId_;
   char           Padding____[4];

   using RptMap = std::vector<OmsRcSynRptDefine>;
   RptMap         RptMap_;

   static void f9OmsRc_CALL OnOmsRcSyn_Config(f9rc_ClientSession* ses, const f9OmsRc_ClientConfig* cfg);
   static void f9OmsRc_CALL OnOmsRcSyn_Report(f9rc_ClientSession* ses, const f9OmsRc_ClientReport* apiRpt);
   bool RemapReqUID(OmsRequestBase& rptReq, RemoteReqMap::Locker& srcMap, RemoteReqMap::OmsRequestSP* pSrcReqSP, OmsRxSNO srcRptSNO);

protected:
   RemoteReqMapSP RemoteMap_;
   OmsCoreSP      OmsCore_;

   /// 回報回補結束.
   /// - 若在 OnOmsRcSyn_LinkBroken() 之前, 仍在回補狀態, 則會先觸發 OnOmsRcSyn_ReportRecoverEnd(),
   ///   然後再觸發 OnOmsRcSyn_LinkBroken();
   /// - 預設 this->RemoteMap_->IsReportRecovering_ = false;
   virtual void OnOmsRcSyn_ReportRecoverEnd();
   /// 當 synSes 收到 f9OmsRc_ClientConfig, 且解析成功, 處理完畢後通知.
   /// 此時的 this->RemoteMap_ 及 this->OmsCore_ 都已準備好.
   /// 預設 do nothing.
   virtual void OnOmsRcSyn_ConfigReady();
   /// 當 LinkBroken 時通知.
   /// - 此時 this->RemoteMap_ 及 this->OmsCore_ 必定有效, 才會觸發通知.
   ///   但是在呼叫 OnOmsRcSyn_LinkBroken() 之前, 會先 this->RemoteMap_->Session_ = nullptr;
   ///   若在 OnOmsRcSyn_LinkBroken() 之前, 仍在回補狀態, 則會先觸發 OnOmsRcSyn_ReportRecoverEnd(),
   /// - 然後再觸發 OnOmsRcSyn_LinkBroken();
   /// - 預設 do nothing.
   virtual void OnOmsRcSyn_LinkBroken();
   /// 回報內容處理完畢, 預設: 透過 OmsRequestRunner 執行回報.
   /// 只有底下情況來到此處:
   /// - rptReq->ReportSt() > f9fmkt_TradingRequestSt_LastRunStep 的 新刪改查.
   /// - (rptReq->RxKind() == f9fmkt_RxKind_RequestNew) 且 (rptReq->OrdNo_ 非空白).
   virtual void OnOmsRcSyn_RunReport(OmsRequestSP rptReq, const OmsRequestBase* ref, const fon9::StrView& message);
   /// 沒有進入 OmsCore 的刪改查回報.
   /// rptReq->ReportSt() <= f9fmkt_TradingRequestSt_LastRunStep;
   /// 預設 do nothing.
   virtual void OnOmsRcSyn_PendingChgRpt(OmsRequestSP rptReq, const OmsRequestBase* ref, const f9OmsRc_ClientReport& apiRpt);

public:
   using base::base;

   static void InitClientSessionParams(f9OmsRc_ClientSessionParams& omsRcParams) {
      omsRcParams.FnOnConfig_ = &OmsRcSynClientSession::OnOmsRcSyn_Config;
      omsRcParams.FnOnReport_ = &OmsRcSynClientSession::OnOmsRcSyn_Report;
   }

   fon9::HostId GetHostId() const {
      return this->HostId_;
   }
   RemoteReqMapSP RemoteMap() const {
      return this->RemoteMap_;
   }

   bool OnDevice_BeforeOpen(fon9::io::Device& dev, std::string& cfgstr) override;
   void OnDevice_StateChanged(fon9::io::Device& dev, const fon9::io::StateChangedArgs& e) override;
};
using OmsRcSynClientSessionSP = fon9::intrusive_ptr<OmsRcSynClientSession>;
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
class OmsRcClientAgent; // "f9omsrc/OmsRcClient.hpp"

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
   char                    Padding_____[5];

   OmsRcSyn_SessionFactory(std::string name, OmsCoreMgrSP&& omsCoreMgr, f9OmsRc_RptFilter rptFilter,
                           OmsRcClientAgent* omsRcCliAgent, fon9::rc::RcFuncConnClient* connCliAgent);

   virtual OmsRcSynClientSessionSP CreateOmsRcSynClientSession(f9rc_ClientSessionParams& f9rcCliParams);
   fon9::io::SessionSP CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& iocfg, std::string& errReason) override;
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager&, const fon9::IoConfigItem&, std::string& errReason) override;

   HostMap::Locker LockHostMap() {
      return this->HostMap_.Lock();
   }
   RemoteReqMapSP FetchRemoteMap(fon9::HostId hostid);
   RemoteReqMapSP FindRemoteMap(fon9::HostId hostid);
   const RcSynRequestFactory* GetRptReportFactory(fon9::StrView layoutName) const;

   void AppendMapSNO(fon9::HostId srcHostId, OmsRxSNO srcSNO, const OmsRequestId& origId);

   class Creator : public OmsIoSessionFactoryConfigParser {
      fon9_NON_COPY_NON_MOVE(Creator);
      using base = OmsIoSessionFactoryConfigParser;
   protected:
      f9OmsRc_RptFilter RptFilter_{static_cast<f9OmsRc_RptFilter>(f9OmsRc_RptFilter_NoExternal | f9OmsRc_RptFilter_IncludeRcSynNew)};
      char              Padding___[6];
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
