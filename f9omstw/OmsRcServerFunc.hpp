// \file f9omstw/OmsRcServerFunc.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRcServerFunc_hpp__
#define __f9omstw_OmsRcServerFunc_hpp__
#include "f9omstw/OmsRcServer.hpp"

namespace f9omstw {

class RcFuncOmsRequest : public fon9::rc::RcFunctionAgent {
   fon9_NON_COPY_NON_MOVE(RcFuncOmsRequest);
   using base = fon9::rc::RcFunctionAgent;
public:
   const ApiSesCfgSP ApiSesCfg_;

   RcFuncOmsRequest(ApiSesCfgSP cfg)
      : base{fon9::rc::RcFunctionCode::OmsRequest}
      , ApiSesCfg_{std::move(cfg)} {
   }
   ~RcFuncOmsRequest();

   void OnSessionApReady(ApiSession& ses) override;
   void OnSessionLinkBroken(ApiSession& ses) override;
   // void OnRecvFunctionCall(ApiSession& ses, fon9::rc::RcFunctionParam& param) override;
};

fon9_WARN_DISABLE_PADDING;
class RcFuncOmsRequestNote : public fon9::rc::RcFunctionNote {
   fon9_NON_COPY_NON_MOVE(RcFuncOmsRequestNote);
   friend class RcFuncOmsRequest;
   enum class Stage {
      /// 等候 Client 送來查詢 Layout 的要求.
      WaitClient,
      /// Policy 還沒準備好, 等準備好之後送出 Layout: this->ApiSesCfg_->ApiSesCfgStr_;
      WaitPolicy,
      /// Policy 準備好, 但 Client 尚未送來查詢 Layout 的要求.
      PolicyReady,
      /// Layout: this->ApiSesCfg_->ApiSesCfgStr_ 已回覆給 client.
      /// 可以開始收下單要求了.
      LayoutAcked,
   };
   Stage Stage_{Stage::WaitClient};
   void SendLayout();

   // 有2個 threads 會用到此物件, 要如何安全的使用呢?
   // - Device thread.
   // - OmsCore thread.
   struct PolicyCfg : public OmsRequestPolicyCfg {
      fon9_NON_COPY_NON_MOVE(PolicyCfg);
      PolicyCfg() = default;
      fon9::io::DeviceSP   Device_;
   };
   using PolicyCfgSP = fon9::intrusive_ptr<PolicyCfg>;
   PolicyCfgSP Policy_{new PolicyCfg};
   static void SetPolicyReady(PolicyCfgSP polcfg);

public:
   const ApiSesCfgSP ApiSesCfg_;
   ApiSession&       ApiSession_;

   RcFuncOmsRequestNote(ApiSesCfgSP cfg, ApiSession& ses)
      : ApiSesCfg_{std::move(cfg)}
      , ApiSession_(ses) {
   }
   ~RcFuncOmsRequestNote();

   void OnRecvFunctionCall(ApiSession& ses, fon9::rc::RcFunctionParam& param) override;
};
fon9_WARN_POP;

} // namespaces
#endif//__f9omstw_OmsRcServerFunc_hpp__
