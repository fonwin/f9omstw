// \file f9omsrc/OmsRcClient.hpp
// \author fonwinz@gmail.com
#ifndef __f9omsrc_OmsRcClient_hpp__
#define __f9omsrc_OmsRcClient_hpp__
#include "f9omsrc/OmsRc.h"
#include "fon9/rc/RcSeedVisitor.hpp"
#include "fon9/rc/RcClientSession.hpp"
#include "fon9/FlowCounter.hpp"

namespace f9omstw {

class OmsRcClientAgent : public fon9::rc::RcClientFunctionAgent {
   fon9_NON_COPY_NON_MOVE(OmsRcClientAgent);
   using base = fon9::rc::RcClientFunctionAgent;
public:
   OmsRcClientAgent() : base{f9rc_FunctionCode_OmsApi} {
   }
   ~OmsRcClientAgent();

   void OnSessionCtor(fon9::rc::RcClientSession& ses, const f9rc_ClientSessionParams* params) override;
   void OnSessionApReady(fon9::rc::RcSession& ses) override;
   void OnSessionLinkBroken(fon9::rc::RcSession& ses) override;
};

//--------------------------------------------------------------------------//

struct OmsRcClientConfig : public f9OmsRc_ClientConfig {
   fon9_NON_COPY_NON_MOVE(OmsRcClientConfig);

   OmsRcClientConfig() {
      fon9::ZeroStruct(static_cast<f9OmsRc_ClientConfig*>(this));
   }
   OmsRcClientConfig(const f9OmsRc_ClientConfig* src) : f9OmsRc_ClientConfig(*src) {
   }
   fon9::TimeStamp   TDay_;
   std::string       OmsSeedPath_;
   std::string       LayoutsStr_;
   /// 收到的 Table 訊息, 保留著分隔符號.
   /// 未經過 fon9::rc::SvParseGvTables() 解析.
   std::string       OrigTablesStr_;

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
   /// "abandon", "event", "TwsNew", "TwsChg", "TwsFil"
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
   fon9::TimeStamp      ChangingTDay_;

   /// 提供給 fon9::rc::SvParseGvTables() 解析使用的訊息, 分隔符號變成 EOS.
   std::string          ConfigGvTablesStr_;
   fon9::rc::SvGvTables ConfigGvTables_;
   OmsRcClientConfig    Config_;

   OmsRcReqLayouts      ReqLayouts_;
   OmsRcRptLayouts      RptLayouts_;
   std::vector<const f9OmsRc_Layout*>  RequestLayoutVector_;

   friend class OmsRcClientAgent;
   void OnSessionLinkBroken(fon9::rc::RcSession& ses);
   void OnSessionApReady(fon9::rc::RcSession& ses);
   void OnRecvOmsOpResult(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param);
   void OnRecvConfig(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param);
   void OnRecvTDayChanged(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param);
   void SendTDayConfirm(fon9::rc::RcSession& ses);

public:
   const f9OmsRc_ClientSessionParams   Params_;
   fon9::FlowCounterThreadSafe         FcReq_;

   OmsRcClientNote(const f9OmsRc_ClientSessionParams& params) : Params_(params) {
   }

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
#endif//__f9omsrc_OmsRcClient_hpp__
