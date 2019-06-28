// \file f9omstw/OmsRcClient.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRcClient_hpp__
#define __f9omstw_OmsRcClient_hpp__
#include "f9omstw/OmsRc.h"
#include "fon9/rc/RcSession.hpp"

namespace f9omstw {

class OmsRcClientSession : public fon9::rc::RcSession, public f9OmsRc_ClientSession {
   fon9_NON_COPY_NON_MOVE(OmsRcClientSession);
   using base = fon9::rc::RcSession;
   std::string Password_;
public:
   const f9OmsRc_ClientHandler   Handler_;

   OmsRcClientSession(fon9::rc::RcFunctionMgrSP funcMgr, fon9::StrView userid, std::string passwd,
                      const f9OmsRc_ClientHandler& handler, const f9OmsRc_ClientSession& cfg);
   fon9::StrView GetAuthPassword() const override;
};

//--------------------------------------------------------------------------//

class OmsRcClientAgent : public fon9::rc::RcFunctionAgent {
   fon9_NON_COPY_NON_MOVE(OmsRcClientAgent);
   using base = fon9::rc::RcFunctionAgent;
public:
   OmsRcClientAgent() : base{fon9::rc::RcFunctionCode::OmsApi} {
   }
   ~OmsRcClientAgent();

   void OnSessionApReady(fon9::rc::RcSession& ses) override;
   void OnSessionLinkBroken(fon9::rc::RcSession& ses) override;
   // void OnRecvFunctionCall(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param) override;
};

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
struct OmsRcClientConfig : public f9OmsRc_ClientConfig {
   OmsRcClientConfig() {
      this->HostId_ = 0;
   }
   std::string       OmsSeedPath_;
   std::string       LayoutsStr_;
   std::string       TablesStr_;
   fon9::TimeStamp   TDay_;
};
fon9_WARN_POP;

class OmsRcLayout : public fon9::seed::Tab, public f9OmsRc_Layout {
   fon9_NON_COPY_NON_MOVE(OmsRcLayout);
   using base = fon9::seed::Tab;
   /// 初始化 f9OmsRc_Layout
   void Initialize();

public:
   template <class... ArgsT>
   OmsRcLayout(ArgsT&&... args) : base{std::forward<ArgsT>(args)...} {
      this->Initialize();
   }
};
using OmsRcLayoutSP = fon9::intrusive_ptr<OmsRcLayout>;
using OmsRcReqLayouts = fon9::NamedIxMapNoRemove<OmsRcLayoutSP>;

fon9_WARN_DISABLE_PADDING;
class OmsRcRptLayout : public OmsRcLayout {
   fon9_NON_COPY_NON_MOVE(OmsRcRptLayout);
   using base = OmsRcLayout;
public:
   /// "abandon", "event", "TwsNew", "TwsChg", "TwsFilled"
   const fon9::CharVector  ExParam_;
   const f9OmsRc_RptKind   RptKind_;

   using RptValues = std::vector<f9OmsRc_StrView>;
   mutable RptValues   RptValues_;

   template <class... ArgsT>
   OmsRcRptLayout(fon9::StrView exParam, f9OmsRc_RptKind rptKind, ArgsT&&... args)
      : base{std::forward<ArgsT>(args)...}
      , ExParam_{exParam}
      , RptKind_{rptKind} {
      RptValues_.resize(this->Fields_.size());
   }
};
fon9_WARN_POP;
using OmsRcRptLayoutSP = fon9::intrusive_ptr<OmsRcRptLayout>;
using OmsRcRptLayouts = std::vector<OmsRcRptLayoutSP>;

class OmsRcClientNote : public fon9::rc::RcFunctionNote {
   fon9::TimeStamp   ChangingTDay_;
   OmsRcClientConfig Config_;
   OmsRcReqLayouts   ReqLayouts_;
   OmsRcRptLayouts   RptLayouts_;

   friend class OmsRcClientAgent;
   void OnSessionLinkBroken(fon9::rc::RcSession& ses);
   void OnSessionApReady(fon9::rc::RcSession& ses);
   void OnRecvOmsOpResult(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param);
   void OnRecvConfig(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param);
   void OnRecvTDayChanged(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param);
   void SendTDayConfirm(fon9::rc::RcSession& ses);

public:
   void OnRecvFunctionCall(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param) override;

   const OmsRcClientConfig& Config() const {
      return this->Config_;
   }
   const OmsRcReqLayouts& ReqLayouts() const {
      return this->ReqLayouts_;
   }
   const OmsRcRptLayouts& RptLayouts() const {
      return this->RptLayouts_;
   }
};

} // namespaces
#endif//__f9omstw_OmsRcClient_hpp__
