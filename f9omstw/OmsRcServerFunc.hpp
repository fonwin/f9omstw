// \file f9omstw/OmsRcServerFunc.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRcServerFunc_hpp__
#define __f9omstw_OmsRcServerFunc_hpp__
#include "f9omstw/OmsRcServer.hpp"

namespace f9omstw {

enum class OmsRcOpKind : uint8_t {
   Config,

   /// C <- S.
   /// 如果 server 正在建立新的 OmsCore, 則 Client 可能先收到 TDayChanged 然後才收到 Config.
   /// 此時 client 應先將 TDay 保留, 等收到 Config 時, 如果 TDay 不一致, 則送出 TDayConfirm;
   TDayChanged = 1,
   /// C -> S.
   TDayConfirm = 1,

   ReportRecover,
   ReportSubscribe,
};

class OmsRcServerAgent : public fon9::rc::RcFunctionAgent {
   fon9_NON_COPY_NON_MOVE(OmsRcServerAgent);
   using base = fon9::rc::RcFunctionAgent;
public:
   const ApiSesCfgSP ApiSesCfg_;

   OmsRcServerAgent(ApiSesCfgSP cfg)
      : base{fon9::rc::RcFunctionCode::OmsApi}
      , ApiSesCfg_{std::move(cfg)} {
   }
   ~OmsRcServerAgent();

   void OnSessionApReady(ApiSession& ses) override;
   void OnSessionLinkBroken(ApiSession& ses) override;
   // void OnRecvFunctionCall(ApiSession& ses, fon9::rc::RcFunctionParam& param) override;
};

class OmsRcServerNote : public fon9::rc::RcFunctionNote {
   fon9_NON_COPY_NON_MOVE(OmsRcServerNote);
   friend class OmsRcServerAgent;

   struct Handler : public OmsRequestPolicy, public OmsRequestPolicyCfg {
      fon9_NON_COPY_NON_MOVE(Handler);
      const fon9::io::DeviceSP   Device_;
      const OmsCoreSP            Core_;
      Handler(fon9::io::DeviceSP dev, const OmsRequestPolicyCfg& cfg, OmsCoreSP core)
         : OmsRequestPolicyCfg(cfg)
         , Device_{std::move(dev)}
         , Core_{std::move(core)} {
      }
      void ClearResource();
   };
   using HandlerSP = fon9::intrusive_ptr<Handler>;

   struct PolicyRunner : public HandlerSP {
      using HandlerSP::HandlerSP;
      /// core.RunCoreTask() handler. run in OmsCore.
      void operator()(OmsResource& res);
      /// Device_->OpQueue_.AddTask() handler. run in device.
      void operator()(fon9::io::Device& dev);
   };

   HandlerSP            Handler_;
   OmsRequestPolicyCfg  PolicyCfg_;
   fon9::SubConn        SubrTDayChanged_{};

   void SendConfig(ApiSession& ses);
   void OnRecvOmsOp(ApiSession& ses, fon9::rc::RcFunctionParam& param);
   void OnRecvStartApi(ApiSession& ses);
   void OnRecvTDayConfirm(ApiSession& ses, fon9::rc::RcFunctionParam& param);
   void StartPolicyRunner(ApiSession& ses, OmsCore* core);

public:
   const ApiSesCfgSP    ApiSesCfg_;

   OmsRcServerNote(ApiSesCfgSP cfg, ApiSession& ses);
   ~OmsRcServerNote();

   void OnRecvFunctionCall(ApiSession& ses, fon9::rc::RcFunctionParam& param) override;
};

} // namespaces
#endif//__f9omstw_OmsRcServerFunc_hpp__
