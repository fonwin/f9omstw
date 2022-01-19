// \file f9omsrc/OmsRcServerFunc.hpp
// \author fonwinz@gmail.com
#ifndef __f9omsrc_OmsRcServerFunc_hpp__
#define __f9omsrc_OmsRcServerFunc_hpp__
#include "f9omsrc/OmsRcServer.hpp"
#include "f9omsrc/OmsRc.h"
#include "f9omstw/OmsPoRequest.hpp"

namespace f9omstw {

class OmsRcServerAgent : public fon9::rc::RcFunctionAgent {
   fon9_NON_COPY_NON_MOVE(OmsRcServerAgent);
   using base = fon9::rc::RcFunctionAgent;
public:
   const ApiSesCfgSP ApiSesCfg_;

   OmsRcServerAgent(ApiSesCfgSP cfg);
   ~OmsRcServerAgent();

   void OnSessionApReady(ApiSession& ses) override;
   void OnSessionLinkBroken(ApiSession& ses) override;
   // void OnRecvFunctionCall(ApiSession& ses, fon9::rc::RcFunctionParam& param) override;
};

fon9_WARN_DISABLE_PADDING;
class OmsRcServerNote : public fon9::rc::RcFunctionNote {
   fon9_NON_COPY_NON_MOVE(OmsRcServerNote);
   friend class OmsRcServerAgent;

   class Handler : public OmsPoRequestHandler {
      fon9_NON_COPY_NON_MOVE(Handler);
      using base = OmsPoRequestHandler;
      void OnRequestPolicyUpdated(OmsRequestPolicySP before) override;
   public:
      const fon9::io::DeviceSP   Device_;
      const ApiSesCfgSP          ApiSesCfg_;
      Handler(fon9::io::DeviceSP dev, const OmsRequestPolicyCfg& cfg, OmsCoreSP core, ApiSesCfgSP apicfg,
              const fon9::StrView& userId)
         : base(cfg, std::move(core), userId)
         , Device_{std::move(dev)}
         , ApiSesCfg_{std::move(apicfg)} {
      }
      void ClearResource() override;
      void StartRecover(OmsRxSNO from, f9OmsRc_RptFilter filter);
   private:
      enum class HandlerSt {
         Preparing,
         Recovering,
         Reporting,
         Disposing,
      };
      HandlerSt         State_{};
      f9OmsRc_RptFilter RptFilter_;
      fon9::SubConn     RptSubr_{};
      OmsRxSNO          WkoRecoverSNO_{};
      void SubscribeReport();
      OmsRxSNO OnRecover(OmsCore&, const OmsRxItem* item);
      void OnReport(OmsCore&, const OmsRxItem& item);
      void SendReport(const OmsRxItem& item);
      ApiSession* IsNeedReport(const OmsRxItem& item);
   };
   using HandlerSP = fon9::intrusive_ptr<Handler>;
   HandlerSP   Handler_;

   struct PolicyConfig : OmsPoRequestConfig {
      std::string TablesGridView_;
   };
   PolicyConfig   PolicyConfig_;

   fon9::SubConn  SubrTDayChanged_{};
   unsigned       ConfigSentCount_{};

   bool CheckApiReady(ApiSession& ses);
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
fon9_WARN_POP;

} // namespaces
#endif//__f9omsrc_OmsRcServerFunc_hpp__
