// \file f9omsrc/OmsRcClientApi.cpp
// \author fonwinz@gmail.com
#include "f9omsrc/OmsRcClient.hpp"
#include "fon9/rc/RcFuncConn.hpp"

extern "C" {

f9OmsRc_API_FN(int)
f9OmsRc_Initialize(const char* logFileFmt) {
   return f9OmsRc_Initialize2(logFileFmt, NULL);
}
f9OmsRc_API_FN(int)
f9OmsRc_Initialize2(const char* logFileFmt, const char* iosvCfg) {
   int retval = f9rc_Initialize(logFileFmt, iosvCfg);
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

static f9omstw::OmsRcClientNote* GetSesNoteLog(fon9::rc::RcClientSession* ses) {
   auto* note = static_cast<f9omstw::OmsRcClientNote*>(ses->GetNote(f9rc_FunctionCode_OmsApi));
   if (fon9_LIKELY(note))
      return note;
   fon9_LOG_ERROR("OmsRcClient.SendReq"
                  "|ses=", fon9::ToPtr(static_cast<f9rc_ClientSession*>(ses)),
                  "|err=Must call f9OmsRc_Initialize() first, "
                  "or must apply f9OmsRc_ClientSessionParams when f9rc_CreateClientSession();");
   return nullptr;
}
static void WaitFlowCtrl(fon9::StrView funcName, fon9::rc::RcClientSession* ses, fon9::TimeInterval fcWait, f9omstw::OmsRcClientNote* note) {
   fon9_LOG_INFO(funcName,
                 "|ses=", fon9::ToPtr(static_cast<f9rc_ClientSession*>(ses)),
                 "|wait=", fcWait);
   if (note->Params_.FnOnFlowControl_) // 流量管制.事件通知.
      note->Params_.FnOnFlowControl_(ses, static_cast<unsigned>(fcWait.ShiftUnit<6>()));
   else
      std::this_thread::sleep_for(fcWait.ToDuration());
}

static bool SendRequest(fon9::rc::RcClientSession* ses, const f9OmsRc_Layout* reqLayout,
                        fon9::RevBufferList&& rbuf, fon9::StrView logReqStr) {
   auto* note = GetSesNoteLog(ses);
   if (!note)
      return false;
   fon9::ToBitv(rbuf, reqLayout->LayoutId_);
   for (;;) {
      fon9::TimeInterval fcWait = note->FcReq_.Fetch();
      if (fon9_LIKELY(fcWait.GetOrigValue() <= 0)) {
         ses->Send(f9rc_FunctionCode_OmsApi, std::move(rbuf));
         if (fon9_UNLIKELY(ses->LogFlags_ & f9oms_ClientLogFlag_Request))
            fon9_LOG_INFO("OmsRcClient.SendReq"
                          "|ses=", fon9::ToPtr(static_cast<f9rc_ClientSession*>(ses)),
                          "|tab=", reqLayout->LayoutId_, ':', reqLayout->LayoutName_,
                          "|req=", logReqStr);
         return true;
      }
      WaitFlowCtrl("OmsRcClient.SendReq.FlowControl", ses, fcWait, note);
   }
}

static inline void RevPutReqStr(fon9::RevBuffer& pkbuf, const fon9_CStrView reqStr) {
   const size_t reqsz = static_cast<size_t>(reqStr.End_ - reqStr.Begin_);
   fon9::RevPutMem(pkbuf, reqStr.Begin_, reqsz);
   fon9::ByteArraySizeToBitvT(pkbuf, reqsz);
}

f9OmsRc_API_FN(int)
f9OmsRc_SendRequestString(f9rc_ClientSession*   ses,
                          const f9OmsRc_Layout* reqLayout,
                          const fon9_CStrView   reqStr) {
   fon9::RevBufferList pkbuf{128};
   RevPutReqStr(pkbuf, reqStr);
   return SendRequest(static_cast<fon9::rc::RcClientSession*>(ses), reqLayout, std::move(pkbuf), reqStr);
}

static void SendRevPackBatchImpl(f9rc_ClientSession*         ses,
                                 const f9OmsRc_RequestBatch* reqBatch,
                                 unsigned                    reqCount) {
   fon9::RevBufferList pkbuf{128};
   fon9::RevBufferList logbuf{fon9::kLogBlockNodeSize};
   const bool isNeedsLog = ((ses->LogFlags_ & f9oms_ClientLogFlag_Request)
                            && (fon9::LogLevel::Info >= fon9::LogLevel_));
   unsigned pkCount = 0;
   for (pkCount = 0; pkCount < reqCount;) {
      --reqBatch;
      ++pkCount;
      if (fon9_UNLIKELY(isNeedsLog)) {
         fon9::RevPrint(logbuf,
                        "|tab=", reqBatch->Layout_->LayoutId_, ':', reqBatch->Layout_->LayoutName_,
                        "|req=", reqBatch->ReqStr_,
                        '\n');
      }
      RevPutReqStr(pkbuf, reqBatch->ReqStr_);
      fon9::ToBitv(pkbuf, reqBatch->Layout_->LayoutId_);
   }
   if (fon9_UNLIKELY(isNeedsLog)) {
      fon9::RevPrint(logbuf, "OmsRcClient.SendBatch|ses=", fon9::ToPtr(static_cast<f9rc_ClientSession*>(ses)),
                     "|pkSize=", fon9::CalcDataSize(pkbuf.cfront()),
                     "|pkCount=", pkCount, '\n');
      fon9::LogWrite(fon9::LogLevel::Info, std::move(logbuf));
   }
   fon9::ToBitv(pkbuf, pkCount);
   fon9::RevPutBitv(pkbuf, fon9_BitvV_NumberNull);
   static_cast<fon9::rc::RcClientSession*>(ses)->Send(f9rc_FunctionCode_OmsApi, std::move(pkbuf));
}

// gMaxPkSizeMSS 盡量接近 TCP MSS(ethernet=1460) 但不要超過, 還要扣除 rc protocol 的 header.
static unsigned gMaxPkSizeMSS = 1300;

static void SendRevPackBatch(f9rc_ClientSession*         ses,
                             const f9OmsRc_RequestBatch* reqBatch,
                             unsigned                    reqCount) {
   if (fon9_UNLIKELY(reqCount <= 0))
      return;
   reqBatch -= reqCount;
   size_t   pksz = 0;
   unsigned pkCount = 0;
   while (++pkCount < reqCount) {
      pksz += (reqBatch->ReqStr_.End_ - reqBatch->ReqStr_.Begin_) + 5;
      ++reqBatch;
      if (fon9_UNLIKELY(pksz >= gMaxPkSizeMSS)) {
         SendRevPackBatchImpl(ses, reqBatch, pkCount);
         reqCount -= pkCount;
         pksz = 0;
         pkCount = 0;
      }
   }
   SendRevPackBatchImpl(ses, reqBatch + 1, pkCount);
}

f9OmsRc_API_FN(int)
f9OmsRc_SendRequestBatch(f9rc_ClientSession*         ses,
                         const f9OmsRc_RequestBatch* reqBatch,
                         unsigned                    reqCount) {
   auto* note = GetSesNoteLog(static_cast<fon9::rc::RcClientSession*>(ses));
   if (!note)
      return false;
   unsigned packCount;
   { // fc lock.
      auto  fcReq = note->FcReq_.Lock();
      if (fcReq->TimeUnit().IsNullOrZero()) // 無流量管制.
         packCount = reqCount;
      else {
         packCount = 0;
         while (reqCount > 0) {
            fon9::TimeInterval fcWait = fcReq->Fetch();
            if (fon9_LIKELY(fcWait.GetOrigValue() <= 0)) {
               --reqCount;
               ++packCount;
               continue;
            }
            fcReq.unlock();
            if (packCount > 0) {
               SendRevPackBatch(ses, reqBatch += packCount, packCount);
               packCount = 0;
            }
            fcReq.lock();
            WaitFlowCtrl("OmsRcClient.SendBatch.FlowControl", static_cast<fon9::rc::RcClientSession*>(ses), fcWait, note);
         }
      }
   } // fc unlock.
   SendRevPackBatch(ses, reqBatch + packCount, packCount);
   return true;
}
f9OmsRc_API_FN(unsigned) f9OmsRc_GetRequestBatchMSS() {
   return gMaxPkSizeMSS;
}
f9OmsRc_API_FN(unsigned) f9OmsRc_SetRequestBatchMSS(unsigned value) {
   unsigned old = gMaxPkSizeMSS;
   gMaxPkSizeMSS = value;
   return old;
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
