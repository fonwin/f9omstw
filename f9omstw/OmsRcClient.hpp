// \file f9omstw/OmsRcClient.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRcClient_hpp__
#define __f9omstw_OmsRcClient_hpp__
#include "f9omstw/OmsRc.h"
#include "f9omstw/OmsGvTable.hpp"
#include "fon9/rc/RcSession.hpp"
#include "fon9/FlowCounter.hpp"

namespace f9omstw {

class OmsRcClientSession : public fon9::rc::RcSession, public f9OmsRc_ClientSession {
   fon9_NON_COPY_NON_MOVE(OmsRcClientSession);
   using base = fon9::rc::RcSession;
   std::string Password_;
public:
   const f9OmsRc_ClientHandler   Handler_;
   fon9::FlowCounterThreadSafe   FcReq_;

   OmsRcClientSession(fon9::rc::RcFunctionMgrSP funcMgr, const f9OmsRc_ClientSessionParams* params);
   fon9::StrView GetAuthPassword() const override;

   void OnDevice_StateChanged(fon9::io::Device& dev, const fon9::io::StateChangedArgs& e) override;
   void OnDevice_StateUpdated(fon9::io::Device& dev, const fon9::io::StateUpdatedArgs& e) override;
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

struct OmsRcClientConfig : public f9OmsRc_ClientConfig {
   fon9_NON_COPY_NON_MOVE(OmsRcClientConfig);

   OmsRcClientConfig() {
      fon9::ZeroStruct(static_cast<f9OmsRc_ClientConfig*>(this));
   }
   fon9::TimeStamp   TDay_;
   std::string       OmsSeedPath_;
   std::string       LayoutsStr_;
   std::string       TablesStr_;

   void MoveFromRc(OmsRcClientConfig&& src) {
      memcpy(static_cast<f9OmsRc_ClientConfig*>(this), static_cast<f9OmsRc_ClientConfig*>(&src), sizeof(f9OmsRc_ClientConfig));
      this->OmsSeedPath_ = std::move(src.OmsSeedPath_);
      this->TDay_ = src.TDay_;
      // LayoutsStr_, TablesStr_ 需要額外處理.
   }
};

class OmsRcLayout : public fon9::seed::Tab, public f9OmsRc_Layout {
   fon9_NON_COPY_NON_MOVE(OmsRcLayout);
   using base = fon9::seed::Tab;
   using LayoutFieldVector = std::vector<f9OmsRc_LayoutField>;
   LayoutFieldVector LayoutFieldVector_;

   /// 初始化 f9OmsRc_Layout
   void Initialize();

public:
   template <class... ArgsT>
   OmsRcLayout(f9OmsRc_TableIndex layoutId, ArgsT&&... args) : base{std::forward<ArgsT>(args)...} {
      this->LayoutId_ = layoutId;
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

   using RptValues = std::vector<fon9_CStrView>;
   mutable RptValues   RptValues_;

   template <class... ArgsT>
   OmsRcRptLayout(fon9::StrView exParam, f9OmsRc_LayoutKind layoutKind, f9OmsRc_TableIndex layoutId, ArgsT&&... args)
      : base{layoutId, std::forward<ArgsT>(args)...}
      , ExParam_{exParam} {
      RptValues_.resize(this->Fields_.size());
      this->LayoutKind_ = layoutKind;
   }
};
fon9_WARN_POP;
using OmsRcRptLayoutSP = fon9::intrusive_ptr<OmsRcRptLayout>;
using OmsRcRptLayouts = std::vector<OmsRcRptLayoutSP>;

class OmsRcClientNote : public fon9::rc::RcFunctionNote {
   fon9_NON_COPY_NON_MOVE(OmsRcClientNote);
   fon9::TimeStamp   ChangingTDay_;
   OmsRcClientConfig Config_;
   std::string       ConfigGvTablesStr_;
   OmsGvTables       ConfigGvTables_;
   OmsRcReqLayouts   ReqLayouts_;
   OmsRcRptLayouts   RptLayouts_;
   std::vector<const f9OmsRc_Layout*>  RequestLayoutVector_;

   friend class OmsRcClientAgent;
   void OnSessionLinkBroken(fon9::rc::RcSession& ses);
   void OnSessionApReady(fon9::rc::RcSession& ses);
   void OnRecvOmsOpResult(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param);
   void OnRecvConfig(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param);
   void OnRecvTDayChanged(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param);
   void SendTDayConfirm(fon9::rc::RcSession& ses);

public:
   OmsRcClientNote() = default;

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

   static const OmsRcClientNote& CastFrom(const f9OmsRc_ClientConfig& cfg) {
      return fon9::ContainerOf(*const_cast<OmsRcClientConfig*>(static_cast<const OmsRcClientConfig*>(&cfg)),
                               &OmsRcClientNote::Config_);
   }
};

} // namespaces
#endif//__f9omstw_OmsRcClient_hpp__
