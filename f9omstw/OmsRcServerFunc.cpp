// \file f9omstw/OmsRcServerFunc.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRcServerFunc.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsPoIvListAgent.hpp"
#include "f9omstw/OmsPoUserRightsAgent.hpp"
#include "fon9/rc/RcFuncConn.hpp"
#include "fon9/io/Device.hpp"

namespace f9omstw {

RcFuncOmsRequest::~RcFuncOmsRequest() {
}
void RcFuncOmsRequest::OnSessionApReady(ApiSession& ses) {
   RcFuncOmsRequestNote* note;
   ses.ResetNote(this->FunctionCode_, fon9::rc::RcFunctionNoteSP{note = new RcFuncOmsRequestNote(this->ApiSesCfg_, ses)});

   auto  polcfg = note->Policy_;
   polcfg->Device_.reset(ses.GetDevice());
   if (auto* authNote = ses.GetNote<fon9::rc::RcServerNote_SaslAuth>(fon9::rc::RcFunctionCode::SASL)) {
      const fon9::auth::AuthResult& authr = authNote->GetAuthResult();
      if (auto agIvList = authr.AuthMgr_->Agents_->Get<OmsPoIvListAgent>(fon9_kCSTR_OmsPoIvListAgent_Name))
         agIvList->GetPolicy(authr, polcfg->IvList_);
      if (auto agUserRights = authr.AuthMgr_->Agents_->Get<OmsPoUserRightsAgent>(fon9_kCSTR_OmsPoUserRightsAgent_Name))
         agUserRights->GetPolicy(authr, polcfg->UserRights_, &polcfg->TeamGroupName_);

      if (0);// 應該在 Client 查詢 config(TDay、layouts、可用櫃號、可用帳號、流量參數...) 時:
      // - 建立 Policy: 透過 OmsCore 取得: 可用帳號、櫃號群組... 取得完畢後就可直接回覆.
      // - this->ApiSesCfg_->CoreMgr_->Reg Core Changed(TDay changed) event;
      //   - TDay changed 事件: 通知 client, 但先不要切換 CurrentCore;
      //   - client 也許出現訊息視窗告知, 等候使用者確定.
      //   - 等 client confirm TDay changed 之後:
      //     - 重新透過新的 OmsCore 取得 Policy, 然後再告知 client 成功切換 CurrentCore.

      if (OmsCoreSP core = this->ApiSesCfg_->CoreMgr_->GetCurrentCore()) {
         core->EmplaceMessage([polcfg](OmsResource& res) {
            // 這裡是在 OmsCore thread, 操作 ApiSession 的 Note 是不安全的.
            // 所以先取得 policy, 然後再到 device thread 通知 PolicyReady.
            polcfg->FetchPolicy(res);
            RcFuncOmsRequestNote::SetPolicyReady(polcfg);
         });
         return;
      }
   }
   note->Stage_ = RcFuncOmsRequestNote::Stage::PolicyReady;
}
void RcFuncOmsRequest::OnSessionLinkBroken(ApiSession& ses) {
   // TODO: 清除 note 之前, 要先確定 note 已無人參考.
   // 如果正在處理: 回報、訂閱、OmsRequestPolicy... 要如何安全的刪除 note 呢?
   if (auto* curNote = static_cast<ApiSession*>(&ses)->GetNote(fon9::rc::RcFunctionCode::OmsRequest)) {
      static_cast<RcFuncOmsRequestNote*>(curNote)->Policy_.reset();
   }
}
//--------------------------------------------------------------------------//
RcFuncOmsRequestNote::~RcFuncOmsRequestNote() {
}
void RcFuncOmsRequestNote::SetPolicyReady(PolicyCfgSP polcfg) {
   polcfg->Device_->OpQueue_.AddTask(fon9::io::DeviceAsyncOp{[polcfg](fon9::io::Device& dev) {
      auto* curNote = static_cast<ApiSession*>(dev.Session_.get())->GetNote(fon9::rc::RcFunctionCode::OmsRequest);
      if (curNote == nullptr || static_cast<RcFuncOmsRequestNote*>(curNote)->Policy_ != polcfg)
         return;
      assert(static_cast<RcFuncOmsRequestNote*>(curNote)->Stage_ <= Stage::WaitPolicy);
      if (static_cast<RcFuncOmsRequestNote*>(curNote)->Stage_ == Stage::WaitClient)
         static_cast<RcFuncOmsRequestNote*>(curNote)->Stage_ = Stage::PolicyReady;
      else {
         assert(static_cast<RcFuncOmsRequestNote*>(curNote)->Stage_ == Stage::WaitPolicy);
         static_cast<RcFuncOmsRequestNote*>(curNote)->SendLayout();
      }
   }});
}
void RcFuncOmsRequestNote::SendLayout() {
   assert(this->Stage_ < Stage::LayoutAcked);
}
void RcFuncOmsRequestNote::OnRecvFunctionCall(ApiSession& ses, fon9::rc::RcFunctionParam& param) {
   // query config.
   // TDay changed confirm.
   // Request: OmsRequestTrade.
   // Query:   seed/tree.
}
//--------------------------------------------------------------------------//
void FnPutApiField_ApiSesName(const ApiReqFieldCfg& cfg, const ApiReqFieldArg& arg) {
   fon9::rc::RcFunctionNote* note = arg.ApiSession_.GetNote(fon9::rc::RcFunctionCode::OmsRequest);
   assert(dynamic_cast<RcFuncOmsRequestNote*>(note) != nullptr);
   cfg.Field_->StrToCell(arg, &static_cast<RcFuncOmsRequestNote*>(note)->ApiSesCfg_->SessionName_);
}
void FnPutApiField_ApiSesUserId(const ApiReqFieldCfg& cfg, const ApiReqFieldArg& arg) {
   cfg.Field_->StrToCell(arg, ToStrView(arg.ApiSession_.GetUserId()));
}
void FnPutApiField_ApiSesFromIp(const ApiReqFieldCfg& cfg, const ApiReqFieldArg& arg) {
   cfg.Field_->StrToCell(arg, ToStrView(arg.ApiSession_.GetRemoteIp()));
}
static FnPutApiField_Register regFnPutApiField_ApiSes{
   "ApiSes.Name",   &FnPutApiField_ApiSesName,
   "ApiSes.UserId", &FnPutApiField_ApiSesUserId,
   "ApiSes.FromIp", &FnPutApiField_ApiSesFromIp,
};

} // namespaces
