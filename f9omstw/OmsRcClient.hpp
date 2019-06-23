// \file f9omstw/OmsRcClient.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRcClient_hpp__
#define __f9omstw_OmsRcClient_hpp__
#include "f9omstw/OmsRc.h"
#include "fon9/rc/RcSession.hpp"
#include "fon9/HostId.hpp"

namespace f9omstw {

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
struct OmsRcClientConfig {
   std::string       OmsSeedPath_;
   fon9::HostId      HostId_{};
   std::string       LayoutsStr_;
   std::string       TablesStr_;
   fon9::TimeStamp   TDay_;
};
fon9_WARN_POP;

class OmsRcLayout : public fon9::seed::Tab, public f9_OmsRcLayout {
   fon9_NON_COPY_NON_MOVE(OmsRcLayout);
   using base = fon9::seed::Tab;
   /// 初始化 f9_OmsRcLayout
   void Initialize();

public:
   template <class... ArgsT>
   OmsRcLayout(ArgsT&&... args) : base{std::forward<ArgsT>(args)...} {
      this->Initialize();
   }
};
using OmsRcLayoutSP = fon9::intrusive_ptr<OmsRcLayout>;
using OmsRcLayouts = fon9::NamedIxMapNoRemove<OmsRcLayoutSP>;

class OmsRcClientNote : public fon9::rc::RcFunctionNote {
   fon9::TimeStamp   ChangingTDay_;
   OmsRcClientConfig Config_;
   OmsRcLayouts      ReqLayouts_;
   OmsRcLayouts      RptLayouts_;
   static void ParseLayout(fon9::StrView& cfgstr, OmsRcLayouts& layouts);

   friend class OmsRcClientAgent;
   void OnSessionLinkBroken(fon9::rc::RcSession& ses);
   void OnSessionLinkReady(fon9::rc::RcSession& ses);
   void OnRecvOmsOpResult(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param);
   void OnRecvConfig(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param);
   void OnRecvTDayChanged(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param);
   void SendTDayConfirm(fon9::rc::RcSession& ses);

public:
   void OnRecvFunctionCall(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param) override;

   const OmsRcClientConfig& Config() const {
      return this->Config_;
   }
   const OmsRcLayouts& ReqLayouts() const {
      return this->ReqLayouts_;
   }
   const OmsRcLayouts& RptLayouts() const {
      return this->RptLayouts_;
   }
};

} // namespaces
#endif//__f9omstw_OmsRcClient_hpp__
