// \file f9omsrc/OmsRcSyn.cpp
//
// #. 如何避免暴露密碼? 不使用帳密?
// #. 需要調整 oms rc 協定嗎?
//
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9omsrc/OmsRcSyn.hpp"
#include "f9omstw/OmsIoSessionFactory.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omsrc/OmsRcClient.hpp"
#include "fon9/rc/RcFuncConn.hpp"

namespace f9omstw {
using namespace fon9;
using namespace fon9::io;
using namespace fon9::rc;
using namespace fon9::seed;
//--------------------------------------------------------------------------//
static void f9OmsRc_CALL OnOmsRcSyn_Config(f9rc_ClientSession* ses, const f9OmsRc_ClientConfig* cfg);
static void f9OmsRc_CALL OnOmsRcSyn_Report(f9rc_ClientSession* ses, const f9OmsRc_ClientReport* rpt);
//--------------------------------------------------------------------------//
enum RcSynFactoryKind : uint8_t {
   /// 一般回報要求: TwsRpt, TwfRpt...
   Report,
   /// 成交回報要求: TwsFil, TwfFil, TwfFil2...
   Filled,
   /// 母單回報要求: Factory_ 建立出來的 req->IsParentRequest()==true;
   Parent,
};
static RcSynFactoryKind GetRequestFactoryKind(OmsRequestFactory& fac) {
   if (auto req = fac.MakeReportIn(f9fmkt_RxKind_RequestNew, fon9::TimeStamp{}))
      if (req->IsParentRequest())
         return RcSynFactoryKind::Parent;
   return RcSynFactoryKind::Report;
}
struct RcSynRequestFactory {
   OmsRequestFactory*   Factory_{};
   RcSynFactoryKind     Kind_{};
   char                 Padding____[7];
   void Set(OmsRequestFactory& fac, RcSynFactoryKind kind) {
      this->Factory_ = &fac;
      this->Kind_ = kind;
   }
   void Set(OmsRequestFactory& fac) {
      this->Set(fac, GetRequestFactoryKind(fac));
   }
};
class OmsRcSyn_SessionFactory : public OmsIoSessionFactory, public RcFunctionMgr {
   fon9_NON_COPY_NON_MOVE(OmsRcSyn_SessionFactory);
   using base = OmsIoSessionFactory;

