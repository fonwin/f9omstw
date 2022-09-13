// \file f9omsrc/OmsRcSyn.hpp
//
// 用 oms rc 協定, 連線到另一台 f9oms 收取回報, 並匯入本機.
//
// \author fonwinz@gmail.com
#ifndef __f9omsrc_OmsRcSyn_hpp__
#define __f9omsrc_OmsRcSyn_hpp__
#include "f9omstw/OmsRequestTrade.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "fon9/rc/RcClientSession.hpp"
#include <deque>

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

} // namespaces
#endif//__f9omsrc_OmsRcSyn_hpp__
