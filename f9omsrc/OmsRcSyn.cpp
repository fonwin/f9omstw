// \file f9omsrc/OmsRcSyn.cpp
//
// #. 如何避免暴露密碼? 不使用帳密?
// #. 需要調整 oms rc 協定嗎?
//
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9omsrc/OmsRcSyn.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omsrc/OmsRcClient.hpp"
#include "fon9/seed/Plugins.hpp"
#include "fon9/rc/RcFuncConn.hpp"

namespace f9omstw {
using namespace fon9;
using namespace fon9::io;
using namespace fon9::rc;
using namespace fon9::seed;
//#define fon9_OmsRcSyn_PRINT_DEBUG_INFO
//--------------------------------------------------------------------------//
static void f9OmsRc_CALL OnOmsRcSyn_Config(f9rc_ClientSession* ses, const f9OmsRc_ClientConfig* cfg);
static void f9OmsRc_CALL OnOmsRcSyn_Report(f9rc_ClientSession* ses, const f9OmsRc_ClientReport* rpt);
//--------------------------------------------------------------------------//
class OmsRcSyn_SessionFactory : public SessionFactory, public RcFunctionMgr {
   fon9_NON_COPY_NON_MOVE(OmsRcSyn_SessionFactory);

   using RptFactoryMap = SortedVector<CharVector, OmsRequestFactory*>;
   RptFactoryMap  RptFactoryMap_;

   using HostMapImpl = SortedVector<HostId, RemoteReqMapSP>;
   using HostMap = MustLock<HostMapImpl>;
   HostMap  HostMap_;
   SubConn  SubConnTDayChanged_{};

   friend unsigned intrusive_ptr_add_ref(const OmsRcSyn_SessionFactory* p) BOOST_NOEXCEPT {
      return intrusive_ptr_add_ref(static_cast<const SessionFactory*>(p));
   }
   friend unsigned intrusive_ptr_release(const OmsRcSyn_SessionFactory* p) BOOST_NOEXCEPT {
      return intrusive_ptr_release(static_cast<const SessionFactory*>(p));
   }
   unsigned RcFunctionMgrAddRef() override {
      return intrusive_ptr_add_ref(this);
   }
   unsigned RcFunctionMgrRelease() override {
      return intrusive_ptr_release(this);
   }

   void OnParentTreeClear(Tree&) override {
      this->HostMap_.Lock()->clear();
   }
   void OnTDayChanged(OmsCore& omsCore) {
      HostMapImpl hosts = std::move(*this->HostMap_.Lock());
      for (auto& ihost : hosts) {
         auto lk = ihost.second->MapSNO_.Lock();
         if (auto* synSes = static_cast<OmsRcSynClientSession*>(ihost.second->Session_)) {
            synSes->GetDevice()->AsyncClose("TDayChanged");
         }
      }
      this->ReloadLastSNO(&omsCore);
   }

public:
   const OmsCoreMgrSP   OmsCoreMgr_;
   bool                 IsOmsCoreRecovering_{};
   char                 Padding_____[7];

