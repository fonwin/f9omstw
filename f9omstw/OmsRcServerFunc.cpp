// \file f9omstw/OmsRcServerFunc.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRcServerFunc.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsPoIvListAgent.hpp"
#include "f9omstw/OmsPoUserRightsAgent.hpp"
#include "fon9/rc/RcFuncConn.hpp"
#include "fon9/io/Device.hpp"

namespace f9omstw {

OmsRcServerAgent::~OmsRcServerAgent() {
}
void OmsRcServerAgent::OnSessionApReady(ApiSession& ses) {
   ses.ResetNote(this->FunctionCode_,
                 fon9::rc::RcFunctionNoteSP{new OmsRcServerNote(this->ApiSesCfg_, ses)});
}
void OmsRcServerAgent::OnSessionLinkBroken(ApiSession& ses) {
   // 清除 note 之前, 要先確定 note 已無人參考.
   // 如果正在處理: 回報、訂閱... 要如何安全的刪除 note 呢?
   // => 由 Policy_ 接收回報、訂閱, 然後在必要時回到 Device thread 處理.
   if (auto* note = static_cast<OmsRcServerNote*>(ses.GetNote(fon9::rc::RcFunctionCode::OmsApi))) {
      if (note->Handler_) {
         note->Handler_->ClearResource();
         note->Handler_.reset();
      }
      this->ApiSesCfg_->CoreMgr_->TDayChangedEvent_.Unsubscribe(&note->SubrTDayChanged_);
   }
}
//--------------------------------------------------------------------------//
OmsRcServerNote::OmsRcServerNote(ApiSesCfgSP cfg, ApiSession& ses)
   : ApiSesCfg_{std::move(cfg)} {
   if (auto* authNote = ses.GetNote<fon9::rc::RcServerNote_SaslAuth>(fon9::rc::RcFunctionCode::SASL)) {
      const fon9::auth::AuthResult& authr = authNote->GetAuthResult();
      fon9::RevBufferList  rbuf{128};
      if (auto agUserRights = authr.AuthMgr_->Agents_->Get<OmsPoUserRightsAgent>(fon9_kCSTR_OmsPoUserRightsAgent_Name)) {
         agUserRights->GetPolicy(authr, this->PolicyConfig_.UserRights_, &this->PolicyConfig_.TeamGroupName_);
         fon9::RevPrint(rbuf, *fon9_kCSTR_ROWSPL);
         agUserRights->MakeGridView(rbuf, this->PolicyConfig_.UserRights_);
         fon9::RevPrint(rbuf, fon9_kCSTR_LEAD_TABLE, agUserRights->Name_, *fon9_kCSTR_ROWSPL);
      }
      if (auto agIvList = authr.AuthMgr_->Agents_->Get<OmsPoIvListAgent>(fon9_kCSTR_OmsPoIvListAgent_Name)) {
         agIvList->GetPolicy(authr, this->PolicyConfig_.IvList_);
         fon9::RevPrint(rbuf, *fon9_kCSTR_ROWSPL);
         agIvList->MakeGridView(rbuf, this->PolicyConfig_.IvList_);
         fon9::RevPrint(rbuf, fon9_kCSTR_LEAD_TABLE, agIvList->Name_, *fon9_kCSTR_ROWSPL);
      }
      this->PolicyConfig_.TablesGridView_ = fon9::BufferTo<std::string>(rbuf.MoveOut());
   }
}
OmsRcServerNote::~OmsRcServerNote() {
}
void OmsRcServerNote::SendConfig(ApiSession& ses) {
   fon9::RevBufferList rbuf{256};
   // TODO: 其他資料: 重複登入時間及來源、上次連線時間及來源...
   fon9::ToBitv(rbuf, this->PolicyConfig_.TablesGridView_);
   fon9::ToBitv(rbuf, this->ApiSesCfg_->ApiSesCfgStr_);
   fon9::IntToBitv(rbuf, fon9::LocalHostId_);
   OmsCore* core = this->Handler_->Core_.get();
   fon9::ToBitv(rbuf, core ? core->TDay() : fon9::TimeStamp::Null());
   fon9::ToBitv(rbuf, core ? core->SeedPath_ : std::string{});
   ses.Send(fon9::rc::RcFunctionCode::OmsApi, std::move(rbuf));
}
void OmsRcServerNote::StartPolicyRunner(ApiSession& ses, OmsCore* core) {
   if (this->Handler_)
      this->Handler_->ClearResource();
   PolicyRunner   runner{new Handler(ses.GetDevice(), this->PolicyConfig_, core)};
   this->Handler_ = runner;
   if (core)
      core->RunCoreTask(runner);
   else // 目前沒有 CurrentCore, 仍要回覆 config, 只是 TDay 為 null, 然後等 TDayChanged 事件.
      this->SendConfig(ses);
}
void OmsRcServerNote::OnRecvStartApi(ApiSession& ses) {
   if (this->SubrTDayChanged_ != nullptr) {
      ses.ForceLogout("Dup Start OmsApi.");
      return;
   }
   fon9::io::DeviceSP   pdev = ses.GetDevice();
   this->SubrTDayChanged_ = this->ApiSesCfg_->CoreMgr_->TDayChangedEvent_.Subscribe([pdev](OmsCore& core) {
      // 通知 TDayChanged, 等候 client 回覆 TDayConfirm, 然後才正式切換.
      // 不能在此貿然更換 CurrentCore, 因為可能還有在路上針對 CurrentCore 的操作.
      // - TDay changed 事件: 通知 client, 但先不要切換 CurrentCore;
      // - client 也許出現訊息視窗告知, 等候使用者確定.
      // - 等 client confirm TDay changed 之後:
      //   重新透過新的 OmsCore 取得 Policy, 然後再告知 client 成功切換 CurrentCore.
      fon9::RevBufferList rbuf{128};
      fon9::ToBitv(rbuf, core.TDay());
      fon9::ToBitv(rbuf, OmsRcOpKind::TDayChanged);
      static_cast<ApiSession*>(pdev->Session_.get())->Send(fon9::rc::RcFunctionCode::OmsApi, std::move(rbuf));
   });
   this->StartPolicyRunner(ses, this->ApiSesCfg_->CoreMgr_->CurrentCore().get());
}
void OmsRcServerNote::OnRecvTDayConfirm(ApiSession& ses, fon9::rc::RcFunctionParam& param) {
   fon9::TimeStamp tday;
   fon9::BitvTo(param.RecvBuffer_, tday);
   auto core = this->ApiSesCfg_->CoreMgr_->CurrentCore();
   if (core && core->TDay() == tday) {
      // 有可能在 SendConfig() 之前, CurrentCore 又改變了.
      // => 可能性不高, 所以不用考慮效率問題.
      // => 如果真的發生了, 必定還會再 TDayConfirm 一次.
      // => 所以最終 client 還是會得到正確的 config;
      this->StartPolicyRunner(ses, core.get());
   }
}
void OmsRcServerNote::OnRecvOmsOp(ApiSession& ses, fon9::rc::RcFunctionParam& param) {
   OmsRcOpKind opkind{};
   fon9::BitvTo(param.RecvBuffer_, opkind);
   switch (opkind) {
   case OmsRcOpKind::Config:
      this->OnRecvStartApi(ses);
      break;
   case OmsRcOpKind::TDayConfirm:
      this->OnRecvTDayConfirm(ses, param);
      break;
   case OmsRcOpKind::ReportRecover:
   case OmsRcOpKind::ReportSubscribe:
      if (0);// ReportRecover/ReportSubscribe;
      return;
   default:
      if (0);// Unknown qkind: throw exception?
      return;
   }
}
void OmsRcServerNote::OnRecvFunctionCall(ApiSession& ses, fon9::rc::RcFunctionParam& param) {
   uint32_t reqTableId{};
   fon9::BitvTo(param.RecvBuffer_, reqTableId);
   if (fon9_UNLIKELY(reqTableId == 0))
      return this->OnRecvOmsOp(ses, param);
   if (fon9_UNLIKELY(reqTableId > this->ApiSesCfg_->ApiReqCfgs_.size())) {
      if (0);// Unknown reqTableId: throw exception?
   }
   const ApiReqCfg&  cfg = this->ApiSesCfg_->ApiReqCfgs_[reqTableId - 1];
   // TODO: Request: OmsRequestTrade.
}
//--------------------------------------------------------------------------//
void OmsRcServerNote::Handler::ClearResource() {
   if (0);// (1) 取消回報訂閱.
   if (this->Device_->OpImpl_GetState() == fon9::io::State::LinkReady) {
      // (2) 移除 fon9::rc SeedTree 裡面 OmsCore/* 的權限.
   }
}

void OmsRcServerNote::PolicyRunner::operator()(OmsResource& res) {
   this->get()->MakePolicy(res, this->get());
   this->get()->Device_->OpQueue_.AddTask(PolicyRunner{*this});
}
void OmsRcServerNote::PolicyRunner::operator()(fon9::io::Device& dev) {
   auto& ses = *static_cast<ApiSession*>(dev.Session_.get());
   auto* note = static_cast<OmsRcServerNote*>(ses.GetNote(fon9::rc::RcFunctionCode::OmsApi));
   if (note && note->Handler_ == this->get())
      note->SendConfig(ses);
   if (auto* seedNote = ses.GetNote(fon9::rc::RcFunctionCode::SeedTree)) {
      if (0);// TODO: 設定 fon9::rc SeedTree 裡面 OmsCore/Brks/BrkId/IvacNo/SubacNo 的權限.
   }
}
//--------------------------------------------------------------------------//
void FnPutApiField_ApiSesName(const ApiReqFieldCfg& cfg, const ApiReqFieldArg& arg) {
   fon9::rc::RcFunctionNote* note = arg.ApiSession_.GetNote(fon9::rc::RcFunctionCode::OmsApi);
   assert(dynamic_cast<OmsRcServerNote*>(note) != nullptr);
   cfg.Field_->StrToCell(arg, &static_cast<OmsRcServerNote*>(note)->ApiSesCfg_->SessionName_);
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
