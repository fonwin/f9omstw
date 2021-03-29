// \file f9omsrc/OmsRcClientApi.cpp
// \author fonwinz@gmail.com
#include "f9omsrc/OmsRcClient.hpp"
#include "fon9/rc/RcFuncConn.hpp"

extern "C" {

f9OmsRc_API_FN(int)
f9OmsRc_Initialize(const char* logFileFmt) {
   int retval = fon9_Initialize(logFileFmt);
   if (retval == 0 && fon9::rc::RcClientMgr_->Get(f9rc_FunctionCode_OmsApi) == nullptr)
      fon9::rc::RcClientMgr_->Add(fon9::rc::RcFunctionAgentSP{new f9omstw::OmsRcClientAgent});
   return retval;
}
   
f9OmsRc_API_FN(void)
f9OmsRc_ResizeFcRequest(f9rc_ClientSession* ses, unsigned count, unsigned ms) {
   if (auto* note = static_cast<fon9::rc::RcClientSession*>(ses)->GetNote(f9rc_FunctionCode_OmsApi)) {
      assert(dynamic_cast<f9omstw::OmsRcClientNote*>(note) != nullptr);
      static_cast<f9omstw::OmsRcClientNote*>(note)->FcReq_.Resize(count, fon9::TimeInterval_Millisecond(ms));
   }
}

f9OmsRc_API_FN(void)
f9OmsRc_SubscribeReport(f9rc_ClientSession* ses,
                        const f9OmsRc_ClientConfig* cfg,
                        f9OmsRc_SNO from,
                        f9OmsRc_RptFilter filter) {
   fon9::rc::RcClientSession* clises = static_cast<fon9::rc::RcClientSession*>(ses);
   fon9::RevBufferList rbuf{64};
   fon9::ToBitv(rbuf, filter);
   fon9::ToBitv(rbuf, from);
   fon9::ToBitv(rbuf, fon9::unsigned_cast(fon9::YYYYMMDDHHMMSS_ToTimeStamp(cfg->CoreTDay_.YYYYMMDD_, 0).GetIntPart()
                                          + cfg->CoreTDay_.UpdatedCount_));
   fon9::ToBitv(rbuf, f9OmsRc_OpKind_ReportSubscribe);
   fon9::RevPutBitv(rbuf, fon9_BitvV_Number0);
   clises->Send(f9rc_FunctionCode_OmsApi, std::move(rbuf));
}

f9OmsRc_API_FN(f9OmsRc_Layout*)
f9OmsRc_GetRequestLayout(f9rc_ClientSession* ses,
                         const f9OmsRc_ClientConfig* cfg,
                         const char* reqName) {
   if (ses == nullptr || cfg == nullptr || reqName == nullptr)
      return nullptr;
   const f9omstw::OmsRcClientNote& note = f9omstw::OmsRcClientNote::CastFrom(*cfg);
   if (static_cast<fon9::rc::RcClientSession*>(ses)->GetNote(f9rc_FunctionCode_OmsApi) != &note)
      return nullptr;
   // note.ReqLayouts().Get() 不是 thread safe, 所以如果正在處理 Config, 則這裡不安全!
   // 如何確保 layout 持續有效呢? 只要沒斷線, 即使 TDayChanged, 應該不會變動 layout config.
   return note.ReqLayouts().Get(fon9::StrView_cstr(reqName));
}

f9OmsRc_API_FN(f9OmsRc_Layout*)
f9OmsRc_GetReportLayout(f9rc_ClientSession* ses,
                        const f9OmsRc_ClientConfig* cfg,
                        unsigned rptTableId) {
   if (ses == nullptr || cfg == nullptr || rptTableId == 0)
      return nullptr;
   const f9omstw::OmsRcClientNote& note = f9omstw::OmsRcClientNote::CastFrom(*cfg);
   if (static_cast<fon9::rc::RcClientSession*>(ses)->GetNote(f9rc_FunctionCode_OmsApi) != &note)
      return nullptr;
   const auto& rptLayouts = note.RptLayouts();
   return rptTableId > rptLayouts.size() ? nullptr : rptLayouts[rptTableId - 1].get();
}

static bool SendRequest(fon9::rc::RcClientSession* ses, const f9OmsRc_Layout* reqLayout,
                        fon9::RevBufferList&& rbuf, fon9::StrView logReqStr) {
   auto* note = static_cast<f9omstw::OmsRcClientNote*>(ses->GetNote(f9rc_FunctionCode_OmsApi));
   if(fon9_UNLIKELY(note == nullptr)) {
      fon9_LOG_ERROR("OmsRcClient.SendReq"
                     "|ses=", fon9::ToPtr(static_cast<f9rc_ClientSession*>(ses)),
                     "|err=Must call f9OmsRc_Initialize() first, "
                     "or must apply f9OmsRc_ClientSessionParams when f9rc_CreateClientSession();");
      return false;
   }
   fon9::ToBitv(rbuf, reqLayout->LayoutId_);
   for (;;) {
      fon9::TimeInterval fcWait = note->FcReq_.Fetch();
      if (fon9_LIKELY(fcWait.GetOrigValue() <= 0)) {
         ses->Send(f9rc_FunctionCode_OmsApi, std::move(rbuf));
         if (ses->LogFlags_ & f9oms_ClientLogFlag_Request)
            fon9_LOG_INFO("OmsRcClient.SendReq"
                          "|ses=", fon9::ToPtr(static_cast<f9rc_ClientSession*>(ses)),
                          "|tab=", reqLayout->LayoutId_, ':', reqLayout->LayoutName_,
                          "|req=", logReqStr);
         return true;
      }
      fon9_LOG_INFO("OmsRcClient.SendReq.FlowControl"
                    "|ses=", fon9::ToPtr(static_cast<f9rc_ClientSession*>(ses)),
                    "|wait=", fcWait);
      if (note->Params_.FnOnFlowControl_) // 流量管制.事件通知.
         note->Params_.FnOnFlowControl_(ses, static_cast<unsigned>(fcWait.ShiftUnit<6>()));
      else
         std::this_thread::sleep_for(fcWait.ToDuration());
   }
}

f9OmsRc_API_FN(int)
f9OmsRc_SendRequestString(f9rc_ClientSession*   ses,
                          const f9OmsRc_Layout* reqLayout,
                          const fon9_CStrView   reqStr) {
   fon9::RevBufferList rbuf{128};
   const size_t reqsz = static_cast<size_t>(reqStr.End_ - reqStr.Begin_);
   fon9::RevPutMem(rbuf, reqStr.Begin_, reqsz);
   fon9::ByteArraySizeToBitvT(rbuf, reqsz);
   return SendRequest(static_cast<fon9::rc::RcClientSession*>(ses), reqLayout, std::move(rbuf), reqStr);
}

f9OmsRc_API_FN(int)
f9OmsRc_SendRequestFields(f9rc_ClientSession*   ses,
                          const f9OmsRc_Layout* reqLayout,
                          const fon9_CStrView*  reqFieldArray) {
   fon9::RevBufferList  rbuf{128};
   unsigned             idx = reqLayout->FieldCount_;
   const fon9_CStrView* pReqStr = reqFieldArray + idx;
   if (fon9_LIKELY(idx > 0)) {
      for (;;) {
         --pReqStr;
         fon9::RevPutMem(rbuf, pReqStr->Begin_, pReqStr->End_);
         if (--idx == 0)
            break;
         fon9::RevPutChar(rbuf, *fon9_kCSTR_CELLSPL);
      }
   }
   std::string logstr;
   if (ses->LogFlags_ & f9oms_ClientLogFlag_Request)
      logstr = fon9::BufferTo<std::string>(rbuf.cfront());
   fon9::ByteArraySizeToBitvT(rbuf, fon9::CalcDataSize(rbuf.cfront()));
   return SendRequest(static_cast<fon9::rc::RcClientSession*>(ses), reqLayout, std::move(rbuf), &logstr);
}

f9OmsRc_API_FN(unsigned)
f9OmsRc_CheckFcRequest(f9rc_ClientSession* ses) {
   if (auto* note = static_cast<fon9::rc::RcClientSession*>(ses)->GetNote(f9rc_FunctionCode_OmsApi)) {
      assert(dynamic_cast<f9omstw::OmsRcClientNote*>(note) != nullptr);
      return static_cast<unsigned>(static_cast<f9omstw::OmsRcClientNote*>(note)->FcReq_.Check().ShiftUnit<6>());
   }
   return 0;
}

} // extern "C"
