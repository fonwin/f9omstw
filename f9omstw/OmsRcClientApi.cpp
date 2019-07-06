// \file f9omstw/OmsRcClientApi.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRcClient.hpp"
#include "fon9/rc/RcFuncConn.hpp"
#include "fon9/framework/IoManager.hpp"
#include "fon9/DefaultThreadPool.hpp"
#include "fon9/LogFile.hpp"

namespace f9omstw {

enum f9OmsRc_St {
   f9OmsRc_St_Pending,
   f9OmsRc_St_Initializing,
   f9OmsRc_St_Initialized,
   f9OmsRc_St_Finalizing,
   f9OmsRc_St_Finalized,
};
static volatile f9OmsRc_St    f9OmsRc_St_{f9OmsRc_St_Pending};
static std::atomic<uint32_t>  f9OmsRc_InitCount_{0};

fon9_WARN_DISABLE_PADDING;
class f9OmsRcCliMgr : public fon9::intrusive_ref_counter<f9OmsRcCliMgr>,
                      public fon9::IoManager,
                      public fon9::rc::RcFunctionMgr {
   fon9_NON_COPY_NON_MOVE(f9OmsRcCliMgr);
   using base = fon9::IoManager;

   struct f9OmsRcSessionFactory : public fon9::SessionFactory {
      fon9_NON_COPY_NON_MOVE(f9OmsRcSessionFactory);
      f9OmsRcSessionFactory() : fon9::SessionFactory{"f9OmsRcClientSession"} {
      }
      fon9::io::SessionSP CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) override {
         (void)errReason;
         const IoConfigItem& iocfg = *static_cast<const IoConfigItem*>(&cfg);
         return new OmsRcClientSession(static_cast<f9OmsRcCliMgr*>(&mgr), iocfg.Params_);
      }
      fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager&, const fon9::IoConfigItem&, std::string& errReason) override {
         errReason = "Not support.";
         return nullptr;
      }
   };
   f9OmsRcSessionFactory   f9OmsRcSessionFactory_;

   friend unsigned int intrusive_ptr_add_ref(const f9OmsRcCliMgr* p) BOOST_NOEXCEPT {
      return intrusive_ptr_add_ref(static_cast<const fon9::intrusive_ref_counter<f9OmsRcCliMgr>*>(p));
   }
   friend unsigned int intrusive_ptr_release(const f9OmsRcCliMgr* p) BOOST_NOEXCEPT {
      return intrusive_ptr_release(static_cast<const fon9::intrusive_ref_counter<f9OmsRcCliMgr>*>(p));
   }
   unsigned AddRef() override { return intrusive_ptr_add_ref(this); }
   unsigned Release() override { return intrusive_ptr_release(this); }
   unsigned IoManagerAddRef() override { return intrusive_ptr_add_ref(this); }
   unsigned IoManagerRelease() override { return intrusive_ptr_release(this); }
   void NotifyChanged(fon9::IoManager::DeviceRun&) override {}
   void NotifyChanged(fon9::IoManager::DeviceItem&) override {}
public:
   struct IoConfigItem : public fon9::IoConfigItem {
      const f9OmsRc_ClientSessionParams*  Params_;
   };

   f9OmsRcCliMgr(const fon9::IoManagerArgs& ioMgrArgs) : base{ioMgrArgs} {
      this->DeviceFactoryPark_->Add(fon9::MakeIoFactoryTcpClient("TcpClient"));
      this->Add(fon9::rc::RcFunctionAgentSP{new fon9::rc::RcFuncConnClient("f9omsApi.0", "f9oms RcClient")});
      this->Add(fon9::rc::RcFunctionAgentSP{new fon9::rc::RcFuncSaslClient{}});
      this->Add(fon9::rc::RcFunctionAgentSP{new OmsRcClientAgent{}});
   }

   int CreateOmsRcClientSession(f9OmsRc_ClientSession** result, const IoConfigItem& iocfg) {
      auto fac = this->DeviceFactoryPark_->Get(fon9::StrView_cstr(iocfg.Params_->DevName_));
      if (!fac)
         return false;
      std::string errReason;
      auto dev = fac->CreateDevice(this, this->f9OmsRcSessionFactory_, iocfg, errReason);
      if (result)
         *result = static_cast<OmsRcClientSession*>(dev->Session_.get());
      dev->Initialize();
      dev->AsyncOpen(iocfg.Params_->DevParams_);
      dev.detach(); // 配合在 DestroyOmsRcClientSession() 建立 dev 時使用 add_ref = false;
      return true;
   }
   void DestroyOmsRcClientSession(OmsRcClientSession* ses, int isWait) {
      // add_ref = false: 配合在 CreateOmsRcClientSession(); 裡面有呼叫 dev.detach();
      fon9::io::DeviceSP dev(ses->GetDevice(), false);
      dev->AsyncDispose("f9OmsRc_DestroySession");
      dev->WaitGetDeviceId();
      if(isWait) {
         while (dev->use_count() > 1)
            std::this_thread::yield();
      }
   }

   void OnDevice_Destructing(fon9::io::Device&) override {
   }
   void OnDevice_Accepted(fon9::io::DeviceServer&, fon9::io::DeviceAcceptedClient&) override {
   }
   void OnDevice_Initialized(fon9::io::Device&) override {
   }
   void OnDevice_StateChanged(fon9::io::Device&, const fon9::io::StateChangedArgs&) override {
   }
   void OnDevice_StateUpdated(fon9::io::Device&, const fon9::io::StateUpdatedArgs&) override {
   }
   void OnSession_StateUpdated(fon9::io::Device&, fon9::StrView stmsg, fon9::LogLevel) override {
      (void)stmsg;
   }
};
fon9_WARN_POP;
static fon9::intrusive_ptr<f9OmsRcCliMgr> f9OmsRcCliMgr_;

} // namespaces
using namespace f9omstw;

