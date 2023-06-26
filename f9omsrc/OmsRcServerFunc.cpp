// \file f9omsrc/OmsRcServerFunc.cpp
// \author fonwinz@gmail.com
#include "f9omsrc/OmsRc.h"
#include "f9omsrc/OmsRcServerFunc.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "fon9/rc/RcFuncConnServer.hpp"
#include "fon9/io/Device.hpp"

namespace f9omstw {

OmsRcServerAgent::OmsRcServerAgent(ApiSesCfgSP cfg)
   : base{f9rc_FunctionCode_OmsApi}
   , ApiSesCfg_{std::move(cfg)} {
   const_cast<ApiSesCfg*>(this->ApiSesCfg_.get())->Subscribe();
}
OmsRcServerAgent::~OmsRcServerAgent() {
   const_cast<ApiSesCfg*>(this->ApiSesCfg_.get())->Unsubscribe();
}
OmsRcServerNoteSP OmsRcServerAgent::CreateOmsRcServerNote(ApiSession& ses) {
   return OmsRcServerNoteSP{new OmsRcServerNote(this->ApiSesCfg_, ses)};
}
void OmsRcServerAgent::OnSessionApReady(ApiSession& ses) {
   ses.ResetNote(this->FunctionCode_, this->CreateOmsRcServerNote(ses));
}
void OmsRcServerAgent::OnSessionLinkBroken(ApiSession& ses) {
   // 清除 note 之前, 要先確定 note 已無人參考.
   // 如果正在處理: 回報、訂閱... 要如何安全的刪除 note 呢?
   // => 由 Policy_ 接收回報、訂閱, 然後在必要時回到 Device thread 處理.
   if (auto* note = static_cast<OmsRcServerNote*>(ses.GetNote(f9rc_FunctionCode_OmsApi))) {
      if (note->Handler_) {
         note->Handler_->ClearResource();
         note->Handler_.reset();
      }
      this->ApiSesCfg_->CoreMgr_->TDayChangedEvent_.Unsubscribe(&note->SubrTDayChanged_);
   }
}
//--------------------------------------------------------------------------//
const unsigned kFcOverCountForceLogout = 10;

OmsRcServerNote::OmsRcServerNote(ApiSesCfgSP cfg, ApiSession& ses)
   : ApiSesCfg_{std::move(cfg)} {
   if (auto* authNote = ses.GetNote<fon9::rc::RcServerNote_SaslAuth>(f9rc_FunctionCode_SASL)) {
      const fon9::auth::AuthResult& authr = authNote->GetAuthResult();
      fon9::RevBufferList           rbuf{128};
      if (auto agUserRights = this->PolicyConfig_.LoadPoUserRights(authr)) {
         fon9::RevPrint(rbuf, *fon9_kCSTR_ROWSPL);
         agUserRights->MakeGridView(rbuf, this->PolicyConfig_.UserRights_);
         fon9::RevPrint(rbuf, fon9_kCSTR_LEAD_TABLE, agUserRights->Name_, *fon9_kCSTR_ROWSPL);
      }
      if (auto agIvList = this->PolicyConfig_.LoadPoIvList(authr)) {
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
   ses.Send(f9rc_FunctionCode_OmsApi, std::move(rbuf));
}
void OmsRcServerNote::StartPolicyRunner(ApiSession& ses, OmsCoreSP core) {
   if (this->Handler_)
      this->Handler_->ClearResource();
   this->Handler_.reset(new Handler(ses, this->PolicyConfig_, std::move(core), this->ApiSesCfg_));
   if (!OmsPoRequestHandler_Start(this->Handler_)) {
      // 目前沒有 CurrentCore, 仍要回覆 config, 只是 TDay 為 null, 然後等 TDayChanged 事件.
      this->SendConfig(ses);
   }
}
void OmsRcServerNote::OnRecvStartApi(ApiSession& ses) {
   if (this->SubrTDayChanged_ != nullptr) {
      ses.ForceLogout("Dup Start OmsApi.");
      return;
   }
   fon9::io::DeviceSP   pdev = ses.GetDevice();
   this->ApiSesCfg_->CoreMgr_->TDayChangedEvent_.Subscribe(
      &this->SubrTDayChanged_, [pdev](OmsCore& core) {
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
      static_cast<ApiSession*>(pdev->Session_.get())->Send(f9rc_FunctionCode_OmsApi, std::move(rbuf));
   });
   this->StartPolicyRunner(ses, this->ApiSesCfg_->CoreMgr_->CurrentCore());
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
      this->StartPolicyRunner(ses, std::move(core));
   }
}
bool OmsRcServerNote::CheckApiReady(ApiSession& ses) {
   if (fon9_UNLIKELY(this->Handler_.get() != nullptr))
      return true;
   ses.ForceLogout("OmsRcServer:Api not ready.");
   return false;
}
void OmsRcServerNote::OnRecvHelpOfferSt(ApiSession& ses, fon9::rc::RcFunctionParam& param) {
   (void)param;
   ses.ForceLogout("Server.OnRecvHelpOfferSt not supported.");
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
      if (this->CheckApiReady(ses)) {
         fon9::TimeStamp   tday;
         OmsRxSNO          from{};
         f9OmsRc_RptFilter filter{};
         fon9::BitvTo(param.RecvBuffer_, tday);
         fon9::BitvTo(param.RecvBuffer_, from);
         fon9::BitvTo(param.RecvBuffer_, filter);
         if (this->Handler_->Core_->TDay() == tday)
            this->Handler_->StartRecover(from, filter);
      }
      break;
   case f9OmsRc_OpKind_HelpOfferSt:
      this->OnRecvHelpOfferSt(ses, param);
      break;
   }
}
void OmsRcServerNote::OnRecvFunctionCall(ApiSession& ses, fon9::rc::RcFunctionParam& param) {
   unsigned reqTableId{UINT_MAX};
   fon9::BitvTo(param.RecvBuffer_, reqTableId);
   if (fon9_UNLIKELY(reqTableId == 0))
      return this->OnRecvOmsOp(ses, param);
   unsigned reqCount;
   bool     isNeedsGetTableId;
   if (fon9_LIKELY(reqTableId <= this->ApiSesCfg_->ApiReqCfgs_.size())) {
      reqCount = 1;
      isNeedsGetTableId = false;
   }
   else {
      if (reqTableId != UINT_MAX) {
         ses.ForceLogout("OmsRcServer:Unknown ReqTableId");
         return;
      }
      // 使用 f9OmsRc_SendRequestBatch() 批次下單:
      // fon9_BitvV_NumberNull(reqTableId:UINT_MAX), reqCount;
      // 然後接下來是: 下單要求(reqTableId + reqstr) * (reqCount筆);
      reqCount = 0;
      fon9::BitvTo(param.RecvBuffer_, reqCount);
      if (reqCount <= 0)
         return;
      isNeedsGetTableId = true;
   }
   if (!this->CheckApiReady(ses))
      return;
   for (; reqCount > 0; --reqCount) {
      if (isNeedsGetTableId)
         fon9::BitvTo(param.RecvBuffer_, reqTableId);
      const ApiReqCfg&  cfg = this->ApiSesCfg_->ApiReqCfgs_[reqTableId - 1];
      size_t            byteCount;
      if (!fon9::PopBitvByteArraySize(param.RecvBuffer_, byteCount))
         fon9::Raise<fon9::BitvNeedsMore>("OmsRcServer:request string needs more");
      if (byteCount <= 0)
         continue;
      OmsRequestRunner  runner;
      fon9::RevPrint(runner.ExLog_, '\n');
      char* const    pout = runner.ExLog_.AllocPrefix(byteCount) - byteCount;
      fon9::StrView  reqstr{pout, byteCount};
      runner.ExLog_.SetPrefixUsed(pout);
      param.RecvBuffer_.Read(pout, byteCount);

      auto pol = this->Handler_->RequestPolicy_;
      this->PolicyConfig_.UpdateIvDenys(pol.get());

      runner.Request_ = cfg.Factory_->MakeRequest(param.RecvTime_);
      if (fon9_LIKELY(runner.Request_)) {
         assert(dynamic_cast<OmsRequestTrade*>(runner.Request_.get()) != nullptr);
         static_cast<OmsRequestTrade*>(runner.Request_.get())->LgOut_ = this->PolicyConfig_.UserRights_.LgOut_;
      }
      else {
         // Rc client 端使用 TwsRpt 補登回報(補單).
         runner.Request_ = cfg.Factory_->MakeReportIn(f9fmkt_RxKind_Unknown, param.RecvTime_);
         if (!runner.Request_) {
            ses.ForceLogout("OmsRcServer:bad factory: " + cfg.Factory_->Name_);
            return;
         }
         // 回報(補單)權限需要自主檢查.
         if (!pol || !runner.CheckReportRights(*pol)) {
            ses.ForceLogout("OmsRcServer:deny input report.");
            return;
         }
         runner.Request_->SetReportNeedsLog();
         runner.Request_->SetForcePublish();
      }
      ApiReqFieldArg arg{*runner.Request_, ses};
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
      if (fon9_UNLIKELY(runner.Request_->IsReportIn())) // 回報補單不用考慮流量, 且沒有 SetPolicy().
         goto __RUN_REPORT;

      if (fon9_LIKELY(this->PolicyConfig_.FcReq_.Fetch().GetOrigValue() <= 0)) {
         static_cast<OmsRequestTrade*>(runner.Request_.get())->SetPolicy(std::move(pol));
      __RUN_REPORT:
         if (fon9_LIKELY(this->Handler_->Core_->MoveToCore(std::move(runner))))
            continue;
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
      if (this->ApiSesCfg_->MakeReportMessage(runner.ExLog_, *runner.Request_))
         ses.Send(f9rc_FunctionCode_OmsApi, std::move(runner.ExLog_));
      if (cstrForceLogout) {
         ses.ForceLogout(cstrForceLogout);
         break;
      }
   }
}
//--------------------------------------------------------------------------//
void OmsRcServerNote::Handler::ClearResource() {
   this->State_ = HandlerSt::Disposing;
   base::ClearResource();
   if (this->Core_)
      this->Core_->ReportSubject().Unsubscribe(&this->RptSubr_);
}
void OmsRcServerNote::Handler::OnRequestPolicyUpdated(OmsRequestPolicySP before) {
   // 只有第一次取得 RequestPolicy(before == nullptr), 才需要送出 SendConfig();
   if (before)
      return;
   HandlerSP handler{this};
   this->Device_->OpQueue_.AddTask([handler](fon9::io::Device& dev) {
      assert(dynamic_cast<ApiSession*>(dev.Session_.get()) != nullptr);
      auto& ses = *static_cast<ApiSession*>(dev.Session_.get());
      auto* note = static_cast<OmsRcServerNote*>(ses.GetNote(f9rc_FunctionCode_OmsApi));
      if (note && note->Handler_ == handler.get())
         note->SendConfig(ses);
   });
}
void OmsRcServerNote::Handler::StartRecover(OmsRxSNO from, f9OmsRc_RptFilter filter) {
   if (this->State_ > HandlerSt::Recovering) // 重覆訂閱/回補/解構中?
      return;
   this->RptFilter_ = filter;
   if (from == 0)
      this->SubscribeReport();
   else {
      this->State_ = HandlerSt::Recovering;
      this->WkoRecoverSNO_ = this->Core_->PublishedSNO();
      using namespace std::placeholders;
      this->Core_->ReportRecover(from, std::bind(&Handler::OnRecover, HandlerSP{this}, _1, _2));
   }
}
void OmsRcServerNote::Handler::SubscribeReport() {
   using namespace std::placeholders;
   this->State_ = HandlerSt::Reporting;
   this->Core_->ReportSubject().Subscribe(&this->RptSubr_, std::bind(&Handler::OnReport, HandlerSP{this}, _1, _2));
}
OmsRxSNO OmsRcServerNote::Handler::OnRecover(OmsCore& core, const OmsRxItem* item) {
   if (this->State_ != HandlerSt::Recovering)
      return 0;
   if (item) {
      // 如果在回補過程中 order 狀態改變, 要如何處理呢?
      // - 之前沒補到的.
      // - 或之後應該要補的(例:先前因 working order 回補了一些, 但後來 order 變成無剩餘量, 就被排除不補了!!).
      if (this->RptFilter_) {
         const OmsOrderRaw* ordraw = static_cast<const OmsOrderRaw*>(item->CastToOrder());
         if (this->RptFilter_ & f9OmsRc_RptFilter_RecoverLastSt) {
            if (ordraw && ordraw->Request().LastUpdated() != ordraw)
               return item->RxSNO() + 1;
         }
         if (this->RptFilter_ & f9OmsRc_RptFilter_RecoverWorkingOrder) {
            if (item->RxSNO() < this->WkoRecoverSNO_) {
               if (ordraw == nullptr) {
                  const OmsRequestBase* req = static_cast<const OmsRequestBase*>(item->CastToRequest());
                  if (req == nullptr)
                     return item->RxSNO() + 1;
                  ordraw = req->LastUpdated();
               }
               if (ordraw && !ordraw->Order().IsWorkingOrder())
                  return item->RxSNO() + 1;
            }
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
      f9rc_FunctionCode_OmsApi, std::move(rbuf));
   // 訂閱即時回報.
   this->SubscribeReport();
   return 0;
}
void OmsRcServerNote::Handler::OnReport(OmsCore&, const OmsRxItem& item) {
   if (this->State_ < HandlerSt::Disposing)
      this->SendReport(item);
}
bool OmsRcServerNote::Handler::SendReport(const OmsRxItem& item) {
   if (auto* ses = this->IsNeedReport(item)) {
      fon9::RevBufferList rbuf{128};
      if (this->ApiSesCfg_->MakeReportMessage(rbuf, item)) {
         ses->Send(f9rc_FunctionCode_OmsApi, std::move(rbuf));
         return true;
      }
   }
   return false;
}
// bool OmsRcServerNote::Handler::PreReport(const OmsRxItem& item) {
//    if (this->State_ >= HandlerSt::Disposing)
//       return false;
//    auto core = this->Core_;
//    if (!core)
//       return false;
//    if (item.RxSNO() <= core->PublishedSNO()) {
//       // 最終發行序號已超過 item, 雖不確定 item 是否有回報,
//       // 但 item [應該.視為] 已經送出.
//       return true;
//    }
//    // item 尚未透過正常流程回報.
//    if (item.CastToOrder())
//       return this->SendReport(item);
//    if (auto* req = static_cast<const OmsRequestBase*>(item.CastToRequest())) {
//       // req可能為子單, [母單]也需要回報.
//       const OmsOrderRaw* ordrawLast;
//       const OmsRxSNO     publishedSNO = core->PublishedSNO();
//       if (auto* parent = req->GetParentRequest()) {
//          if (publishedSNO < parent->RxSNO())
//             goto __BACKEND_FLUSH_AND_RETURN;
//          if ((ordrawLast = parent->LastUpdated()) != nullptr)
//             if (publishedSNO < ordrawLast->RxSNO())
//                goto __BACKEND_FLUSH_AND_RETURN;
//       }
//       // req可能為刪改查, [原始新單]也需要回報.
//       if ((ordrawLast = req->LastUpdated()) != nullptr) {
//          if (auto* ini = ordrawLast->Order().Initiator())
//             if (req != ini) {
//                if (publishedSNO < ini->RxSNO())
//                   goto __BACKEND_FLUSH_AND_RETURN;
//                if ((ordrawLast = ini->LastUpdated()) != nullptr)
//                   if (publishedSNO < ordrawLast->RxSNO())
//                      goto __BACKEND_FLUSH_AND_RETURN;
//             }
//       }
//       return this->SendReport(item);
//    }
//    // 必須把與 item 有關聯的 request 也送出, 所以必須使用 BackendFlush();
// __BACKEND_FLUSH_AND_RETURN:;
//    core->BackendFlush();
//    return true;
// }
//--------------------------------------------------------------------------//
/// sesUserId 與 reqUserId 的關係:
/// (1) 必須完全相同.
/// (2) reqUserId 附屬於 sesUserId: (sesSz < reqSz), 例:
///     sesUserId="K01"; reqUserId="K01.A1";
static inline bool CheckReqUserId(fon9::StrView sesUserId, fon9::StrView reqUserId) {
   const auto sesSz = sesUserId.size();
   const auto reqSz = reqUserId.size();
   if (sesSz > reqSz)
      return false;
   if (sesSz == reqSz || reqUserId.begin()[sesSz] == '.')
      return memcmp(sesUserId.begin(), reqUserId.begin(), sesSz) == 0;
   return false;
}
ApiSession* OmsRcServerNote::Handler::IsNeedReport(const OmsRxItem& item) {
   auto* ses = static_cast<fon9::rc::RcSession*>(this->Device_->Session_.get());
   // 檢查: UserId, 可用帳號, this->RptFilter_;
   fon9::StrView      sesUserId = ToStrView(ses->GetUserId());
   const OmsOrderRaw* ordraw = static_cast<const OmsOrderRaw*>(item.CastToOrder());
   if (ordraw) {
      fon9_WARN_DISABLE_SWITCH;
      switch (ordraw->RequestSt_) {
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
      if ((ordraw = reqb->LastUpdated()) == nullptr) { // abandon?
         if (this->RptFilter_ & (f9OmsRc_RptFilter_MatchOnly | f9OmsRc_RptFilter_RecoverWorkingOrder))
            return nullptr;
         const OmsRequestTrade* reqt = static_cast<const OmsRequestTrade*>(reqb);
         if (!reqt)
            return nullptr;
         if (CheckReqUserId(sesUserId, ToStrView(reqt->UserId_)))
            return ses;
         // abandon 不考慮可用帳號回報.
         return nullptr;
      }
   }
   auto& order = ordraw->Order();
   auto* ini = order.Initiator();
   if (fon9_UNLIKELY(this->RptFilter_ & f9OmsRc_RptFilter_NoExternal)) {
      auto& req = ordraw->Request();
      // 底下使用 while() 是為了裡面能直接 break; 不會有迴圈行為.
      while (req.IsReportIn() && !ordraw->IsForceInternal()) {
         // 回報程序收到的(TwsRpt, TwfRpt, TwsFil, TwfFil...), 才需要判斷是否為 External.
         const OmsRequestTrade* reqTrade;
         // 這裡如果 ordraw 是 internal, 但 req 不是, 即使將 ordraw 送給了 Client(OmsRcSyn),
         // Client 收到 ordraw 後, 也找不到對應的 req, 所以即使送了也毫無意義。
         // => 若對方有此需求, 則應設定 f9OmsRc_RptFilter_IncludeRcSynNew 旗標.
         // => 但是無法處理改單要求對應的回報.
         //    => 這點不會造成問題, 因為改單回報如果沒有對應的 RequestChg,
         //       則會使用 Rpt(TwsRpt,TwfRpt) 處理.
         if (req.RxKind() == f9fmkt_RxKind_Filled) {
            assert(dynamic_cast<const OmsReportFilled*>(&req) != nullptr);
            if (static_cast<const OmsReportFilled*>(&req)->IsForceInternalRpt())
               break;
            if ((reqTrade = ini) == nullptr)
               return nullptr;
            if (!reqTrade->IsReportIn())
               break;
         }
         else {
            if ((this->RptFilter_ & f9OmsRc_RptFilter_IncludeRcSynNew)
                && item.RxKind() == f9fmkt_RxKind_RequestNew
                && static_cast<const OmsRequestBase*>(&item)->IsSynReport())
               break;
            assert(dynamic_cast<const OmsRequestTrade*>(&req) != nullptr);
            reqTrade = static_cast<const OmsRequestTrade*>(&req);
         }
         if (reqTrade->IsForceInternalRpt())
            break;
         return nullptr;
      }
   }
   if (this->RptFilter_ & f9OmsRc_RptFilter_MatchOnly) {
      if (ordraw->RequestSt_ != f9fmkt_TradingRequestSt_Filled)
         return nullptr;
   }
   // -----
   if (ini && CheckReqUserId(sesUserId, ToStrView(ini->UserId_)))
      return ses;
   if (!(this->RptFilter_ & f9OmsRc_RptFilter_UncheckIvList)) {
      // 如果有重啟過, Ivr_ 會在 Backend 載入時建立, 所以這裡可以放心使用 order.ScResource().Ivr_;
      auto rights = this->RequestPolicy_->GetIvRights(order.ScResource().Ivr_.get(), nullptr);
      if (IsEnumContains(rights, OmsIvRight::AllowSubscribeReport)
          || (rights & OmsIvRight::DenyTradingAll) != OmsIvRight::DenyTradingAll)
         return ses;
   }
   return nullptr;
}
//--------------------------------------------------------------------------//
void FnPutApiField_ApiSesName(const ApiReqFieldCfg& cfg, const ApiReqFieldArg& arg) {
   fon9::rc::RcFunctionNote* note = arg.ApiSession_.GetNote(f9rc_FunctionCode_OmsApi);
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
   //"OmsCore=CoreMgr|Cfg=forms/ApiAll.cfg|AddTo=FpSession/OmsRcSvr"
   f9omstw::ApiSesCfgSP  sesCfg;
   f9omstw::OmsCoreMgrSP coreMgr;
   fon9::StrView         tag, value;
   while (fon9::SbrFetchTagValue(args, tag, value)) {
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
      else if (tag == "AddTo") {
         if (!sesCfg)
            holder.SetPluginsSt(fon9::LogLevel::Error, "Config not setted.");
         else if (fon9::rc::RcFunctionMgr* rcFuncMgr = fon9::rc::FindRcFunctionMgr(holder, value)) {
            rcFuncMgr->Add(fon9::rc::RcFunctionAgentSP{new f9omstw::OmsRcServerAgent{sesCfg}});
            continue;
         }
         return false;
      }
      else {
         holder.SetPluginsSt(fon9::LogLevel::Error, "Unknown tag=", tag);
         return false;
      }
   }
   return true;
}

extern "C" fon9::seed::PluginsDesc f9p_OmsRcServerAgent;
static fon9::seed::PluginsPark f9pAutoPluginsReg{"OmsRcServerAgent", &f9p_OmsRcServerAgent};
fon9::seed::PluginsDesc f9p_OmsRcServerAgent{"", &OmsRcServerAgent_Start, nullptr, nullptr,};