   OmsRcSyn_SessionFactory(std::string name, OmsCoreMgrSP&& omsCoreMgr)
      : SessionFactory(std::move(name))
      , OmsCoreMgr_{std::move(omsCoreMgr)} {
      this->Add(RcFunctionAgentSP{new OmsRcClientAgent});
      this->Add(RcFunctionAgentSP{new RcFuncConnClient("f9OmsRcSyn.0", "f9OmsRcSyn")});
      this->Add(RcFunctionAgentSP{new RcFuncSaslClient{}});
      const OmsRequestFactoryPark& facPark = this->OmsCoreMgr_->RequestFactoryPark();
      OmsRequestFactory*           facRpt;
      // TODO: this->RptFactoryMap_ 對照表由外部設定檔匯入.
      // ----- 台灣證券.
      if ((facRpt = facPark.GetFactory("TwsRpt")) != nullptr) {
         this->RptFactoryMap_.kfetch(CharVector::MakeRef(StrView{"TwsNew"})).second = facRpt;
         this->RptFactoryMap_.kfetch(CharVector::MakeRef(StrView{"TwsChg"})).second = facRpt;
         this->RptFactoryMap_.kfetch(CharVector::MakeRef(StrView{"TwsRpt"})).second = facRpt;
      }
      if ((facRpt = facPark.GetFactory("TwsFil")) != nullptr) {
         this->RptFactoryMap_.kfetch(CharVector::MakeRef(StrView{"TwsFil"})).second = facRpt;
      }
      // ----- 台灣期權: 報價單=TwfRptQ、一般單(單式、複式)=TwfRpt.
      if ((facRpt = facPark.GetFactory("TwfRpt")) != nullptr) {
         this->RptFactoryMap_.kfetch(CharVector::MakeRef(StrView{"TwfNew"})).second = facRpt;
         this->RptFactoryMap_.kfetch(CharVector::MakeRef(StrView{"TwfChg"})).second = facRpt;
         this->RptFactoryMap_.kfetch(CharVector::MakeRef(StrView{"TwfRpt"})).second = facRpt;
      }
      if ((facRpt = facPark.GetFactory("TwfRptQ")) != nullptr) {
         this->RptFactoryMap_.kfetch(CharVector::MakeRef(StrView{"TwfNewQ"})).second = facRpt;
         this->RptFactoryMap_.kfetch(CharVector::MakeRef(StrView{"TwfChgQ"})).second = facRpt;
         this->RptFactoryMap_.kfetch(CharVector::MakeRef(StrView{"TwfRptQ"})).second = facRpt;
      }
      if ((facRpt = facPark.GetFactory("TwfFil")) != nullptr) {
         this->RptFactoryMap_.kfetch(CharVector::MakeRef(StrView{"TwfFil"})).second = facRpt;
      }
      if ((facRpt = facPark.GetFactory("TwfFil2")) != nullptr) {
         this->RptFactoryMap_.kfetch(CharVector::MakeRef(StrView{"TwfFil2"})).second = facRpt;
      }
      // -----
      this->OmsCoreMgr_->TDayChangedEvent_.Subscribe(&this->SubConnTDayChanged_,
         std::bind(&OmsRcSyn_SessionFactory::OnTDayChanged, this, std::placeholders::_1));
   }
   ~OmsRcSyn_SessionFactory() {
      this->OmsCoreMgr_->TDayChangedEvent_.Unsubscribe(&this->SubConnTDayChanged_);
   }

   SessionSP CreateSession(IoManager& mgr, const IoConfigItem& iocfg, std::string& errReason) override {
      (void)mgr;
      f9rc_ClientSessionParams f9rcCliParams;
      ZeroStruct(&f9rcCliParams);
      f9rcCliParams.UserData_ = this;
      // f9rcCliParams.RcFlags_ = f9rc_RcFlag_NoChecksum;
      // f9rcCliParams.LogFlags_ = ;

      std::string cfgstr = iocfg.SessionArgs_.ToString();
      StrView  cfgs{&cfgstr};
      StrView  tag, value;
      while (fon9::SbrFetchTagValue(cfgs, tag, value)) {
         if (const char* pend = value.end())
            *const_cast<char*>(pend) = '\0';
         if (tag == "user")
            f9rcCliParams.UserId_ = value.begin();
         else if (tag == "pass")
            f9rcCliParams.Password_ = value.begin();
         else {
            errReason = tag.ToString("Unknown tag: ");
            return nullptr;
         }
      }
      f9OmsRc_ClientSessionParams   omsRcParams;
      f9OmsRc_InitClientSessionParams(&f9rcCliParams, &omsRcParams);
      omsRcParams.FnOnConfig_ = &OnOmsRcSyn_Config;
      omsRcParams.FnOnReport_ = &OnOmsRcSyn_Report;

      return new OmsRcSynClientSession(this, &f9rcCliParams);
   }
   SessionServerSP CreateSessionServer(IoManager&, const IoConfigItem&, std::string& errReason) override {
      errReason = "OmsRcSyn_SessionFactory: Not support server.";
      return nullptr;
   }