//--------------------------------------------------------------------------//

extern "C" {

f9OmsRc_API_FN(int)
f9OmsRc_Initialize(const char* logFileFmt) {
   if (f9OmsRc_St_ >= f9OmsRc_St_Finalizing)
      return ENOSYS;
   ++f9OmsRc_InitCount_;
   if (logFileFmt) {
      if(*logFileFmt == '\0')
         logFileFmt = "./logs/{0:f+'L'}/f9OmsRc.log";
      auto res = fon9::InitLogWriteToFile(logFileFmt, fon9::FileRotate::TimeScale::Day, 0, 0);
      if (res.IsError())
         return res.GetError().value();
   }
   // 不考慮 multi thread 同時呼叫 f9OmsRc_Initialize()/f9OmsRc_Finalize() 的情況.
   if (f9OmsRc_St_ < f9OmsRc_St_Initializing) {
      f9OmsRc_St_ = f9OmsRc_St_Initializing;
      fon9::IoManagerArgs ioMgrArgs;
      ioMgrArgs.Name_ = "f9OmsRcCliMgr";
      ioMgrArgs.IoServiceCfgstr_ = "ThreadCount=1";// "|Wait=Policy|Cpus=List|Capacity=0";
      f9OmsRcCliMgr_.reset(new f9OmsRcCliMgr{ioMgrArgs});

      fon9::GetDefaultThreadPool();
      fon9::GetDefaultTimerThread();
      f9OmsRc_St_ = f9OmsRc_St_Initialized;
   }
   else while (f9OmsRc_St_ == f9OmsRc_St_Initializing) {
      std::this_thread::yield();
   }
   return 0;
}
   
f9OmsRc_API_FN(void)
f9OmsRc_Finalize() {
   assert(f9OmsRc_InitCount_ > 0);
   if (--f9OmsRc_InitCount_ > 0)
      return;
   f9OmsRc_St_ = f9OmsRc_St_Finalizing;
   f9OmsRcCliMgr_.reset();
   fon9::WaitDefaultThreadPoolQuit(fon9::GetDefaultThreadPool());
   // TODO: fon9::GetDefaultTimerThread(); 如何結束?
   f9OmsRc_St_ = f9OmsRc_St_Finalized;
}

f9OmsRc_API_FN(void)
f9OmsRc_FcReqResize(f9OmsRc_ClientSession* ses, unsigned count, unsigned ms) {
   static_cast<OmsRcClientSession*>(ses)->FcReq_.Resize(count, fon9::TimeInterval_Millisecond(ms));
}

f9OmsRc_API_FN(int)
f9OmsRc_CreateSession(f9OmsRc_ClientSession** result,
                      const f9OmsRc_ClientSessionParams* params) {
   if (f9OmsRcCliMgr_.get() == nullptr)
      return false;
   f9OmsRcCliMgr::IoConfigItem iocfg;
   iocfg.Params_ = params;
   return f9OmsRcCliMgr_->CreateOmsRcClientSession(result, iocfg);
}

f9OmsRc_API_FN(void)
f9OmsRc_DestroySession(f9OmsRc_ClientSession* ses, int isWait) {
   if (ses && f9OmsRcCliMgr_)
      f9OmsRcCliMgr_->DestroyOmsRcClientSession(static_cast<OmsRcClientSession*>(ses), isWait);
}

f9OmsRc_API_FN(void)
f9OmsRc_SubscribeReport(f9OmsRc_ClientSession* ses,
                        const f9OmsRc_ClientConfig* cfg,
                        f9OmsRc_SNO from,
                        f9OmsRc_RptFilter filter) {
   OmsRcClientSession* cli = static_cast<OmsRcClientSession*>(ses);
   fon9::RevBufferList rbuf{64};
   fon9::ToBitv(rbuf, filter);
   fon9::ToBitv(rbuf, from);
   fon9::ToBitv(rbuf, fon9::unsigned_cast(fon9::YYYYMMDDHHMMSS_ToTimeStamp(cfg->CoreTDay_.YYYYMMDD_, 0).GetIntPart()
                                          + cfg->CoreTDay_.UpdatedCount_));
   fon9::ToBitv(rbuf, f9OmsRc_OpKind_ReportSubscribe);
   fon9::RevPutBitv(rbuf, fon9_BitvV_Number0);
   cli->Send(fon9::rc::RcFunctionCode::OmsApi, std::move(rbuf));
}

f9OmsRc_API_FN(f9OmsRc_Layout*)
f9OmsRc_GetRequestLayout(f9OmsRc_ClientSession* ses,
                         const f9OmsRc_ClientConfig* cfg,
                         const char* reqName) {
   if (ses == nullptr || cfg == nullptr || reqName == nullptr)
      return nullptr;
   const OmsRcClientNote& note = OmsRcClientNote::CastFrom(*cfg);
   if (static_cast<OmsRcClientSession*>(ses)->GetNote(fon9::rc::RcFunctionCode::OmsApi) != &note)
      return nullptr;
   // note.ReqLayouts().Get() 不是 thread safe, 所以如果正在處理 Config, 則這裡不安全!
   // 如何確保 layout 持續有效呢? 只要沒斷線, 即使 TDayChanged, 應該不會變動 layout config.
   return note.ReqLayouts().Get(fon9::StrView_cstr(reqName));
}

f9OmsRc_API_FN(f9OmsRc_Layout*)
f9OmsRc_GetReportLayout(f9OmsRc_ClientSession* ses,
                        const f9OmsRc_ClientConfig* cfg,
                        unsigned rptTableId) {
   if (ses == nullptr || cfg == nullptr || rptTableId == 0)
      return nullptr;
   const OmsRcClientNote& note = OmsRcClientNote::CastFrom(*cfg);
   if (static_cast<OmsRcClientSession*>(ses)->GetNote(fon9::rc::RcFunctionCode::OmsApi) != &note)
      return nullptr;
   const auto& rptLayouts = note.RptLayouts();
   return rptTableId > rptLayouts.size() ? nullptr : rptLayouts[rptTableId - 1].get();
}

static void SendRequest(OmsRcClientSession* ses, const f9OmsRc_Layout* reqLayout,
                        fon9::RevBufferList&& rbuf, fon9::StrView logReqStr) {
   fon9::ToBitv(rbuf, reqLayout->LayoutId_);
   for (;;) {
      fon9::TimeInterval fcWait = ses->FcReq_.Fetch();
      if (fon9_LIKELY(fcWait.GetOrigValue() <= 0)) {
         ses->Send(fon9::rc::RcFunctionCode::OmsApi, std::move(rbuf));
         if (ses->LogFlags_ & f9OmsRc_ClientLogFlag_Request)
            fon9_LOG_INFO("OmsRcClient.SendReq"
                          "|ses=", fon9::ToPtr(static_cast<f9OmsRc_ClientSession*>(ses)),
                          "|tab=", reqLayout->LayoutId_, ':', reqLayout->LayoutName_,
                          "|req=", logReqStr);
         return;
      }
      fon9_LOG_INFO("OmsRcClient.SendReq.FlowControl"
                    "|ses=", fon9::ToPtr(static_cast<f9OmsRc_ClientSession*>(ses)),
                    "|wait=", fcWait);
      if (ses->Handler_.FnOnFlowControl_) // 流量管制.事件通知.
         ses->Handler_.FnOnFlowControl_(ses, static_cast<unsigned>(fcWait.ShiftUnit<6>()));
      else
         std::this_thread::sleep_for(fcWait.ToDuration());
   }
}

f9OmsRc_API_FN(void)
f9OmsRc_SendRequestString(f9OmsRc_ClientSession* ses,
                          const f9OmsRc_Layout* reqLayout,
                          const fon9_CStrView reqStr) {
   fon9::RevBufferList rbuf{128};
   const size_t reqsz = static_cast<size_t>(reqStr.End_ - reqStr.Begin_);
   fon9::RevPutMem(rbuf, reqStr.Begin_, reqsz);
   fon9::ByteArraySizeToBitvT(rbuf, reqsz);
   SendRequest(static_cast<OmsRcClientSession*>(ses), reqLayout, std::move(rbuf), reqStr);
}

f9OmsRc_API_FN(void)
f9OmsRc_SendRequestFields(f9OmsRc_ClientSession* ses,
                          const f9OmsRc_Layout* reqLayout,
                          const fon9_CStrView* reqFieldArray) {
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
   if (ses->LogFlags_ & f9OmsRc_ClientLogFlag_Request)
      logstr = fon9::BufferTo<std::string>(rbuf.cfront());
   fon9::ByteArraySizeToBitvT(rbuf, fon9::CalcDataSize(rbuf.cfront()));
   SendRequest(static_cast<OmsRcClientSession*>(ses), reqLayout, std::move(rbuf), &logstr);
}

f9OmsRc_API_FN(unsigned)
f9OmsRc_FcReqCheck(f9OmsRc_ClientSession* ses) {
   return static_cast<unsigned>(static_cast<OmsRcClientSession*>(ses)->FcReq_.Check().ShiftUnit<6>());
}

} // extern "C"
