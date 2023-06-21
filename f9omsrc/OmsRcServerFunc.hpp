// \file f9omsrc/OmsRcServerFunc.hpp
// \author fonwinz@gmail.com
#ifndef __f9omsrc_OmsRcServerFunc_hpp__
#define __f9omsrc_OmsRcServerFunc_hpp__
#include "f9omsrc/OmsRcServer.hpp"
#include "f9omsrc/OmsRc.h"
#include "f9omstw/OmsPoRequest.hpp"

namespace f9omstw {

class OmsRcServerNote;
using OmsRcServerNoteSP = std::unique_ptr<OmsRcServerNote>;

class OmsRcServerAgent : public fon9::rc::RcFunctionAgent {
   fon9_NON_COPY_NON_MOVE(OmsRcServerAgent);
   using base = fon9::rc::RcFunctionAgent;

   virtual OmsRcServerNoteSP CreateOmsRcServerNote(ApiSession& ses);

public:
   const ApiSesCfgSP ApiSesCfg_;

   OmsRcServerAgent(ApiSesCfgSP cfg);
   ~OmsRcServerAgent();

   void OnSessionApReady(ApiSession& ses) override;
   void OnSessionLinkBroken(ApiSession& ses) override;
   // void OnRecvFunctionCall(ApiSession& ses, fon9::rc::RcFunctionParam& param) override;
};

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
      Handler(ApiSession& ses, const OmsRequestPolicyCfg& cfg, OmsCoreSP core, ApiSesCfgSP apicfg)
         : base(cfg, std::move(core), ToStrView(ses.GetUserId()), ses.GetUserIdForAuthz())
         , Device_{std::move(ses.GetDevice())}
         , ApiSesCfg_{std::move(apicfg)} {
      }
      void ClearResource() override;
      void StartRecover(OmsRxSNO from, f9OmsRc_RptFilter filter);
      // bool PreReport(const OmsRxItem& item);
   private:
      enum class HandlerSt {
         Preparing,
         Recovering,
         Reporting,
         Disposing,
      };
      HandlerSt         State_{};
      f9OmsRc_RptFilter RptFilter_;
      char              Padding____[3];
      fon9::SubConn     RptSubr_{};
      OmsRxSNO          WkoRecoverSNO_{};
      void SubscribeReport();
      OmsRxSNO OnRecover(OmsCore&, const OmsRxItem* item);
      void OnReport(OmsCore&, const OmsRxItem& item);
      bool SendReport(const OmsRxItem& item);
      ApiSession* IsNeedReport(const OmsRxItem& item);
   };
   using HandlerSP = fon9::intrusive_ptr<Handler>;
   HandlerSP   Handler_;

   struct PolicyConfig : OmsPoRequestConfig {
      fon9_NON_COPY_NON_MOVE(PolicyConfig);
      PolicyConfig() = default;
      std::string TablesGridView_;
   };
   PolicyConfig   PolicyConfig_;

   fon9::SubConn  SubrTDayChanged_{};
   unsigned       ConfigSentCount_{};
   char           Padding____[4];

   bool CheckApiReady(ApiSession& ses);
   void SendConfig(ApiSession& ses);
   void OnRecvOmsOp(ApiSession& ses, fon9::rc::RcFunctionParam& param);
   void OnRecvStartApi(ApiSession& ses);
   void OnRecvTDayConfirm(ApiSession& ses, fon9::rc::RcFunctionParam& param);
   void StartPolicyRunner(ApiSession& ses, OmsCoreSP core);

protected:
   virtual void OnRecvHelpOfferSt(ApiSession& ses, fon9::rc::RcFunctionParam& param);

public:
   const ApiSesCfgSP    ApiSesCfg_;

   OmsRcServerNote(ApiSesCfgSP cfg, ApiSession& ses);
   ~OmsRcServerNote();

   void OnRecvFunctionCall(ApiSession& ses, fon9::rc::RcFunctionParam& param) override;

   const PolicyConfig& GetPolicyConfig() const {
      return this->PolicyConfig_;
   }

   // 應使用 Backend::Flush() 強制回報, 因為 PreReport() 裡面的 SendReport() 會呼叫 this->ApiSesCfg_->MakeReportMessage();
   // 而 this->ApiSesCfg_->MakeReportMessage() 只能在 Backend thread 裡面呼叫, 不是 thread safe;
   // - 在 Backend 尚未回報前, 提前回報, 當 Backend 正常流程到達時, 可能會有重複.
   // - 若 Backend 的正常流程可能已回報(判斷 PublishedSNO),
   //   但無法確認是否有需要送給Client, 則返回 true;
   // - 若 item 不需要回報, 則返回 false;
   // bool PreReport(const OmsRxItem& item) {
   //    if (auto hdr = this->Handler_)
   //       return hdr->PreReport(item);
   //    return false;
   // }

   OmsCoreSP GetOmsCore() const {
      if (auto hdr = this->Handler_)
         return hdr->Core_;
      return nullptr;
   }
   bool HasAdminRight() const {
      if (auto hdr = this->Handler_) {
         if (auto pol = hdr->RequestPolicy_)
            return pol->HasAdminRight();
      }
      return false;
   }
};

} // namespaces
#endif//__f9omsrc_OmsRcServerFunc_hpp__
