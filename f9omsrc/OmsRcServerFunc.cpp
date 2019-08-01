// \file f9omsrc/OmsRcServerFunc.cpp
// \author fonwinz@gmail.com
#include "f9omsrc/OmsRc.h"
#include "f9omsrc/OmsRcServerFunc.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsPoIvListAgent.hpp"
#include "f9omstw/OmsPoUserRightsAgent.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "fon9/rc/RcFuncConn.hpp"
#include "fon9/io/Device.hpp"

namespace f9omstw {

OmsRcServerAgent::OmsRcServerAgent(ApiSesCfgSP cfg)
   : base{fon9::rc::RcFunctionCode::OmsApi}
   , ApiSesCfg_{std::move(cfg)} {
   const_cast<ApiSesCfg*>(this->ApiSesCfg_.get())->Subscribe();
}
OmsRcServerAgent::~OmsRcServerAgent() {
   const_cast<ApiSesCfg*>(this->ApiSesCfg_.get())->Unsubscribe();
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
const unsigned kFcOverCountForceLogout = 10;
static void ResetFlowCounter(fon9::FlowCounter& fc, FlowControlArgs fcArgs) {
   unsigned count = fcArgs.Count_;
   auto     ti = fon9::TimeInterval_Millisecond(fcArgs.IntervalMS_);
   if (count && ti.GetOrigValue() > 0) {
      // 增加一點緩衝, 避免網路延遲造成退單爭議.
      unsigned cbuf = count / 10;
      count += (cbuf <= 0) ? 1 : cbuf;
      ti -= ti / 10;
   }
   fc.Resize(count, ti);
}

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
         ResetFlowCounter(this->PolicyConfig_.FcReq_, this->PolicyConfig_.UserRights_.FcRequest_);
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
   this->PolicyConfig_.FcReqOverCount_ = 0;
   fon9::RevBufferList rbuf{256};
   if (++this->ConfigSentCount_ == 1) {
      fon9::ToBitv(rbuf, this->PolicyConfig_.TablesGridView_);
      fon9::ToBitv(rbuf, this->ApiSesCfg_->ApiSesCfgStr_);
   }
   else {
      fon9::RevPutBitv(rbuf, fon9_BitvV_ByteArrayEmpty);
      fon9::RevPutBitv(rbuf, fon9_BitvV_ByteArrayEmpty);
   }
   fon9::IntToBitv(rbuf, fon9::LocalHostId_);
   OmsCore* core = this->Handler_->Core_.get();
   fon9::ToBitv(rbuf, core ? core->SeedPath_ : std::string{});
   fon9::ToBitv(rbuf, core ? core->TDay() : fon9::TimeStamp::Null());
   fon9::ToBitv(rbuf, f9OmsRc_OpKind_Config);
   fon9::RevPutBitv(rbuf, fon9_BitvV_Number0); // ReqTableId=0
   ses.Send(fon9::rc::RcFunctionCode::OmsApi, std::move(rbuf));
}
void OmsRcServerNote::StartPolicyRunner(ApiSession& ses, OmsCore* core) {
   if (this->Handler_)
      this->Handler_->ClearResource();
   PolicyRunner   runner{new Handler(ses.GetDevice(), this->PolicyConfig_, core, this->ApiSesCfg_)};
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
      fon9::ToBitv(rbuf, f9OmsRc_OpKind_TDayChanged);
      fon9::RevPutBitv(rbuf, fon9_BitvV_Number0); // ReqTableId=0
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
bool OmsRcServerNote::CheckApiReady(ApiSession& ses) {
   if (fon9_UNLIKELY(this->Handler_.get() != nullptr))
      return true;
   ses.ForceLogout("OmsRcServer:Api not ready.");
   return false;
}
void OmsRcServerNote::OnRecvOmsOp(ApiSession& ses, fon9::rc::RcFunctionParam& param) {
   f9OmsRc_OpKind opkind{};
   fon9::BitvTo(param.RecvBuffer_, opkind);
   switch (opkind) {
   default:
      ses.ForceLogout("OmsRcServer:Unknown f9OmsRc_OpKind");
      return;
   case f9OmsRc_OpKind_Config:
      this->OnRecvStartApi(ses);
      break;
   case f9OmsRc_OpKind_TDayConfirm:
      this->OnRecvTDayConfirm(ses, param);
      break;
   case f9OmsRc_OpKind_ReportSubscribe:
      if (!this->CheckApiReady(ses))
         return;
      fon9::TimeStamp   tday;
      OmsRxSNO          from{};
      f9OmsRc_RptFilter filter{};
      fon9::BitvTo(param.RecvBuffer_, tday);
      fon9::BitvTo(param.RecvBuffer_, from);
      fon9::BitvTo(param.RecvBuffer_, filter);
      if (this->Handler_->Core_->TDay() == tday)
         this->Handler_->StartRecover(from, filter);
      return;
   }
}
void OmsRcServerNote::OnRecvFunctionCall(ApiSession& ses, fon9::rc::RcFunctionParam& param) {
   unsigned reqTableId{};
   fon9::BitvTo(param.RecvBuffer_, reqTableId);
   if (fon9_UNLIKELY(reqTableId == 0))
      return this->OnRecvOmsOp(ses, param);
   if (fon9_UNLIKELY(reqTableId > this->ApiSesCfg_->ApiReqCfgs_.size())) {
      ses.ForceLogout("OmsRcServer:Unknown ReqTableId");
      return;
   }
   if (!this->CheckApiReady(ses))
      return;
   const ApiReqCfg&  cfg = this->ApiSesCfg_->ApiReqCfgs_[reqTableId - 1];
   size_t            byteCount;
   if (!fon9::PopBitvByteArraySize(param.RecvBuffer_, byteCount))
      fon9::Raise<fon9::BitvNeedsMore>("OmsRcServer:request string needs more");
   if (byteCount <= 0)
      return;
   OmsRequestRunner  runner;
   fon9::RevPrint(runner.ExLog_, '\n');
   char* const    pout = runner.ExLog_.AllocPrefix(byteCount) - byteCount;
   fon9::StrView  reqstr{pout, byteCount};
   runner.ExLog_.SetPrefixUsed(pout);
   param.RecvBuffer_.Read(pout, byteCount);

   runner.Request_ = static_cast<OmsRequestTrade*>(cfg.Factory_->MakeRequest(param.RecvTime_).get());
   ApiReqFieldArg arg{*runner.Request_.get(), ses};
   for (const auto& fldcfg : cfg.ApiFields_) {
      arg.ClientFieldValue_ = fon9::StrFetchNoTrim(reqstr, *fon9_kCSTR_CELLSPL);
      if (fldcfg.Field_)
         fldcfg.Field_->StrToCell(arg, arg.ClientFieldValue_);
      if (fldcfg.FnPut_)
         (*fldcfg.FnPut_)(fldcfg, arg);
   }
   arg.ClientFieldValue_.Reset(nullptr);
   for (const auto& fldcfg : cfg.SysFields_) {
      if (fldcfg.FnPut_)
         (*fldcfg.FnPut_)(fldcfg, arg);
   }
   const char* cstrForceLogout;
   if (fon9_LIKELY(this->PolicyConfig_.FcReq_.Fetch().GetOrigValue() <= 0)) {
      runner.Request_->SetPolicy(this->Handler_->RequestPolicy_);
      if (fon9_LIKELY(this->Handler_->Core_->MoveToCore(std::move(runner))))
         return;
      cstrForceLogout = nullptr;
   }
   else {
      if (++this->PolicyConfig_.FcReqOverCount_ > kFcOverCountForceLogout) {
         // 超過流量, 若超過次數超過 n 則強制斷線.
         // 避免有惡意連線, 大量下單壓垮系統.
         runner.RequestAbandon(nullptr, OmsErrCode_OverFlowControl,
                               cstrForceLogout = "FlowControl: ForceLogout.");
      }
      else {
         runner.RequestAbandon(nullptr, OmsErrCode_OverFlowControl);
         cstrForceLogout = nullptr;
      }
   }
   // request abandon, 立即回報失敗.
   assert(runner.Request_->IsAbandoned());
   runner.ExLog_.MoveOut();
   if (this->ApiSesCfg_->MakeReport(runner.ExLog_, *runner.Request_))
      ses.Send(fon9::rc::RcFunctionCode::OmsApi, std::move(runner.ExLog_));
   if (cstrForceLogout)
      ses.ForceLogout(cstrForceLogout);
}
//--------------------------------------------------------------------------//
void OmsRcServerNote::Handler::ClearResource() {
   this->State_ = HandlerSt::Disposing;
   this->Core_->ReportSubject().Unsubscribe(&this->RptSubr_);
   if (this->Device_->OpImpl_GetState() == fon9::io::State::LinkReady) {
      if (0);// TODO: 移除 fon9::rc SeedVisitor 裡面 OmsCore/* 的權限.
   }
}
void OmsRcServerNote::Handler::StartRecover(OmsRxSNO from, f9OmsRc_RptFilter filter) {
   if (this->State_ > HandlerSt::Recovering) // 重覆訂閱/回補/解構中?
      return;
   this->State_ = HandlerSt::Recovering;
   this->RptFilter_ = filter;
   using namespace std::placeholders;
   this->Core_->ReportRecover(from, std::bind(&Handler::OnRecover, HandlerSP{this}, _1, _2));
}
OmsRxSNO OmsRcServerNote::Handler::OnRecover(OmsCore& core, const OmsRxItem* item) {
   if (this->State_ != HandlerSt::Recovering)
      return 0;
   if (item) {
      if (this->RptFilter_ & f9OmsRc_RptFilter_RecoverLastSt) {
         if (const OmsOrderRaw* ord = static_cast<const OmsOrderRaw*>(item->CastToOrder())) {
            if (ord->Request_->LastUpdated() != ord)
               return item->RxSNO() + 1;
         }
      }
      this->SendReport(*item);
      return item->RxSNO() + 1;
   }
   // 通知: 回補結束.
   fon9::RevBufferList rbuf{128};
   fon9::ToBitv(rbuf, core.PublishedSNO());
   fon9::ToBitv(rbuf, core.TDay());
   fon9::ToBitv(rbuf, f9OmsRc_OpKind_ReportSubscribe);
   fon9::RevPutBitv(rbuf, fon9_BitvV_Number0);
   static_cast<fon9::rc::RcSession*>(this->Device_->Session_.get())->Send(
      fon9::rc::RcFunctionCode::OmsApi, std::move(rbuf));
   // 訂閱即時回報.
   using namespace std::placeholders;
   core.ReportSubject().Subscribe(&this->RptSubr_, std::bind(&Handler::OnReport, HandlerSP{this}, _1, _2));
   return 0;
}
void OmsRcServerNote::Handler::OnReport(OmsCore&, const OmsRxItem& item) {
   if (this->State_ < HandlerSt::Disposing)
      this->SendReport(item);
}
void OmsRcServerNote::Handler::SendReport(const OmsRxItem& item) {
   if (auto* ses = this->IsNeedReport(item)) {
      fon9::RevBufferList rbuf{128};
      if (this->ApiSesCfg_->MakeReport(rbuf, item))
         ses->Send(fon9::rc::RcFunctionCode::OmsApi, std::move(rbuf));
   }
}
ApiSession* OmsRcServerNote::Handler::IsNeedReport(const OmsRxItem& item) {
   auto* ses = static_cast<fon9::rc::RcSession*>(this->Device_->Session_.get());
   // 檢查 UserId, 可用帳號.
   fon9::StrView      sesUserId = ToStrView(ses->GetUserId());
   const OmsOrderRaw* ord = static_cast<const OmsOrderRaw*>(item.CastToOrder());
   if (ord) {
      fon9_WARN_DISABLE_SWITCH;
      switch (ord->RequestSt_) {
      case f9fmkt_TradingRequestSt_Queuing:
         if (this->RptFilter_ & f9OmsRc_RptFilter_NoQueuing)
            return nullptr;
         break;
      case f9fmkt_TradingRequestSt_Sending:
         if (this->RptFilter_ & f9OmsRc_RptFilter_NoSending)
            return nullptr;
         break;
      }
      fon9_WARN_POP;
   }
   else {
      const OmsRequestBase* reqb = static_cast<const OmsRequestBase*>(item.CastToRequest());
      if (!reqb) {
         if (item.CastToEvent() == nullptr)
            return nullptr;
         // event report: 直接回報, 不檢查 UserId、可用帳號.
         return ses;
      }
      if ((ord = reqb->LastUpdated()) == nullptr) { // abandon?
         const OmsRequestTrade* reqt = static_cast<const OmsRequestTrade*>(reqb);
         if (!reqt)
            return nullptr;
         if (ToStrView(reqt->UserId_) == sesUserId)
            return ses;
         // abandon 不考慮可用帳號回報.
         return nullptr;
      }
   }
   if (ToStrView(ord->Order_->Initiator()->UserId_) == sesUserId)
      return ses;
   // 如果有重啟過, Ivr_ 會在 Backend 載入時建立.
   auto rights = this->RequestPolicy_->GetIvRights(ord->Order_->ScResource().Ivr_.get());
   if (IsEnumContains(rights, OmsIvRight::AllowSubscribeReport)
       || (rights & OmsIvRight::DenyTradingAll) != OmsIvRight::DenyTradingAll)
      return ses;
   return nullptr;
}
//--------------------------------------------------------------------------//
void OmsRcServerNote::PolicyRunner::operator()(OmsResource& res) {
   Handler* handler = this->get();
   handler->RequestPolicy_ = handler->MakePolicy(res);
   handler->Device_->OpQueue_.AddTask(PolicyRunner{*this});
}
void OmsRcServerNote::PolicyRunner::operator()(fon9::io::Device& dev) {
   auto& ses = *static_cast<ApiSession*>(dev.Session_.get());
   auto* note = static_cast<OmsRcServerNote*>(ses.GetNote(fon9::rc::RcFunctionCode::OmsApi));
   if (note && note->Handler_ == this->get())
      note->SendConfig(ses);
   if (auto* seedNote = ses.GetNote(fon9::rc::RcFunctionCode::SeedVisitor)) {
      if (0);// TODO: 設定 fon9::rc SeedVisitor 裡面 OmsCore/Brks/BrkId/IvacNo/SubacNo 的權限.
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

//--------------------------------------------------------------------------//

#include "fon9/seed/Plugins.hpp"
#include "fon9/ConfigLoader.hpp"
#include "fon9/framework/IoManager.hpp"

static bool OmsRcServerAgent_Start(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
   //"OmsCore=CoreMgr|Cfg=forms/ApiAll.cfg|AddTo=FpSession.RcSv"
   f9omstw::ApiSesCfgSP  sesCfg;
   f9omstw::OmsCoreMgrSP coreMgr;
   while (!args.empty()) {
      fon9::StrView value = SbrFetchNoTrim(args, '|');
      fon9::StrView tag = StrFetchTrim(value, '=');
      StrTrim(&value);
      if (tag == "OmsCore") {
         coreMgr = holder.Root_->GetSapling<f9omstw::OmsCoreMgr>(value);
         if (!coreMgr) {
            holder.SetPluginsSt(fon9::LogLevel::Error, "Not found OmsCore=", value);
            return false;
         }
      }
      else if (tag == "Cfg") {
         if (!coreMgr) {
            holder.SetPluginsSt(fon9::LogLevel::Error, "OmsCore not setted.");
            return false;
         }
         try {
            sesCfg.reset();
            sesCfg = f9omstw::MakeApiSesCfg(coreMgr, value);
         }
         catch (fon9::ConfigLoader::Err& err) {
            holder.SetPluginsSt(fon9::LogLevel::Error, "Cfg load err=", err);
            return false;
         }
      }
      else if (tag == "AddTo") { // IoMgr or SessionFactoryPark.
         if (!sesCfg) {
            holder.SetPluginsSt(fon9::LogLevel::Error, "Config not setted.");
            return false;
         }
         const char* pAddTo = value.begin();
         tag = fon9::StrFetchTrim(value, '/');
         auto sesFacPark = holder.Root_->Get<fon9::SessionFactoryPark>(tag);
         if (!sesFacPark) {
            if (auto iomgr = holder.Root_->GetSapling<fon9::IoManager>(tag))
               sesFacPark = iomgr->SessionFactoryPark_;
            if (!sesFacPark) {
               holder.SetPluginsSt(fon9::LogLevel::Error, "Not found SessionFactoryPark=",
                                   fon9::StrView{pAddTo, value.end()});
               return false;
            }
         }
         // 參考 RcFuncConnServer.cpp 裡面的 struct RcSessionServer_Factory;
         fon9::rc::RcFunctionMgr* rcFuncMgr = dynamic_cast<fon9::rc::RcFunctionMgr*>(sesFacPark->Get(value).get());
         if (!rcFuncMgr) {
            holder.SetPluginsSt(fon9::LogLevel::Error, "Not found RcFunctionMgr=",
                                fon9::StrView{pAddTo, value.end()});
            return false;
         }
         rcFuncMgr->Add(fon9::rc::RcFunctionAgentSP{new f9omstw::OmsRcServerAgent{sesCfg}});
      }
      else {
         holder.SetPluginsSt(fon9::LogLevel::Error, "Unknown tag=", tag);
         return false;
      }
   }
   return true;
}

extern "C" fon9::seed::PluginsDesc f9p_OmsRcServerAgent;
static fon9::seed::PluginsPark f9p_NamedIoManager_reg{"OmsRcServerAgent", &f9p_OmsRcServerAgent};
fon9::seed::PluginsDesc f9p_OmsRcServerAgent{"", &OmsRcServerAgent_Start, nullptr, nullptr,};