   using RptFactoryMap = SortedVector<CharVector, RcSynRequestFactory>;
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
   void OnTDayChanged(OmsCore& omsCore) override {
      base::OnTDayChanged(omsCore);
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
   bool  IsOmsCoreRecovering_{};
   char  Padding_____[7];

   OmsRcSyn_SessionFactory(std::string name, OmsCoreMgrSP&& omsCoreMgr)
      : base(std::move(name), std::move(omsCoreMgr)) {
      this->Add(RcFunctionAgentSP{new OmsRcClientAgent});
      this->Add(RcFunctionAgentSP{new RcFuncConnClient("f9OmsRcSyn.0", "f9OmsRcSyn")});
      this->Add(RcFunctionAgentSP{new RcFuncSaslClient{}});
      const OmsRequestFactoryPark& facPark = this->OmsCoreMgr_->RequestFactoryPark();
      const std::string kRpt{"Rpt"};
      const std::string kFil{"Fil"};
      for (size_t idx = facPark.GetTabCount(); idx > 0;) {
         if (OmsRequestFactory* facRpt = static_cast<OmsRequestFactory*>(facPark.GetTab(--idx))) {
            CharVector facName{facRpt->Name_};
            auto       ifind = facRpt->Name_.find(kRpt);
            if (ifind == std::string::npos) {
               ifind = facRpt->Name_.find(kFil);
               if (ifind == std::string::npos)
                  continue;
               // TwsFil, TwfFil, TwfFil2:
               // - 母單成交回報, 則需配合使用 ordraw(ParentOrderRaw) 的內容回報,
               //   因為母單成交後, 可能觸發母單的其他行為, 造成其他欄位的異動,
               //   必須使用 ordraw 回報更新, 才能正確反映這類情況。
               // - 非母單成交回報: 直接使用 OmsReportFilled 回報,
               //   不需要結合 ordraw(TwsOrderRaw, TwfOrderRaw...) 的內容;
               this->RptFactoryMap_.kfetch(facName).second.Set(*facRpt, RcSynFactoryKind::Filled);
            }
            else {
               // 將 "Rpt" 取代成 New, Chg, 也就是一旦 facPark 找到 "*Rpt*" 則建立:
               // "*Rpt*", "*New*", "*Chg*" 三個回報接收 factory;
               // 例如:
               //    "TwfRpt"  => "TwfRpt",  "TwfNew",  "TwfChg";
               //    "TwfRptQ" => "TwfRptQ", "TwfNewQ", "TwfChgQ";
               // 回報內容結合 TwfNew + TwfOrd 填入 TwfRpt 然後進行回報.
               this->RptFactoryMap_.kfetch(facName).second.Set(*facRpt);
               memcpy(facName.begin() + ifind, "New", 3);
               this->RptFactoryMap_.kfetch(facName).second.Set(*facRpt);
               memcpy(facName.begin() + ifind, "Chg", 3);
               this->RptFactoryMap_.kfetch(facName).second.Set(*facRpt);
            }
         }
      }
      // -----
      this->OnAfterCtor();
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
   const RcSynRequestFactory* GetRptReportFactory(StrView layoutName) const {
      auto ifind = this->RptFactoryMap_.find(CharVector::MakeRef(layoutName));
      return ifind == this->RptFactoryMap_.end() ? nullptr : &ifind->second;
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
      const fon9::StrView strLayoutName{iLayout->LayoutName_};
      auto*  synfac = ud->GetRptReportFactory(strLayoutName);
      if (synfac == nullptr) {
         if ((synfac = ud->GetRptReportFactory(iLayout->ExParam_)) == nullptr) {
            ++iRptMap;
            continue;
         }
         if(iLayout->LayoutKind_ == f9OmsRc_LayoutKind_ReportOrder
            && synfac->Kind_ == RcSynFactoryKind::Filled) {
            // [LayoutName = 母單Ord; ExParam = 成交Req;] 的回報;
            //    => 使用[母單Rpt] 的 Request 處理;
            // 例: BergOrd + TwfFil  => BergRpt
            std::string rptName = strLayoutName.ToString();
            const auto  ipos = rptName.find("Ord");
            if (ipos != rptName.npos) {
               rptName.replace(ipos, 3, "Rpt");
               if (auto* synParentRpt = ud->GetRptReportFactory(&rptName)) {
                  if (synParentRpt->Kind_ == RcSynFactoryKind::Parent)
                     synfac = synParentRpt;
               }
            }
         }
      }
      OmsRequestFactory*  facRpt = synfac->Factory_;
      auto* pLayoutFld = iLayout->FieldArray_;
      std::vector<bool> isFldAssigned; // 是否已有名稱完全相符的欄位?
      iRptMap->Fields_.resize(iLayout->FieldCount_);
      isFldAssigned.resize(facRpt->Fields_.size());
      enum {
         LayoutName_Unknown = 0,
         LayoutName_TwsOrd  = 1,
      } layoutName{};
      if (strLayoutName == "TwsOrd") {
         layoutName = LayoutName_TwsOrd;
      }
      for (auto& iRptFld : iRptMap->Fields_) {
         StrView fldName{pLayoutFld->Named_.Name_};
         if ((iRptFld = facRpt->Fields_.Get(fldName)) != nullptr) {
            if (layoutName == LayoutName_TwsOrd && fldName == "OType") {
               // TwsOrd 的 OType 不填入 TwsRpt, 避免影響 IniOType;
               // 改成在 OmsTwsReport.cpp: OmsTwsReport::RunReportInCore_InitiatorNew(); 填入正確的 TwsOrd.OType_;
               iRptFld = nullptr;
               goto __NEXT_LAYOUT_FLD;
            }
            isFldAssigned[unsigned_cast(iRptFld->GetIndex())] = true;
         }
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
         ++pLayoutFld;
      }
      iRptMap->RptFactory_ = facRpt;
      ++iRptMap;
   }

   synSes->OmsCore_ = std::move(omsCore);
   f9OmsRc_SubscribeReport(ses, cfg, remote->LastSNO_ + 1, f9OmsRc_RptFilter_NoExternal);
   dev->Manager_->OnSession_StateUpdated(*dev, ToStrView(rbuf), LogLevel::Info);
}
//--------------------------------------------------------------------------//
static inline bool IsOmsForceInternal(const fon9_CStrView val) {
   return (val.End_ - val.Begin_) == sizeof(fon9_kCSTR_OmsForceInternal) - 1
      && memcmp(val.Begin_, fon9_kCSTR_OmsForceInternal, sizeof(fon9_kCSTR_OmsForceInternal) - 1) == 0;
}
static void OnOmsRcSyn_AssignReq(const OmsRcSynClientSession::RptDefine& rptdef, const f9OmsRc_ClientReport* rpt, OmsRequestBase& req) {
   SimpleRawWr    wr{req};
   fon9_CStrView* fldv = const_cast<fon9_CStrView*>(rpt->FieldArray_);

   if (rpt->Layout_->IdxSesName_ >= 0) {
      fon9_CStrView& sesName = fldv[rpt->Layout_->IdxSesName_];
      if (IsOmsForceInternal(sesName)) {
         #define fon9_kCSTR_OmsRcSyn   "OmsRcSyn"
         sesName.Begin_ = fon9_kCSTR_OmsRcSyn;
         sesName.End_ = sesName.Begin_ + sizeof(fon9_kCSTR_OmsRcSyn) - 1;
      }
   }

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
   req->SetSynReport();
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
      // 新單從外部來, 後續的刪改要求, 則 rpt->ReferenceSNO_ 可能沒有對應的新單回報;
      (*snoMap)[rpt->ReportSNO_] = OnOmsRcSyn_MakeRpt(rptdef, rpt);
      return; // 接下來需要等到 ordraw 回報, 才執行回報處理.
   }
   if (!ref->IsInCore()) {
      if (rpt->Layout_->LayoutKind_ != f9OmsRc_LayoutKind_ReportOrder) {
         // => rpt 為改單, ref 可能為尚未觸發的條件單 or Queuing 的新單.
         // => 此時 rpt 因為沒有送出到交易所, 所以 rpt 後續的修改(ordraw), 應直接填入此次的 ref;
         (*snoMap)[rpt->ReportSNO_] = ref;
         return;
      }
      req.reset(const_cast<OmsRequestBase*>(ref));
      OnOmsRcSyn_AssignReq(rptdef, rpt, *req);
      if (ref->RxKind() == f9fmkt_RxKind_RequestNew) {
         // 新單回報: 必須等到 [有委託書號] 才處理.
         if (OmsIsOrdNoEmpty(ref->OrdNo_)) {
            // 如果是 [沒編委託書號] 的 [失敗新單] , 則拋棄.
            if (ref->ErrCode() != OmsErrCode_NoError)
               (*snoMap)[rpt->ReferenceSNO_].reset();
            return;
         }
         // 雖然這裡有額外處理 Queuing, WaitingCond 之類的 ReportSt,
         // 但對方可能尚未編委託書號, 等到編委託書號時, 可能已經不是此狀態。
         // 因此不一定會執行到這裡的 req->SetReportSt(f9fmkt_TradingRequestSt_*AtOther);
         // 如果有需要同步 Queuing, WaitingCond 則對方需要先填入委託書號。
         if (req->ReportSt() == f9fmkt_TradingRequestSt_Queuing)
            req->SetReportSt(f9fmkt_TradingRequestSt_QueuingAtOther);
         else if (req->ReportSt() == f9fmkt_TradingRequestSt_WaitingCond)
            req->SetReportSt(f9fmkt_TradingRequestSt_WaitingCondAtOther);
      }
      else {
         // 刪改回報: 等最後結果再處理; 不理會 Sending, Queuing.
         if (ref->ReportSt() <= f9fmkt_TradingRequestSt_LastRunStep) {
            if (!req->IsParentRequest())
               return;
            // 母單的刪改回報: 因為在 Sending 狀態時仍有可能更新其他欄位, 所以必須執行回報處理.
         }
      }
      ref = nullptr;
   }
   else {
      (*snoMap)[rpt->ReportSNO_] = req = OnOmsRcSyn_MakeRpt(rptdef, rpt);
      switch (req->RxKind()) {
      default:
      case f9fmkt_RxKind_Event:
         return;
      case f9fmkt_RxKind_RequestChgCond:
      case f9fmkt_RxKind_RequestDelete:
      case f9fmkt_RxKind_RequestChgQty:
      case f9fmkt_RxKind_RequestChgPri:
      case f9fmkt_RxKind_RequestQuery:
         // 從 ref 複製必要欄位(IvacNo、Symbol...);
         req->OnSynReport(ref, nullptr);
         /* fall through */
      case f9fmkt_RxKind_RequestNew:
         // 接下來需要等到 ordraw 回報, 才知道 req 的結果, 到時才可正確處理回報.
         return;
      case f9fmkt_RxKind_Filled:
         if (ref->RxKind() == f9fmkt_RxKind_Filled) {
            // 此次的 rpt 必定為: filled 的 ordraw: rpt->Layout_ = "?Ord", rpt->ExParam_ = "?Fil?";
            // 母單成交: 需要用 ordraw(for filled) 進行回報處理;
            //          此時 req 為 ParentRpt(req->RxKind == f9fmkt_RxKind_Order) 不會來到此處.
            // 其他成交: 使用 [回報req:TwsFil,TwfFil,TwfFil2] 進行; 所以這裡直接 return; 不用回報.
            return;
         }
         // 來到此處的 req 為 [成交回報:TwsFil,TwfFil,TwfFil2], 且 ref 為 New;
         // 成交回報: 直接到 OmsCore 進行處理, 不用等候 [成交的ordraw];
         // 因為每個 OmsCore 同一筆委託的狀態不一定相同, 成交回報, 只需進行相關成交的運算即可, 不需要遠端的 ordraw;
         break;
      case f9fmkt_RxKind_Unknown:
      case f9fmkt_RxKind_Order:
         if (ref->RxKind() != f9fmkt_RxKind_RequestNew) {
            // ref = 已進入 OmsCore 的刪改, 則 req 須等到流程結束(Sending, Queuing 不處理), 才進行回報.
            if (req->ReportSt() <= f9fmkt_TradingRequestSt_LastRunStep)
               return;
            // 來到此處: req 必定為底下情況之一:
            // - req 為 遠端刪改Request(ref) 的回報.
            // - req(ParentRpt) 為母單成交回報的 ordraw;
            //   由於 ParentRpt 一般不會包含 Cum* 欄位,
            //   所以 ParentOrder 處理 ParentRpt(filled) 時:
            //    - 先更新 ParentRpt 的欄位.
            //    - 處理: 子單成交(ref) 的回報.
            if (ref->RxKind() == f9fmkt_RxKind_Filled) {
               // 母單成交回報: 必定是 [ses連線的主機] 建立的母單, 才會收到母單的(成交)回報.
               assert(req->IsParentRequest());
               // 在底下, 執行 MoveToCore() 之前, 會先執行 req->OnSynReport(ref, message);
               // 此時由 req 保留 ref, 可參考 OmsParentRequestIni::OnSynReport();
               // 然後在 OmsParentRequestIni::RunReportInCore() 可得知 RequestFilled(=此處的ref) 的所在.
               break;
            }
         }
         break;
      }
   }
   StrView message;
   if (rpt->Layout_->IdxMessage_ >= 0)
      message = rpt->FieldArray_[rpt->Layout_->IdxMessage_];
   req->OnSynReport(ref, message);
   req->SetReportNeedsLog();
   req->SetParentReport((*snoMap)[req->GetParentRequestSNO()].get());
   // -----
   OmsRequestRunner runner;
   runner.Request_ = req;
   synSes->OmsCore_->MoveToCore(std::move(runner));
}
//--------------------------------------------------------------------------//
static bool OmsRcSyn_Start(PluginsHolder& holder, StrView args) {
   class OmsRcSyn_ArgsParser : public OmsIoSessionFactoryConfigParser {
      fon9_NON_COPY_NON_MOVE(OmsRcSyn_ArgsParser);
      using base = OmsIoSessionFactoryConfigParser;
   public:
      using base::base;
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
   return OmsRcSyn_ArgsParser(holder, "OmsRcSynCli").Parse(args);
}

} // namespaces
//--------------------------------------------------------------------------//
extern "C" fon9::seed::PluginsDesc f9p_OmsRcSyn;
static fon9::seed::PluginsPark f9pRegister{"OmsRcSyn", &f9p_OmsRcSyn};
fon9::seed::PluginsDesc f9p_OmsRcSyn{"", &f9omstw::OmsRcSyn_Start, nullptr, nullptr,};