   void ReloadLastSNO(OmsCoreSP core) {
      if (!core)
         return;
      this->IsOmsCoreRecovering_ = true;
      intrusive_ptr<OmsRcSyn_SessionFactory> pthis{this};
      core->ReportRecover(1, [pthis](OmsCore& omsCore, const OmsRxItem* item) {
         if (item == nullptr) { // 回補完畢.
            pthis->IsOmsCoreRecovering_ = false;
            return OmsRxSNO{};
         }
         if (auto req = static_cast<const OmsRequestBase*>(item->CastToRequest())) {
            // && (req->RxItemFlags() & OmsRequestFlag_ReportIn)
            HostId   hostid;
            OmsRxSNO sno = OmsReqUID_Builder::ParseReqUID(*req, hostid);
            if (sno && hostid && hostid != fon9::LocalHostId_) {
               auto remote = pthis->FetchRemoteMap(hostid);
               auto snoMap = remote->MapSNO_.Lock();
               if (remote->LastSNO_ < sno)
                  remote->LastSNO_ = sno;
               if (snoMap->size() <= sno)
                  snoMap->resize(omsCore.PublishedSNO() + 1024 * 1024);
               (*snoMap)[sno].reset(req);
            }
         }
         return item->RxSNO() + 1; // 返回下一個要回補的序號.
      });
   }

