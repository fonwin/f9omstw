// \file f9omsrc/OmsRcSyn.cpp
//
// 如何避免暴露密碼? 不使用帳密?
// ==> 使用 PassKey 機制:
//     fon9/PassKey.hpp
//     fon9/auth/PassIdMgr.hpp
//       fon9::auth::PassIdMgr::PlantPassKeyMgr(*this->MaAuth_, "PassIdMgr"); // 在程式啟動時執行;
//
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9omsrc/OmsRcSyn.hpp"
#include "f9omsrc/OmsRcClient.hpp"
#include "fon9/FileReadAll.hpp"
#include "fon9/FilePath.hpp"
#include "fon9/PassKey.hpp"

namespace f9omstw {
using namespace fon9;
using namespace fon9::io;
using namespace fon9::rc;
using namespace fon9::seed;

static const char kCSTR_SplRemoteStillConnectedLn[] = "|The remote host is still connected.\n";
//--------------------------------------------------------------------------//
OmsRcSyn_SessionFactory::OmsRcSyn_SessionFactory(std::string       name,
                                                 OmsCoreMgrSP&&    omsCoreMgr,
                                                 f9OmsRc_RptFilter rptFilter,
                                                 OmsRcClientAgent* omsRcCliAgent,
                                                 RcFuncConnClient* connCliAgent)
   : base(std::move(name), std::move(omsCoreMgr))
   , RptFilter_{rptFilter} {
   this->Add(RcFunctionAgentSP{omsRcCliAgent ? omsRcCliAgent : new OmsRcClientAgent});
   this->Add(RcFunctionAgentSP{connCliAgent ? connCliAgent : new RcFuncConnClient("f9OmsRcSyn.0", "f9OmsRcSyn")});
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
unsigned OmsRcSyn_SessionFactory::RcFunctionMgrAddRef() {
   return intrusive_ptr_add_ref(this);
}
unsigned OmsRcSyn_SessionFactory::RcFunctionMgrRelease() {
   return intrusive_ptr_release(this);
}

SessionServerSP OmsRcSyn_SessionFactory::CreateSessionServer(IoManager&, const IoConfigItem&, std::string& errReason) {
   errReason = this->Name_ + ": Not support server.";
   return nullptr;
}
SessionSP OmsRcSyn_SessionFactory::CreateSession(IoManager& mgr, const IoConfigItem& iocfg, std::string& errReason) {
   (void)mgr;
   f9rc_ClientSessionParams f9rcCliParams;
   ZeroStruct(&f9rcCliParams);
   f9rcCliParams.UserData_ = this;
   f9rcCliParams.Password_ = "";
   // f9rcCliParams.RcFlags_ = f9rc_RcFlag_NoChecksum;
   // f9rcCliParams.LogFlags_ = ;

   std::string cfgstr = iocfg.SessionArgs_.ToString();
   CharVector  passbuf;
   StrView     cfgs{&cfgstr};
   StrView     tag, value;
   while (SbrFetchTagValue(cfgs, tag, value)) {
      if (const char* pend = value.end())
         *const_cast<char*>(pend) = '\0';
      if (fon9::iequals(tag, "user"))
         f9rcCliParams.UserId_ = value.begin();
      else if (fon9::iequals(tag, "pass"))
         f9rcCliParams.Password_ = value.begin();
      else if (fon9::iequals(tag, "PassKey")) {
         PassKeyToPassword(value, passbuf);
         f9rcCliParams.Password_ = passbuf.begin();
      }
      else {
         errReason = tag.ToString("Unknown tag: ");
         return nullptr;
      }
   }
   if (f9rcCliParams.UserId_ == nullptr) {
      errReason = "Not found 'User' tag.";
      return nullptr;
   }
   f9OmsRc_ClientSessionParams   omsRcParams;
   f9OmsRc_InitClientSessionParams(&f9rcCliParams, &omsRcParams);
   OmsRcSynClientSession::InitClientSessionParams(omsRcParams);
   return this->CreateOmsRcSynClientSession(f9rcCliParams);
}
OmsRcSynClientSessionSP OmsRcSyn_SessionFactory::CreateOmsRcSynClientSession(f9rc_ClientSessionParams& f9rcCliParams) {
   return new OmsRcSynClientSession(this, &f9rcCliParams);
}

void OmsRcSyn_SessionFactory::OnParentTreeClear(Tree&) {
   this->HostMap_.Lock()->clear();
}
void OmsRcSyn_SessionFactory::OnTDayChanged(OmsCore& omsCore) {
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

void OmsRcSyn_SessionFactory::ReloadLastSNO(OmsCoreSP core) {
   if (!core)
      return;
   this->IsOmsCoreRecovering_ = true;
   auto coreLocker = core->LockRxItems();
   const auto last = core->LastSNO();
   for (OmsRxSNO snoL = 0; snoL <= last;) {
      const auto* item = core->GetRxItem(++snoL, coreLocker);
      if (item == nullptr)
         continue;
      const auto* req = static_cast<const OmsRequestBase*>(item->CastToRequest());
      if (req == nullptr)
         continue;
      HostId   hostid;
      OmsRxSNO sno = OmsReqUID_Builder::ParseReqUID(*req, hostid);
      if (sno && hostid && hostid != fon9::LocalHostId_) {
         const_cast<OmsRequestBase*>(req)->SetSynReport();
         #ifdef _DEBUG
            const_cast<OmsRequestBase*>(req)->SetOrigHostId(hostid);
         #endif
         auto remote = this->FetchRemoteMap(hostid);
         auto snoMap = remote->MapSNO_.Lock();
         if (remote->LastSNO_ < sno)
            remote->LastSNO_ = sno;
         if (snoMap->size() <= sno)
            snoMap->resize(sno + 1024 * 1024);
         (*snoMap)[sno].reset(req);
      }
   }
   this->ReloadLogMapSNO(*core, coreLocker);
   this->IsOmsCoreRecovering_ = false;
}
void OmsRcSyn_SessionFactory::ReloadLogMapSNO(OmsCore& omsCore, OmsBackend::Locker& coreLocker) {
   if (!(this->RptFilter_ & f9OmsRc_RptFilter_IncludeRcSynNew))
      return;
   auto res = this->LogMapSNO_.Open(fon9::FilePath::AppendPath(&omsCore.LogPath(), "OmsRcSynMap.log"),
                                    FileMode::Append | FileMode::CreatePath | FileMode::Read);
   if (res.IsError()) {
      fon9_LOG_ERROR("OmsRcSyn: Open log|fname=", this->LogMapSNO_.GetOpenName(), '|', res);
      return;
   }
   File::PosType  fpos = 0;
   LinePeeker     lnPeeker;
   OmsRequestId   reqUID;
   FileReadAll(this->LogMapSNO_, fpos, [&](DcQueueList& rdbuf, File::Result& rdres) {
      (void)rdres;
      while (const char* pln = lnPeeker.PeekUntil(rdbuf, '\n')) {
         StrView  lnstr{pln, lnPeeker.LineSize_ - 1};// -1:不用換行字元.
         HostId   srcHostId = StrTo(&lnstr, HostId{});
         lnstr.SetBegin(lnstr.begin() + 1);
         OmsRxSNO srcSNO = StrTo(&lnstr, OmsRxSNO{});
         lnstr.SetBegin(lnstr.begin() + 1);
         reqUID.ReqUID_.AssignFrom(lnstr);
         HostId   origHostId;
         OmsRxSNO origSNO = OmsReqUID_Builder::ParseReqUID(reqUID, origHostId);
         RemoteReqMap::OmsRequestSP origReq;
         if (origHostId == fon9::LocalHostId_) {
            if (auto* rxitem = omsCore.GetRxItem(origSNO, coreLocker))
               origReq.reset(static_cast<const OmsRequestBase*>(rxitem->CastToRequest()));
         }
         else {
            auto origMap = this->FetchRemoteMap(origHostId)->MapSNO_.Lock();
            // assert(origSNO < origMap->size()); 有可能該筆 ReqUID 回報被放棄(例: Bad BrkId),
            // 造成 ReloadLastSNO() 時, 沒有該筆回報, 所以 assert(origSNO < origMap->size()) 不一定成立;
            if (origSNO < origMap->size())
               origReq = (*origMap)[origSNO];
         }
         if (origReq) {
            auto srcHost = this->FetchRemoteMap(srcHostId);
            auto srcMap = srcHost->MapSNO_.Lock();
            if (srcMap->size() <= srcSNO)
               srcMap->resize(srcSNO + 1024 * 1024);
            assert((*srcMap)[srcSNO].get() == nullptr);
            (*srcMap)[srcSNO] = origReq;
            if (srcHost->LastSNO_ < srcSNO)
               srcHost->LastSNO_ = srcSNO;
         }
         lnPeeker.PopConsumed(rdbuf);
      }
      return true;
   });
}

RemoteReqMapSP OmsRcSyn_SessionFactory::FetchRemoteMap(HostId hostid) {
   HostMap::Locker hmap{this->HostMap_};
   RemoteReqMapSP& retval = hmap->kfetch(hostid).second;
   if (!retval)
      retval.reset(new RemoteReqMap);
   return retval;
}
RemoteReqMapSP OmsRcSyn_SessionFactory::FindRemoteMap(HostId hostid) {
   HostMap::Locker hmap{this->HostMap_};
   auto            ifind = hmap->find(hostid);
   return(ifind == hmap->end() ? nullptr : ifind->second);
}
const RcSynRequestFactory* OmsRcSyn_SessionFactory::GetRptReportFactory(StrView layoutName) const {
   auto ifind = this->RptFactoryMap_.find(CharVector::MakeRef(layoutName));
   return ifind == this->RptFactoryMap_.end() ? nullptr : &ifind->second;
}
void OmsRcSyn_SessionFactory::AppendMapSNO(HostId srcHostId, OmsRxSNO srcSNO, const OmsRequestId& origId) {
   if (this->LogMapSNO_.IsOpened()) {
      RevBufferFixedSize<256> rbuf;
      RevPrint(rbuf, srcHostId, '|', srcSNO, '|', origId.ReqUID_, '\n');
      this->LogMapSNO_.Append(ToStrView(rbuf));
   }
}

void OmsRcSyn_SessionFactory::RerunParentInCore(fon9::HostId hostid, OmsResource& resource) {
   RemoteReqMapSP remoteHost = this->FindRemoteMap(hostid);
   RevBufferList  rbuf{256};
   if (!remoteHost) {
      RevPrint(rbuf, "Host not found.\n");
   __RETURN_LOG_ERROR_HEAD:
      RevPrint(rbuf, fon9::LocalNow(), "|RcSyn.EnableParent|HostId=", hostid);
      resource.LogAppend(std::move(rbuf));
      return;
   }
   if (remoteHost->Session_) {
      RevPrint(rbuf, kCSTR_SplRemoteStillConnectedLn);
      goto __RETURN_LOG_ERROR_HEAD;
   }
   RevPrint(rbuf, fon9::LocalNow(), "|RcSyn.EnableParent.Start|HostId=", hostid, '\n');
   resource.LogAppend(std::move(rbuf));
   RevPrint(rbuf, '\n');
   OmsRxSNO count = 0;
   {
      auto reqs = remoteHost->MapSNO_.Lock();
      auto maxLoop = remoteHost->LastSNO_ + 1; // +1: 序號0也要執行一次;
      for (auto& ireq : *reqs) {
         if (maxLoop <= 0)
            break;
         --maxLoop;
         if (!ireq || ireq->OrigHostId() != hostid)
            continue;
         if (auto* ordraw = ireq->LastUpdated()) {
            if (ordraw->Order().RerunParent(resource)) {
               RevPrint(rbuf, '|', ireq->ReqUID_);
               ++count;
            }
         }
      }
   }
   RevPrint(rbuf, fon9::LocalNow(), "|RcSyn.EnableParent.End|HostId=", hostid, "|count=", count);
   resource.LogAppend(std::move(rbuf));
}
void OmsRcSyn_SessionFactory::OnSeedCommand(SeedOpResult& res, StrView cmdln, FnCommandResultHandler resHandler,
                                            MaTreeBase::Locker&& ulk, SeedVisitor* visitor) {
#define kCSTR_RerunParent_Prefix    "rerun."
#define kSIZE_RerunParent_Prefix    (sizeof(kCSTR_RerunParent_Prefix)-1)
   std::string strResult;
   if (cmdln == "?") {
      // 每個 HostId 建立一個 MenuItem;
      auto hosts = this->HostMap_.Lock();
      if (hosts->empty()) {
         res.OpResult_ = OpResult::not_found_key;
         resHandler(res, "No remote host found.");
         return;
      }
      char chSpl = 0;
      for (auto i : *hosts) {
         if (chSpl)
            strResult.push_back(chSpl);
         fon9::RevPrintAppendTo(strResult, kCSTR_RerunParent_Prefix, i.first, fon9_kCSTR_CELLSPL "Host.", i.first, ": Rerun ParentOrder");
         chSpl = *fon9_kCSTR_ROWSPL;
      }
   }
   else if (cmdln.size() > kSIZE_RerunParent_Prefix && memcmp(cmdln.begin(), kCSTR_RerunParent_Prefix, kSIZE_RerunParent_Prefix) == 0) {
      fon9::HostId hostid = StrTo(StrView{cmdln.begin() + kSIZE_RerunParent_Prefix, cmdln.end()}, fon9::HostId{});
      if (RemoteReqMapSP remoteHost = this->FindRemoteMap(hostid)) {
         if (remoteHost->Session_) {
            // 遠端主機然在連線中, 禁止在本地啟用 hostid 的 ParentOrder.
            // 因為: 可能造成雙方同持觸發送出子單!
            res.OpResult_ = OpResult::bad_command_argument;
            resHandler(res, StrView{kCSTR_SplRemoteStillConnectedLn + 1, sizeof(kCSTR_SplRemoteStillConnectedLn) - 3});
            return;
         }
         this->OmsCoreMgr_->CurrentCore()->RunCoreTask(std::bind(&OmsRcSyn_SessionFactory::RerunParentInCore, this, hostid, std::placeholders::_1));
         strResult = fon9::RevPrintTo<std::string>("Enable ParentOrder: HostId=", hostid);
      }
      else {
         res.OpResult_ = OpResult::bad_command_argument;
         resHandler(res, "HostId not found.");
         return;
      }
   }
   else {
      base::OnSeedCommand(res, cmdln, std::move(resHandler), std::move(ulk), visitor);
      return;
   }
   resHandler(res, &strResult);
}
//--------------------------------------------------------------------------//
void OmsRcSynClientSession::OnOmsRcSyn_ConfigReady() {
}
void OmsRcSynClientSession::OnOmsRcSyn_LinkBroken() {
   if (this->OmsCore_ && this->HostId_) {
      RevBufferList rbuf{128};
      RevPrint(rbuf, LocalNow(), "|OmsRcSyn.LinkBroken|HostId=", this->HostId_, '\n');
      this->OmsCore_->LogAppend(std::move(rbuf));
   }
}
bool OmsRcSynClientSession::OnDevice_BeforeOpen(Device& dev, std::string& cfgstr) {
   (void)cfgstr;
   OmsRcSyn_SessionFactory* ud = static_cast<OmsRcSyn_SessionFactory*>(this->UserData_);
   if (ud->IsOmsCoreRecovering_) // 回補完畢後才能開啟 Session;
      dev.AsyncClose("Wait local oms recover.");
   return true;
}
void OmsRcSynClientSession::OnDevice_StateChanged(Device& dev, const StateChangedArgs& e) {
   base::OnDevice_StateChanged(dev, e);
   if (e.BeforeState_ == State::LinkReady && this->RemoteMap_) {
      assert(this->RemoteMap_->Session_ == this);
      this->RemoteMap_->Session_ = nullptr;
      if (this->RemoteMap_->IsReportRecovering_)
         this->OnOmsRcSyn_ReportRecoverEnd();
      this->OnOmsRcSyn_LinkBroken();
      this->RemoteMap_.reset();
      this->OmsCore_.reset();
   }
}
void OmsRcSynClientSession::OnOmsRcSyn_ReportRecoverEnd() {
   this->RemoteMap_->IsReportRecovering_ = false;
   if (this->OmsCore_ && this->RemoteMap_) {
      RevBufferList rbuf{128};
      RevPrint(rbuf, LocalNow(), "|OmsRcSyn.ReportRecoverEnd|HostId=", this->HostId_, "|LastSeqNo=", this->RemoteMap_->LastSNO_, '\n');
      this->OmsCore_->LogAppend(std::move(rbuf));
   }
}
//--------------------------------------------------------------------------//
static const f9OmsRc_LayoutField* FindLayoutField(const f9OmsRc_Layout& layout, StrView fldName) {
   auto*       pLayoutFld = layout.FieldArray_;
   auto* const endLayoutFld = pLayoutFld + layout.FieldCount_;
   for (; pLayoutFld != endLayoutFld; ++pLayoutFld) {
      if (fldName == pLayoutFld->Named_.Name_)
         return pLayoutFld;
   }
   return nullptr;
}
void f9OmsRc_CALL OmsRcSynClientSession::OnOmsRcSyn_Config(f9rc_ClientSession* ses, const f9OmsRc_ClientConfig* cfg) {
   OmsRcSyn_SessionFactory* ud = static_cast<OmsRcSyn_SessionFactory*>(ses->UserData_);
   OmsRcSynClientSession*   synSes = static_cast<OmsRcSynClientSession*>(ses);
   auto*                    dev = synSes->GetDevice();
   if (ud->IsOmsCoreRecovering_) {
      dev->AsyncClose("Wait local oms recover.");
      return;
   }

   OmsCoreSP                omsCore = ud->OmsCoreMgr_->CurrentCore();
   const auto               tdayLocal = (omsCore ? GetYYYYMMDD(omsCore->TDay()) : 0u);
   RevBufferFixedSize<256>  rbuf;
   RevPrint(rbuf, "Remote.HostId=", synSes->HostId_ = cfg->CoreTDay_.HostId_);
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
   if (remote != synSes->RemoteMap_) {
      if (remote->Session_ && remote->Session_ != synSes) {
         RevPrint(rbuf, "err=Dup HostId|");
         dev->AsyncClose(rbuf.ToStrT<std::string>());
         return;
      }
      if (synSes->RemoteMap_)
         synSes->RemoteMap_->Session_ = nullptr;
      synSes->RemoteMap_ = remote;
      remote->Session_ = synSes;
   }
   // ----- 建立匯入對照表.
   const OmsRcClientNote& note = OmsRcClientNote::CastFrom(*cfg);
   assert(synSes->GetNote(f9rc_FunctionCode_OmsApi) == &note);

   const auto& rptLayouts = note.RptLayouts();
   synSes->RptMap_.resize(rptLayouts.size());
   auto iRptMap = synSes->RptMap_.begin();
   for(auto& iLayout : rptLayouts) {
      const StrView         strLayoutName{iLayout->LayoutName_};
      auto*                 synfac = ud->GetRptReportFactory(strLayoutName);
      const f9OmsRc_Layout* ordSrcLayout = nullptr;
      if (synfac == nullptr) {
         const StrView strOrdSrcName{iLayout->ExParam_};
         if ((synfac = ud->GetRptReportFactory(strOrdSrcName)) == nullptr) {
            ++iRptMap;
            continue;
         }
         if (iLayout->LayoutKind_ == f9OmsRc_LayoutKind_ReportOrder) {
            for (auto& xLayout : rptLayouts) {
               if (strOrdSrcName == xLayout->LayoutName_) {
                  ordSrcLayout = xLayout.get();
                  break;
               }
            }
            if (synfac->Kind_ == RcSynFactoryKind::Filled) {
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
      }
      OmsRequestFactory* facRpt = synfac->Factory_;
      auto*              pLayoutFld = iLayout->FieldArray_;
      std::vector<bool>  isFldAssigned; // 是否已有名稱完全相符的欄位?
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
            if (memcmp(fldName.begin(), "Ini", 3) == 0) {
               fldName.SetBegin(fldName.begin() + 3);
               if (FindLayoutField(*iLayout, fldName))
                  goto __NEXT_LAYOUT_FLD;
               if (ordSrcLayout && FindLayoutField(*ordSrcLayout, fldName))
                  goto __NEXT_LAYOUT_FLD;
            }
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
   if (omsCore) {
      RevBufferList rmsg{128};
      RevPrint(rmsg, LocalNow(),
               "|OmsRcSyn.ReportRecover|HostId=", synSes->HostId_,
               "|SeqNo=", remote->LastSNO_ + 1,
               "|Fc=", cfg->FcReqCount_, '/', cfg->FcReqMS_,
               "|Rights=", fon9::ToHex(cfg->RightFlags_),
               '\n');
      omsCore->LogAppend(std::move(rmsg));
   }
   synSes->OmsCore_ = std::move(omsCore);
   synSes->RemoteMap_->IsReportRecovering_ = true;
   f9OmsRc_SubscribeReport(ses, cfg, remote->LastSNO_ + 1, ud->RptFilter_);
   dev->Manager_->OnSession_StateUpdated(*dev, ToStrView(rbuf), LogLevel::Info);
   synSes->OnOmsRcSyn_ConfigReady();
}
//--------------------------------------------------------------------------//
static inline bool IsOmsForceInternal(const fon9_CStrView val) {
   return (val.End_ - val.Begin_) == sizeof(fon9_kCSTR_OmsForceInternal) - 1
      && memcmp(val.Begin_, fon9_kCSTR_OmsForceInternal, sizeof(fon9_kCSTR_OmsForceInternal) - 1) == 0;
}
static void OnOmsRcSyn_AssignReq(const OmsRcSynRptDefine& rptdef, const f9OmsRc_ClientReport* rpt, OmsRequestBase& req) {
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
static OmsRequestSP OnOmsRcSyn_MakeRpt(const OmsRcSynRptDefine& rptdef, const f9OmsRc_ClientReport* rpt) {
   f9fmkt_RxKind kind = (rpt->Layout_->IdxKind_ >= 0
                         ? static_cast<f9fmkt_RxKind>(*rpt->FieldArray_[rpt->Layout_->IdxKind_].Begin_)
                         : f9fmkt_RxKind_Unknown); // 此時應該是 f9fmkt_RxKind_Order;
   OmsRequestSP  req = rptdef.RptFactory_->MakeReportIn(kind, fon9::UtcNow());
   req->SetSynReport();
   OnOmsRcSyn_AssignReq(rptdef, rpt, *req);
   return req;
}
bool OmsRcSynClientSession::RemapReqUID(OmsRequestBase& rptReq, RemoteReqMap::OmsRequestSP& riSrcReqSP, OmsRxSNO srcRptSNO) {
   // 任何回報, 都有可能來自 [非:原接單主機], 此時 rptReq 必須是整個系統唯一, 所以:
   // (*snoMap)[apiRpt->ReportSNO_] 與 (OrigHost.MapSNO_)[origSNO] 必須是同一個 rptReq;
   fon9::HostId   origHostId;
   const OmsRxSNO origSNO = OmsReqUID_Builder::ParseReqUID(rptReq, origHostId);
   if (fon9_UNLIKELY(origSNO == 0 || origHostId == 0))
      return false;
   rptReq.SetOrigHostId(origHostId);
   if (fon9_LIKELY(origHostId == this->HostId_))
      return false;
   OmsRcSyn_SessionFactory* ud = static_cast<OmsRcSyn_SessionFactory*>(this->UserData_);
   if (fon9_UNLIKELY(origHostId == fon9::LocalHostId_)) {
      if (auto* origReq = dynamic_cast<const OmsRequestBase*>(this->OmsCore_->GetRxItem(origSNO)))
         riSrcReqSP.reset(origReq);
   }
   else {
      auto origMap = ud->FetchRemoteMap(origHostId);
      auto snoMap = origMap->MapSNO_.Lock();
      if (snoMap->size() <= origSNO)
         snoMap->resize(origSNO + 1024 * 1024);
      auto& origReqSP = (*snoMap)[origSNO];
      if (origReqSP)
         riSrcReqSP = origReqSP;
      else
         origReqSP = riSrcReqSP;
   }
   ud->AppendMapSNO(this->HostId_, srcRptSNO, *riSrcReqSP);
   return true;
}
void f9OmsRc_CALL OmsRcSynClientSession::OnOmsRcSyn_Report(f9rc_ClientSession* ses, const f9OmsRc_ClientReport* apiRpt) {
   OmsRcSynClientSession*  synSes = static_cast<OmsRcSynClientSession*>(ses);
   if (apiRpt->ReportSNO_ == 0) {
      if (apiRpt->Layout_ == NULL) // 回補結束. 對方無資料.
         synSes->OnOmsRcSyn_ReportRecoverEnd();
      return;
   }
   assert(synSes->RemoteMap_.get() != nullptr);
   const auto snoMap = synSes->RemoteMap_->MapSNO_.Lock(); // 不會主動 snoMap.unlock(); 所以使用 const;
   synSes->RemoteMap_->LastSNO_ = apiRpt->ReportSNO_;
   if (fon9_UNLIKELY(apiRpt->Layout_ == NULL)) { // 回補結束.
      synSes->OnOmsRcSyn_ReportRecoverEnd();
      return;
   }
   switch (apiRpt->Layout_->LayoutKind_) {
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
   // # apiRpt->ReferenceSNO_
   //   - 由於訂閱回報時, 使用 NoExternal 排除了外部回報,
   //     所以除了新單要求, 其餘回報的 apiRpt->ReferenceSNO_ 必定有效.
   //   - 可是如果是新單從外部來, 刪改的下單要求, 則 apiRpt->ReferenceSNO_ 可能沒有對應的新單回報;
   assert(0 < apiRpt->Layout_->LayoutId_ && apiRpt->Layout_->LayoutId_ <= synSes->RptMap_.size());
   if (!(0 < apiRpt->Layout_->LayoutId_ && apiRpt->Layout_->LayoutId_ <= synSes->RptMap_.size()))
      return;
   const auto& rptdef = synSes->RptMap_[unsigned_cast(apiRpt->Layout_->LayoutId_ - 1)];
   if (rptdef.RptFactory_ == nullptr)
      return;
   // -----
   RemoteReqMap::OmsRequestSP* pSrcReqSP;
   if (fon9_LIKELY(apiRpt->ReportSNO_ < snoMap->size())) {
      pSrcReqSP = &((*snoMap)[apiRpt->ReportSNO_]);
      if (pSrcReqSP->get() != nullptr) {
         // 不處理重複回報.
         // 可能已經有其他地方參照了 (*snoMap)[apiRpt->ReportSNO_],
         // 所以: 不可建立新的 Rpt, 也沒必要重建.
         return;
      }
   }
   else {
      snoMap->resize(apiRpt->ReportSNO_ + 1024 * 1024);
      pSrcReqSP = &((*snoMap)[apiRpt->ReportSNO_]);
   }
   OmsRequestSP rptReq;
   if (apiRpt->ReferenceSNO_ == 0) {
      // 新單 or 無新單的亂序回報(但是,因訂閱回報時使用 NoExternal 所以不會有此情況).
      *pSrcReqSP = rptReq = OnOmsRcSyn_MakeRpt(rptdef, apiRpt);
      assert(rptReq->RxKind() == f9fmkt_RxKind_RequestNew);
      synSes->RemapReqUID(*rptReq, *pSrcReqSP, apiRpt->ReportSNO_);
      return; // 接下來需要等到 ordraw 回報, 才有足夠的資料(req + ordraw), 執行回報處理.
   }
   const OmsRequestBase* ref = (*snoMap)[apiRpt->ReferenceSNO_].get();
   if (ref == nullptr) {
      // 有 apiRpt->ReferenceSNO_, 但沒對應到遠端的 ref;
      // => 遠端沒送來 ref 的回報: 可能因為 ref [對遠端而言] 是從外部來的新單.
      //    => 即使有用 f9OmsRc_RptFilter_IncludeRcSynNew 也有可能發生:
      //       => HostA(Req:X@A) -> HostB(Req:X@A,RefSnoB)
      //          => HostA: 未重啟, 則 HostB.snoMap[RefSnoB] 可取得正確的 Req.
      //          => HostA: 重啟回補時的 HostB.LastSNO > RefSnoB,
      //                    則將永遠失去 HostB.snoMap[RefSnoB] 的參照.
      //    => 後續的回報, 會使用 OrdNo 來尋找原委託.
      // => apiRpt 為此類 ref 後續的刪改要求.
      // => 接下來需等到此筆 apiRpt 的 ordraw 回報, 再執行回報處理.
      *pSrcReqSP = OnOmsRcSyn_MakeRpt(rptdef, apiRpt);
      return;
   }
   if (!ref->IsInCore()) {
      // ref 尚未執行 OmsCore.MoveToCore();
      *pSrcReqSP = ref;
      if (apiRpt->Layout_->LayoutKind_ != f9OmsRc_LayoutKind_ReportOrder) {
         // => apiRpt 為改單, ref 可能為 [尚未觸發的條件單] or [Queuing 的新單].
         // => 此時 apiRpt 因為沒有送出到交易所, 所以 apiRpt 後續的異動(ordraw), 應直接填入此次的 ref;
         return;
      }
      // apiRpt 為 f9OmsRc_LayoutKind_ReportOrder, 則 ref 必定為 OmsRequestBase;
      // 此時 apiRpt 必定為 ref(OmsRequestBase) 的後續異動回報(ordraw).
      rptReq.reset(const_cast<OmsRequestBase*>(ref));
      OnOmsRcSyn_AssignReq(rptdef, apiRpt, *rptReq);
      if (ref->RxKind() == f9fmkt_RxKind_RequestNew) {
         // 新單回報: 必須等到 [有委託書號] 才處理.
         if (OmsIsOrdNoEmpty(ref->OrdNo_)) {
            if (ref->ReportSt() >= f9fmkt_TradingRequestSt_Done) {
               // 拋棄: [沒編委託書號] 的 [新單結束(例:失敗新單)].
               (*snoMap)[apiRpt->ReferenceSNO_].reset();
            }
            return;
         }
         // 如果有需要同步 Queuing, WaitingCond 則對方需要先填入委託書號。
         if (rptReq->ReportSt() == f9fmkt_TradingRequestSt_Queuing)
            rptReq->SetReportSt(f9fmkt_TradingRequestSt_QueuingAtOther);
         else if (rptReq->ReportSt() == f9fmkt_TradingRequestSt_WaitingCond)
            rptReq->SetReportSt(f9fmkt_TradingRequestSt_WaitingCondAtOther);
      }
      else {
         // 刪改回報: 等收到最後結果再處理; 不理會 Sending, Queuing.
         if (ref->ReportSt() <= f9fmkt_TradingRequestSt_LastRunStep) {
            if (rptReq->IsParentRequest()) {
               // 母單的刪改回報: 因為在 Sending 狀態時, 仍有可能更新其他欄位(例: ChildLeavesQty),
               // 所以母單必須繼續往下, 執行回報處理.
            }
            else {
               synSes->OnOmsRcSyn_PendingChgRpt(std::move(rptReq), nullptr, *apiRpt);
               return;
            }
         }
      }
      ref = nullptr;
   }
   else {
      *pSrcReqSP = rptReq = OnOmsRcSyn_MakeRpt(rptdef, apiRpt);
      switch (rptReq->RxKind()) {
      default:
      case f9fmkt_RxKind_Event:
         return;
      case f9fmkt_RxKind_RequestChgCond:
      case f9fmkt_RxKind_RequestDelete:
      case f9fmkt_RxKind_RequestChgQty:
      case f9fmkt_RxKind_RequestChgPri:
      case f9fmkt_RxKind_RequestQuery:
         // 從 ref 複製必要欄位(IvacNo、Symbol...);
         rptReq->OnSynReport(ref, nullptr);
         synSes->RemapReqUID(*rptReq, *pSrcReqSP, apiRpt->ReportSNO_);
         /* fall through */
      case f9fmkt_RxKind_RequestNew:
         // 接下來需要等到 f9fmkt_RxKind_Order(ordraw) 回報, 才知道 rptReq 的結果, 到時才可正確處理回報.
         return;
      case f9fmkt_RxKind_Filled:
         if (ref->RxKind() == f9fmkt_RxKind_Filled) {
            // 此次的 apiRpt 必定為: filled 的 ordraw: apiRpt->Layout_ = "?Ord", apiRpt->ExParam_ = "?Fil?";
            // 母單成交: 需要用 ordraw(for filled) 進行回報處理;
            //          此時 rptReq 為 ParentRpt(rptReq->RxKind == f9fmkt_RxKind_Order) 不會來到此處.
            // 其他成交: 使用 [回報rptReq:TwsFil,TwfFil,TwfFil2] 進行; 所以這裡直接 return; 不用回報.
            return;
         }
         // 來到此處的 rptReq 為 [成交回報:TwsFil,TwfFil,TwfFil2], 且 ref 為 New;
         // 成交回報: 直接到 OmsCore 進行處理, 不用等候 [成交的ordraw];
         // 因為每個 OmsCore 同一筆委託的狀態不一定相同, 成交回報, 只需進行相關成交的運算即可, 不需要遠端的 ordraw;
         break;
      case f9fmkt_RxKind_Unknown:
      case f9fmkt_RxKind_Order:
         if (ref->RxKind() != f9fmkt_RxKind_RequestNew) {
            // ref = 已進入 OmsCore 的刪改, 則 rptReq 須等到流程結束(Sending, Queuing 不處理), 才進行回報.
            if (rptReq->ReportSt() <= f9fmkt_TradingRequestSt_LastRunStep) {
               synSes->OnOmsRcSyn_PendingChgRpt(std::move(rptReq), ref, *apiRpt);
               return;
            }
            // 來到此處: rptReq 必定為底下情況之一:
            // - rptReq 為 遠端刪改Request(ref) 的回報.
            // - rptReq(ParentRpt) 為母單成交回報的 ordraw;
            //   由於 ParentRpt 一般不會包含 Cum* 欄位,
            //   所以 ParentOrder 處理 ParentRpt(filled) 時:
            //    - 先更新 ParentRpt 的欄位.
            //    - 處理: 子單成交(ref) 的回報.
            if (ref->RxKind() == f9fmkt_RxKind_Filled) {
               // ref=母單成交回報, [ses連線的主機] 建立的母單, 會強制送來[母單的(成交)回報], 不論該成交的來源.
               // 所以, ref 可能是重複的回報 => 應該要找到在本地端的 FilledRequest.
               // => 但為了避免 ref 為正常成交回報, 但尚未進入 Core, 所以應在 Core 裡面處理.
               // => 可參考 OmsParentRequestIni::RunReportInCore();
               // -------------------------------------------------------------------------
               // => 所以當 B主機啟用 A主機線路後, B主機收到 A主機的子單成交回報, 會有底下的情況:
               //    => B 更新子單成交; 但不會更新母單;
               //       => B 送出子單成交回報給 A;
               //          => 因為: B收到的成交為 IsForceInternalRpt();
               //    => A 更新子單及母單成交;
               //       => A 送出 [子單及母單] 成交回報給 B;
               //          => 因為: A為該子單及母單的建立者 !IsReportIn();
               //          => 且 B訂閱時有 f9OmsRc_RptFilter_IncludeRcSynNew 旗標;
               //    => B 收到 A的子單成交回報: "OmsFilled: Duplicate MatchKey."
               //    => B 收到 A的母單成交回報: B 更新母單;
               //       => 因為該成交為 B 從交易所收到的, 所以 IsForceInternalRpt();
               //       => 因此 B 會送出 [母單成交回報] 給 A;
               //       => A 會排除重複回報.
               //    => 所以若有第3台主機 C, 則會收到 2 次成交回報:
               //       => 一次來自 B, 因為: ReqFilled.IsForceInternalRpt();
               //       => 一次來自 A, 因為: !ReqInitiator.IsReportIn();
               // -------------------------------------------------------------------------
               assert(rptReq->IsParentRequest());
               // 在底下, 執行 MoveToCore() 之前, 會先執行 rptReq->OnSynReport(ref, message);
               // 此時由 rptReq 保留 ref, 可參考 OmsParentRequestIni::OnSynReport();
               // 然後在 OmsParentRequestIni::RunReportInCore() 可得知 RequestFilled(=此處的ref) 的所在.
               break;
            }
         }
         break;
      }
   }
   StrView message;
   if (apiRpt->Layout_->IdxMessage_ >= 0)
      message = apiRpt->FieldArray_[apiRpt->Layout_->IdxMessage_];
   rptReq->OnSynReport(ref, message);
   rptReq->SetReportNeedsLog();
   rptReq->SetParentReport((*snoMap)[rptReq->GetParentRequestSNO()].get());
   // -----
   assert((rptReq->ReportSt() > f9fmkt_TradingRequestSt_LastRunStep)
          || (rptReq->RxKind() == f9fmkt_RxKind_RequestNew && !OmsIsOrdNoEmpty(rptReq->OrdNo_)));
   synSes->OnOmsRcSyn_RunReport(std::move(rptReq), ref, message);
}
void OmsRcSynClientSession::OnOmsRcSyn_RunReport(OmsRequestSP rptReq, const OmsRequestBase* ref, const StrView& message) {
   (void)ref; (void)message;
   OmsRequestRunner runner;
   runner.Request_ = std::move(rptReq);
   this->OmsCore_->MoveToCore(std::move(runner));
}
void OmsRcSynClientSession::OnOmsRcSyn_PendingChgRpt(OmsRequestSP rptReq, const OmsRequestBase* ref, const f9OmsRc_ClientReport& apiRpt) {
   (void)rptReq; (void)ref; (void)apiRpt;
}
//--------------------------------------------------------------------------//
bool OmsRcSyn_SessionFactory::Creator::OnUnknownTag(PluginsHolder& holder, StrView tag, StrView value) {
   if (tag == "IncludeRcSynNew") {
      if (fon9::toupper(value.Get1st()) == 'Y')
         this->RptFilter_ = static_cast<f9OmsRc_RptFilter>(this->RptFilter_ | f9OmsRc_RptFilter_IncludeRcSynNew);
      else
         this->RptFilter_ = static_cast<f9OmsRc_RptFilter>(this->RptFilter_ & ~f9OmsRc_RptFilter_IncludeRcSynNew);
      return true;
   }
   return base::OnUnknownTag(holder, tag, value);
}
intrusive_ptr<OmsRcSyn_SessionFactory> OmsRcSyn_SessionFactory::Creator::CreateRcSynSessionFactory(OmsCoreMgrSP&& omsCoreMgr) {
   return new OmsRcSyn_SessionFactory(this->Name_, std::move(omsCoreMgr), this->RptFilter_, nullptr, nullptr);
}
SessionFactorySP OmsRcSyn_SessionFactory::Creator::CreateSessionFactory() {
   if (auto omsCoreMgr = this->GetOmsCoreMgr()) {
      if (auto retval = this->CreateRcSynSessionFactory(std::move(omsCoreMgr))) {
         // 不能在建構時呼叫 retval->ReloadLastSNO(this->OmsCoreMgr_->CurrentCore());
         // 因為裡面有 intrusive_ptr<OmsRcSyn_SessionFactory> pthis; 可能會造成建構完成前刪除!
         retval->ReloadLastSNO(retval->OmsCoreMgr_->CurrentCore());
         return retval;
      }
   }
   return nullptr;
}
static bool OmsRcSyn_Start(PluginsHolder& holder, StrView args) {
   return OmsRcSyn_SessionFactory::Creator(holder, "OmsRcSynCli").Parse(args);
}

} // namespaces
//--------------------------------------------------------------------------//
extern "C" fon9::seed::PluginsDesc f9p_OmsRcSyn;
static fon9::seed::PluginsPark f9pRegister{"OmsRcSyn", &f9p_OmsRcSyn};
fon9::seed::PluginsDesc f9p_OmsRcSyn{"", &f9omstw::OmsRcSyn_Start, nullptr, nullptr,};
