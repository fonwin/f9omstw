// \file f9omstw/OmsRcServerFunc.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRcServerFunc_hpp__
#define __f9omstw_OmsRcServerFunc_hpp__
#include "f9omstw/OmsRcServer.hpp"
#include "f9omstw/OmsRc.h"
#include "fon9/FlowCounter.hpp"

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

   struct Handler : public fon9::intrusive_ref_counter<Handler>, public OmsRequestPolicyCfg {
      fon9_NON_COPY_NON_MOVE(Handler);
      OmsRequestPolicySP         RequestPolicy_;
      const fon9::io::DeviceSP   Device_;
      const OmsCoreSP            Core_;
      const ApiSesCfgSP          ApiSesCfg_;
      Handler(fon9::io::DeviceSP dev, const OmsRequestPolicyCfg& cfg, OmsCoreSP core, ApiSesCfgSP apicfg)
         : OmsRequestPolicyCfg(cfg)
         , Device_{std::move(dev)}
         , Core_{std::move(core)}
         , ApiSesCfg_{std::move(apicfg)} {
      }
      void ClearResource();
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
      OmsRxSNO OnRecover(OmsCore&, const OmsRxItem* item);
      void OnReport(OmsCore&, const OmsRxItem& item);
      void SendReport(const OmsRxItem& item);
      ApiSession* IsNeedReport(const OmsRxItem& item);
   };
   using HandlerSP = fon9::intrusive_ptr<Handler>;

   struct PolicyRunner : public HandlerSP {
      using HandlerSP::HandlerSP;
      /// core.RunCoreTask() handler. run in OmsCore.
      /// - MakePolicy(res, this->get());
      /// - to device thread, run (*this)(dev);
      void operator()(OmsResource& res);
      /// Device_->OpQueue_.AddTask() handler. run in device.
      /// - if (ses.Note.Handler_ == this)   SendConfig();
      void operator()(fon9::io::Device& dev);
   };

   struct PolicyConfig : OmsRequestPolicyCfg {
      std::string       TablesGridView_;
      fon9::FlowCounter FcReq_;
      unsigned          FcReqOverCount_{};
   };
   PolicyConfig   PolicyConfig_;
   HandlerSP      Handler_;
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
#endif//__f9omstw_OmsRcServerFunc_hpp__