   RemoteReqMapSP FetchRemoteMap(HostId hostid) {
      HostMap::Locker hmap{this->HostMap_};
      RemoteReqMapSP& retval = hmap->kfetch(hostid).second;
      if (!retval)
         retval.reset(new RemoteReqMap);
      return retval;
   }
   OmsRequestFactory* GetRptReportFactory(StrView layoutName) const {
      auto ifind = this->RptFactoryMap_.find(CharVector::MakeRef(layoutName));
      return ifind == this->RptFactoryMap_.end() ? nullptr : ifind->second;
   }
};
//--------------------------------------------------------------------------//
bool OmsRcSynClientSession::OnDevice_BeforeOpen(fon9::io::Device& dev, std::string& cfgstr) {
   (void)cfgstr;
   OmsRcSyn_SessionFactory* ud = static_cast<OmsRcSyn_SessionFactory*>(this->UserData_);
   if (ud->IsOmsCoreRecovering_) // 回補完畢後才能開啟 Session;
      dev.AsyncClose("Wait local oms recover.");
   return true;
}
void OmsRcSynClientSession::OnDevice_StateChanged(Device& dev, const StateChangedArgs& e) {
   base::OnDevice_StateChanged(dev, e);
   if (e.BeforeState_ == State::LinkReady && this->RemoteMapSP_) {
      assert(this->RemoteMapSP_->Session_ == this);
      this->RemoteMapSP_->Session_ = nullptr;
      this->RemoteMapSP_.reset();
      this->OmsCore_.reset();
   }
}
static void f9OmsRc_CALL OnOmsRcSyn_Config(f9rc_ClientSession* ses, const f9OmsRc_ClientConfig* cfg) {
   OmsRcSyn_SessionFactory* ud = static_cast<OmsRcSyn_SessionFactory*>(ses->UserData_);
   OmsRcSynClientSession*   synSes = static_cast<OmsRcSynClientSession*>(ses);
   auto*                    dev = synSes->GetDevice();
   if (ud->IsOmsCoreRecovering_) {
      dev->AsyncClose("Wait local oms recover.");
      return;
   }

   OmsCoreSP                omsCore = ud->OmsCoreMgr_->CurrentCore();
   const auto               tdayLocal = (omsCore ? GetYYYYMMDD(omsCore->TDay()) : 0u);
   RevBufferFixedSize<128>  rbuf;
   RevPrint(rbuf, "Remote.HostId=", cfg->CoreTDay_.HostId_);
   if (fon9_UNLIKELY(tdayLocal != cfg->CoreTDay_.YYYYMMDD_)) {
      // iocfg.DeviceArgs_ 需增加 "ClosedReopen=10" 來處理重新開啟;
      RevPrint(rbuf, "err=TDay not match|TDay=", tdayLocal, "|Remote=", cfg->CoreTDay_.YYYYMMDD_, '|');
      dev->AsyncClose(rbuf.ToStrT<std::string>());
      return;
   }
   if (cfg->CoreTDay_.HostId_ == fon9::LocalHostId_) {
      RevPrint(rbuf, "err=Same as Local.HostId|");
      dev->AsyncClose(rbuf.ToStrT<std::string>());
      return;
   }
   auto remote = ud->FetchRemoteMap(cfg->CoreTDay_.HostId_);
   auto snoMap = remote->MapSNO_.Lock();
   if (remote != synSes->RemoteMapSP_) {
      if (remote->Session_ && remote->Session_ != synSes) {
         RevPrint(rbuf, "err=Dup HostId|");
         dev->AsyncClose(rbuf.ToStrT<std::string>());
         return;
      }
      if (synSes->RemoteMapSP_)
         synSes->RemoteMapSP_->Session_ = nullptr;
      synSes->RemoteMapSP_ = remote;
      remote->Session_ = synSes;
   }
   // ----- 建立匯入對照表.
   const OmsRcClientNote& note = OmsRcClientNote::CastFrom(*cfg);
   assert(synSes->GetNote(f9rc_FunctionCode_OmsApi) == &note);

   const auto& rptLayouts = note.RptLayouts();
   synSes->RptMap_.resize(rptLayouts.size());
   auto iRptMap = synSes->RptMap_.begin();
   for(auto& iLayout : rptLayouts) {
      OmsRequestFactory* facRpt = ud->GetRptReportFactory(&iLayout->Name_);
      if (facRpt == nullptr) {
         if ((facRpt = ud->GetRptReportFactory(iLayout->ExParam_)) == nullptr) {
            ++iRptMap;
            continue;
         }
      }
   #ifdef fon9_OmsRcSyn_PRINT_DEBUG_INFO
      printf("[%d]%s=>%s:[%d:%s]\n", iLayout->LayoutId_,
             iLayout->Name_.c_str(), (facRpt ? facRpt->Name_.c_str() : ""),
             iLayout->LayoutKind_, iLayout->ExParam_.Begin_);
   #endif
      auto* pLayoutFld = iLayout->FieldArray_;
      std::vector<bool> isFldAssigned; // 是否已有名稱完全相符的欄位?
      iRptMap->Fields_.resize(iLayout->FieldCount_);
      isFldAssigned.resize(facRpt->Fields_.size());
      for (auto& iRptFld : iRptMap->Fields_) {
         StrView fldName{pLayoutFld->Named_.Name_};
         if ((iRptFld = facRpt->Fields_.Get(fldName)) != nullptr)
            isFldAssigned[unsigned_cast(iRptFld->GetIndex())] = true;
         else {
            // TODO: 額外欄位對照, 使用設定檔處理?
            if (memcmp(fldName.begin(), "Ini", 3) == 0)
               fldName.SetBegin(fldName.begin() + 3);
            else if (memcmp(fldName.begin(), "Match", 5) == 0) {
               if (fldName == "MatchPri1")
                  fldName = "Pri";
               else if (fldName == "MatchPri2")
                  fldName = "PriLeg2";
               else
                  fldName.SetBegin(fldName.begin() + 5);
            }
            else if (memcmp(fldName.begin(), "Last", 4) == 0)
               fldName.SetBegin(fldName.begin() + 4);
            else if (fldName == "ReqSt")
               fldName = "ReportSt";
            else if (fldName == "AfterQty")
               fldName = "Qty";
            else if (fldName == "ReportSide") // 報價單欄位.
               fldName = "Side";
            else if (fldName == "BidAfterQty")
               fldName = "BidQty";
            else if (fldName == "OfferAfterQty")
               fldName = "OfferQty";
            else {
               goto __NEXT_LAYOUT_FLD;
            }
            if ((iRptFld = facRpt->Fields_.Get(fldName)) != nullptr) {
               if (isFldAssigned[unsigned_cast(iRptFld->GetIndex())])
                  iRptFld = nullptr;
            }
         }
      __NEXT_LAYOUT_FLD:;
      #ifdef fon9_OmsRcSyn_PRINT_DEBUG_INFO
         printf("\t" "%-13s=> %s\n", pLayoutFld->Named_.Name_.Begin_, iRptFld ? iRptFld->Name_.c_str() : "-----");
      #endif
         ++pLayoutFld;
      }
      iRptMap->RptFactory_ = facRpt;
      ++iRptMap;
   }
#ifdef fon9_OmsRcSyn_PRINT_DEBUG_INFO
   puts("-----");
#endif

   synSes->OmsCore_ = std::move(omsCore);
   f9OmsRc_SubscribeReport(ses, cfg, remote->LastSNO_ + 1, f9OmsRc_RptFilter_NoExternal);
   dev->Manager_->OnSession_StateUpdated(*dev, ToStrView(rbuf), LogLevel::Info);
}
//--------------------------------------------------------------------------//
static void OnOmsRcSyn_AssignReq(const OmsRcSynClientSession::RptDefine& rptdef, const f9OmsRc_ClientReport* rpt, OmsRequestBase& req) {
   SimpleRawWr  wr{req};
   const fon9_CStrView* fldv = rpt->FieldArray_;
   for (const Field* fld : rptdef.Fields_) {
      if (fld)
         fld->StrToCell(wr, *fldv);
      ++fldv;
   }
}
static OmsRequestSP OnOmsRcSyn_MakeRpt(const OmsRcSynClientSession::RptDefine& rptdef, const f9OmsRc_ClientReport* rpt) {
   f9fmkt_RxKind kind = (rpt->Layout_->IdxKind_ >= 0
                         ? static_cast<f9fmkt_RxKind>(*rpt->FieldArray_[rpt->Layout_->IdxKind_].Begin_)
                         : f9fmkt_RxKind_Unknown); // 此時應該是 f9fmkt_RxKind_Order;
   OmsRequestSP  req = rptdef.RptFactory_->MakeReportIn(kind, fon9::UtcNow());
   OnOmsRcSyn_AssignReq(rptdef, rpt, *req);
   return req;
}
static void f9OmsRc_CALL OnOmsRcSyn_Report(f9rc_ClientSession* ses, const f9OmsRc_ClientReport* rpt) {
   if (rpt->ReportSNO_ == 0)
      return;
   OmsRcSynClientSession*  synSes = static_cast<OmsRcSynClientSession*>(ses);
   assert(synSes->RemoteMapSP_.get() != nullptr);
   auto snoMap = synSes->RemoteMapSP_->MapSNO_.Lock();
   synSes->RemoteMapSP_->LastSNO_ = rpt->ReportSNO_;
   if (rpt->Layout_ == NULL)
      return;
   switch (rpt->Layout_->LayoutKind_) {
   default:
   case f9OmsRc_LayoutKind_Request:
   case f9OmsRc_LayoutKind_ReportRequestAbandon:
   case f9OmsRc_LayoutKind_ReportEvent:
      return;
   case f9OmsRc_LayoutKind_ReportRequest:
   case f9OmsRc_LayoutKind_ReportRequestIni:
   case f9OmsRc_LayoutKind_ReportOrder:
      break;
   }
   // # 刪改: 僅在最後成功後回報, 過程(Queuing, Sending)不執行回報.
   // # rpt->ReferenceSNO_
   //   - 由於訂閱回報時, 使用 NoExternal 排除了外部回報,
   //     所以除了新單要求, 其餘回報的 rpt->ReferenceSNO_ 必定有效.
   //   - 可是如果是新單從外部來, 刪改的下單要求, 則 rpt->ReferenceSNO_ 可能沒有對應的新單回報;
   assert(0 < rpt->Layout_->LayoutId_ && rpt->Layout_->LayoutId_ <= synSes->RptMap_.size());
   if (!(0 < rpt->Layout_->LayoutId_ && rpt->Layout_->LayoutId_ <= synSes->RptMap_.size()))
      return;
   const auto& rptdef = synSes->RptMap_[unsigned_cast(rpt->Layout_->LayoutId_ - 1)];
   if (rptdef.RptFactory_ == nullptr)
      return;
   // -----
#ifdef fon9_OmsRcSyn_PRINT_DEBUG_INFO
   char  msgbuf[1024 * 4];
   char* pmsg = msgbuf;
   pmsg += sprintf(pmsg, "%u:%s(%u|%s)[sno=%lu|ref=%lu]", rpt->Layout_->LayoutId_, rpt->Layout_->LayoutName_.Begin_,
                   rpt->Layout_->LayoutKind_, rpt->Layout_->ExParam_.Begin_,
                   static_cast<unsigned long>(rpt->ReportSNO_), static_cast<unsigned long>(rpt->ReferenceSNO_));
   for (unsigned L = 0; L < rpt->Layout_->FieldCount_; ++L) {
      pmsg += sprintf(pmsg, "|%s=%s",
                      rpt->Layout_->FieldArray_[L].Named_.Name_.Begin_,
                      rpt->FieldArray_[L].Begin_);
   }
   puts(msgbuf);
#endif
   // -----
   if (snoMap->size() <= rpt->ReportSNO_)
      snoMap->resize(rpt->ReportSNO_ + 1024 * 1024);
   else {
      // assert((*snoMap)[rpt->ReportSNO_].get() == nullptr);
      if ((*snoMap)[rpt->ReportSNO_].get() != nullptr)
         return;
   }
   OmsRequestSP req;
   if (rpt->ReferenceSNO_ == 0) {
      // 新單 or 無新單的亂序回報(但是,因訂閱回報時使用 NoExternal 所以不會有此情況).
      (*snoMap)[rpt->ReportSNO_] = req = OnOmsRcSyn_MakeRpt(rptdef, rpt);
      assert(req->RxKind() == f9fmkt_RxKind_RequestNew);
      return; // 接下來需要等到 ordraw 回報, 才執行回報處理.
   }
   const OmsRequestBase* ref = (*snoMap)[rpt->ReferenceSNO_].get();
   if (ref == nullptr) {
      // 新單從外部來, 刪改的下單要求, 則 rpt->ReferenceSNO_ 可能沒有對應的新單回報;
      (*snoMap)[rpt->ReportSNO_] = OnOmsRcSyn_MakeRpt(rptdef, rpt);
      return; // 接下來需要等到 ordraw 回報, 才執行回報處理.
   }
   if (ref->use_count() == 1) {
      // 尚未執行回報: 直接使用 ref 執行回報即可;
      req.reset(const_cast<OmsRequestBase*>(ref));
      OnOmsRcSyn_AssignReq(rptdef, rpt, *req);
      if (ref->RxKind() == f9fmkt_RxKind_RequestNew) {
         // 新單回報: 必須等到有委託書號才處理.
         // 如果是新單失敗沒編委託書號, 則拋棄此筆回報.
         if (OmsIsOrdNoEmpty(ref->OrdNo_)) {
            if (ref->ErrCode() != OmsErrCode_NoError)
               (*snoMap)[rpt->ReferenceSNO_].reset();
            return;
         }
      }
      else {
         // 刪改回報: 等最後結果再處理(Sending, Queuing 不處理).
         if (ref->ReportSt() <= f9fmkt_TradingRequestSt_LastRunStep)
            return;
      }
      ref = nullptr;
   }
   else {
      (*snoMap)[rpt->ReportSNO_] = req = OnOmsRcSyn_MakeRpt(rptdef, rpt);
      switch (req->RxKind()) {
      default:
      case f9fmkt_RxKind_Event:
         return;
      case f9fmkt_RxKind_RequestDelete:
      case f9fmkt_RxKind_RequestChgQty:
      case f9fmkt_RxKind_RequestChgPri:
      case f9fmkt_RxKind_RequestQuery:
         // 從 ref 複製必要欄位(IvacNo、Symbol...);
         req->OnSynReport(ref, nullptr);
         /* fall through */
      case f9fmkt_RxKind_RequestNew:
         // 接下來需要等到 ordraw 回報, 才執行回報處理.
         return;
      case f9fmkt_RxKind_Filled:
         if (ref->RxKind() == f9fmkt_RxKind_Filled) // 不處理: filled 的 ordraw.
            return;
         break;
      case f9fmkt_RxKind_Unknown:
      case f9fmkt_RxKind_Order:
         if (ref->RxKind() != f9fmkt_RxKind_RequestNew) {
            // ref = 已進入 OmsCore 的刪改, 則 req 必須流程結束(Sending, Queuing 不處理).
            if (req->ReportSt() <= f9fmkt_TradingRequestSt_LastRunStep)
               return;
         }
         break;
      }
   }
   StrView message;
   if (rpt->Layout_->IdxMessage_ >= 0)
      message = rpt->FieldArray_[rpt->Layout_->IdxMessage_];
   req->OnSynReport(ref, message);
   req->SetReportNeedsLog();
   // -----
   OmsRequestRunner runner;
   runner.Request_ = req;
   synSes->OmsCore_->MoveToCore(std::move(runner));
}
//--------------------------------------------------------------------------//
static bool OmsRcSyn_Start(PluginsHolder& holder, StrView args) {
   class OmsRcSyn_ArgsParser : public SessionFactoryConfigParser {
      fon9_NON_COPY_NON_MOVE(OmsRcSyn_ArgsParser);
      using base = SessionFactoryConfigParser;
   public:
      PluginsHolder& PluginsHolder_;
      StrView        OmsCoreName_;

      OmsRcSyn_ArgsParser(PluginsHolder& pluginsHolder)
         : base{"OmsRcSynCli"}
         , PluginsHolder_{pluginsHolder} {
      }
      bool OnUnknownTag(seed::PluginsHolder& holder, StrView tag, StrView value) {
         if (tag == "OmsCore") {
            this->OmsCoreName_ = value;
            return true;
         }
         return base::OnUnknownTag(holder, tag, value);
      }
      OmsCoreMgrSP GetOmsCoreMgr() {
         if (auto omsCoreMgr = this->PluginsHolder_.Root_->GetSapling<OmsCoreMgr>(this->OmsCoreName_))
            return omsCoreMgr;
         this->ErrMsg_ += "|err=Unknown OmsCore";
         if (!this->OmsCoreName_.empty()) {
            this->ErrMsg_.push_back('=');
            this->OmsCoreName_.AppendTo(this->ErrMsg_);
         }
         return nullptr;
      }
      SessionFactorySP CreateSessionFactory() override {
         if (auto omsCoreMgr = this->GetOmsCoreMgr()) {
            intrusive_ptr<OmsRcSyn_SessionFactory> retval{new OmsRcSyn_SessionFactory(this->Name_, std::move(omsCoreMgr))};
            // 不能在建構時呼叫 retval->ReloadLastSNO(this->OmsCoreMgr_->CurrentCore());
            // 因為裡面有 intrusive_ptr<OmsRcSyn_SessionFactory> pthis; 可能會造成建構完成前刪除!
            retval->ReloadLastSNO(retval->OmsCoreMgr_->CurrentCore());
            return retval;
         }
         return nullptr;
      }
   };
   return OmsRcSyn_ArgsParser{holder}.Parse(holder, args);
}

} // namespaces
//--------------------------------------------------------------------------//
extern "C" fon9::seed::PluginsDesc f9p_OmsRcSyn;
static fon9::seed::PluginsPark f9pRegister{"OmsRcSyn", &f9p_OmsRcSyn};
fon9::seed::PluginsDesc f9p_OmsRcSyn{"", &f9omstw::OmsRcSyn_Start, nullptr, nullptr,};
