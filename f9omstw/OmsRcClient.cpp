// \file f9omstw/OmsRcClient.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRcClient.hpp"
#include "f9omstw/OmsBase.hpp"
#include "f9omstw/OmsErrCode.h"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/Log.hpp"

namespace f9omstw {

OmsRcClientSession::OmsRcClientSession(fon9::rc::RcFunctionMgrSP funcMgr, const f9OmsRc_ClientSessionParams* params)
   : base(std::move(funcMgr), fon9::rc::RcSessionRole::User, static_cast<fon9::rc::RcFlag>(params->RcFlags_))
   , Password_{params->Password_}
   , Handler_(*params->Handler_) {
   this->SetUserId(fon9::StrView_cstr(params->UserId_));
   this->UserData_ = params->UserData_;
   this->LogFlags_ = params->LogFlags_;
}
fon9::StrView OmsRcClientSession::GetAuthPassword() const {
   return fon9::StrView{&this->Password_};
}

static void LogDevSt(fon9::StrView head, OmsRcClientSession* ses, const fon9::io::StateUpdatedArgs& e) {
   if (ses->LogFlags_ & f9OmsRc_ClientLogFlag_Link)
      fon9_LOG_INFO(head, "|ses=", fon9::ToPtr(ses), "|dev=", fon9::ToPtr(ses->GetDevice()),
                    "|st=", fon9::io::GetStateStr(e.State_),
                    "|devid=", e.DeviceId_,
                    "|info=", e.Info_);
   if (ses->Handler_.FnOnLinkEv_)
      ses->Handler_.FnOnLinkEv_(ses, static_cast<f9io_State>(e.State_), e.Info_.ToCStrView());
}
void OmsRcClientSession::OnDevice_StateChanged(fon9::io::Device& dev, const fon9::io::StateChangedArgs& e) {
   LogDevSt("OmsRcClient.OnDeviceSt", this, e.After_);
   base::OnDevice_StateChanged(dev, e);
}
void OmsRcClientSession::OnDevice_StateUpdated(fon9::io::Device& dev, const fon9::io::StateUpdatedArgs& e) {
   LogDevSt("OmsRcClient.OnDeviceStU", this, e);
   base::OnDevice_StateUpdated(dev, e);
}
//--------------------------------------------------------------------------//
OmsRcClientAgent::~OmsRcClientAgent() {
}
void OmsRcClientAgent::OnSessionLinkBroken(fon9::rc::RcSession& ses) {
   if (auto* note = static_cast<OmsRcClientNote*>(ses.GetNote(fon9::rc::RcFunctionCode::OmsApi)))
      note->OnSessionLinkBroken(ses);
}
void OmsRcClientAgent::OnSessionApReady(fon9::rc::RcSession& ses) {
   auto* note = static_cast<OmsRcClientNote*>(ses.GetNote(fon9::rc::RcFunctionCode::OmsApi));
   if (note == nullptr) {
      ses.ResetNote(fon9::rc::RcFunctionCode::OmsApi,
                    fon9::rc::RcFunctionNoteSP{note = new OmsRcClientNote});
   }
   note->OnSessionApReady(ses);
}
//--------------------------------------------------------------------------//
void OmsRcClientNote::OnSessionLinkBroken(fon9::rc::RcSession& ses) {
   (void)ses;
   this->Config_.TDay_.Assign0();
   this->ChangingTDay_.Assign0();
}
void OmsRcClientNote::OnSessionApReady(fon9::rc::RcSession& ses) {
   if (static_cast<OmsRcClientSession*>(&ses)->LogFlags_ & f9OmsRc_ClientLogFlag_Link)
      fon9_LOG_INFO("OmsRcClient.ApReady|ses=", fon9::ToPtr(static_cast<f9OmsRc_ClientSession*>(static_cast<OmsRcClientSession*>(&ses))),
                    "|user=", ses.GetUserId(),
                    "|remoteAp=", ses.GetRemoteParam().ApVer_,
                    "|remoteIp=", ses.GetRemoteIp());
   assert(this->Config_.TDay_.GetOrigValue() == 0 && this->ChangingTDay_.GetOrigValue() == 0);
   fon9::RevBufferList rbuf{128};
   fon9::ToBitv(rbuf, f9OmsRc_OpKind_Config);
   fon9::RevPutBitv(rbuf, fon9_BitvV_Number0); // ReqTableId=0
   ses.Send(fon9::rc::RcFunctionCode::OmsApi, std::move(rbuf));
}
void OmsRcClientNote::OnRecvFunctionCall(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param) {
   unsigned tableId{0};
   fon9::BitvTo(param.RecvBuffer_, tableId);
   if (fon9_UNLIKELY(tableId == 0))
      return this->OnRecvOmsOpResult(ses, param);
   f9OmsRc_ClientReport rpt;
   std::string          rptstr;
   rpt.ReportSNO_ = rpt.ReferenceSNO_ = 0;
   fon9::BitvTo(param.RecvBuffer_, rpt.ReportSNO_);
   fon9::BitvTo(param.RecvBuffer_, rpt.ReferenceSNO_);
   fon9::BitvTo(param.RecvBuffer_, rptstr);
   const OmsRcRptLayout* tab = (tableId > this->RptLayouts_.size() ? nullptr : this->RptLayouts_[tableId - 1].get());

   assert(dynamic_cast<OmsRcClientSession*>(&ses) != nullptr);
   OmsRcClientSession* clises = static_cast<OmsRcClientSession*>(&ses);
   if ((clises->LogFlags_ & f9OmsRc_ClientLogFlag_Report) || rpt.ReportSNO_ == 0)
      fon9_LOG_INFO("OmsRcClient.RxReport|ses=", fon9::ToPtr(static_cast<f9OmsRc_ClientSession*>(clises)),
                    "|tab=", tableId,
                    ':', tab ? fon9::StrView{&tab->Name_} : fon9::StrView{"?"},
                    "|sno=", rpt.ReportSNO_, "|ref=", rpt.ReferenceSNO_, "|rpt=", rptstr);

   if (tab == nullptr)
      return;
   if (!clises->Handler_.FnOnReport_)
      return;
      
   char*          strBeg = &*rptstr.begin();
   char* const    strEnd = strBeg + rptstr.size();
   fon9_CStrView* pval = &*tab->RptValues_.begin();
   rpt.Layout_ = tab;
   rpt.FieldArray_ = pval;
   for (unsigned L = 0; L < rpt.Layout_->FieldCount_; ++L) {
      if (strBeg == strEnd)
         pval->Begin_ = pval->End_ = strEnd;
      else {
         pval->Begin_ = strBeg;
         if ((strBeg = reinterpret_cast<char*>(memchr(strBeg, *fon9_kCSTR_CELLSPL, static_cast<size_t>(strEnd - strBeg)))) == nullptr)
            pval->End_ = strBeg = strEnd;
         else {
            pval->End_ = strBeg;
            *strBeg++ = '\0';
         }
      }
      ++pval;
   }
   clises->Handler_.FnOnReport_(clises, &rpt);
}
void OmsRcClientNote::OnRecvOmsOpResult(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param) {
   f9OmsRc_OpKind opkind{};
   fon9::BitvTo(param.RecvBuffer_, opkind);
   switch (opkind) {
   default:
      ses.ForceLogout("Unknown OmsRcOpKind");
      return;
   case f9OmsRc_OpKind_Config:
      this->OnRecvConfig(ses, param);
      break;
   case f9OmsRc_OpKind_TDayChanged:
      this->OnRecvTDayChanged(ses, param);
      break;
   case f9OmsRc_OpKind_ReportSubscribe:
      fon9::TimeStamp      tday;
      f9OmsRc_ClientReport rpt;
      fon9::ZeroStruct(rpt);
      fon9::BitvTo(param.RecvBuffer_, tday);
      fon9::BitvTo(param.RecvBuffer_, rpt.ReportSNO_);
      OmsRcClientSession* clises = static_cast<OmsRcClientSession*>(&ses);
      if (this->Config_.TDay_ == tday && clises->Handler_.FnOnReport_)
         clises->Handler_.FnOnReport_(clises, &rpt);
      return;
   }
}
//--------------------------------------------------------------------------//
void OmsRcLayout::Initialize() {
   this->LayoutKind_ = f9OmsRc_LayoutKind_Request;
   fon9_CStrViewFrom(this->LayoutName_, this->Name_);

   this->LayoutFieldVector_.resize(this->Fields_.size());
   this->FieldCount_ = static_cast<decltype(this->FieldCount_)>(this->Fields_.size());
   this->FieldArray_ = &*this->LayoutFieldVector_.begin();
   for (unsigned L = this->FieldCount_; L > 0;) {
      f9OmsRc_LayoutField* cfld = &this->LayoutFieldVector_[--L];
      const auto*          rfld = this->Fields_.Get(L);
      fon9_CStrViewFrom(cfld->Name_, rfld->Name_);
      fon9_CStrViewFrom(cfld->Title_, rfld->GetTitle());
      fon9_CStrViewFrom(cfld->Description_, rfld->GetDescription());
      fon9::NumOutBuf nbuf;
      rfld->GetTypeId(nbuf).ToString(cfld->TypeId_);
   }

   /// 將欄位內容依照 reqLayouts 的順序填入下單要求.
   /// - 將下單所需要的欄位 index 取出.
   /// - 將欄位內容(字串)存到 vector 對應的位置.
   /// - 建立下單要求字串.
   #define SET_FIELD_IDX(layout, fldName)           \
      if (auto fld = layout->Fields_.Get(#fldName)) \
         layout->Idx##fldName##_ = static_cast<f9OmsRc_FieldIndexS>(fld->GetIndex()); \
      else                                          \
         layout->Idx##fldName##_ = -1;              \
   //------------------------------------------------
   SET_FIELD_IDX(this, Kind);
   SET_FIELD_IDX(this, Market);
   SET_FIELD_IDX(this, SessionId);
   SET_FIELD_IDX(this, BrkId);
   SET_FIELD_IDX(this, OrdNo);
   SET_FIELD_IDX(this, IvacNo);
   SET_FIELD_IDX(this, SubacNo);
   SET_FIELD_IDX(this, IvacNoFlag);
   SET_FIELD_IDX(this, UsrDef);
   SET_FIELD_IDX(this, ClOrdId);
   SET_FIELD_IDX(this, Side);
   SET_FIELD_IDX(this, OType);
   SET_FIELD_IDX(this, Symbol);
   SET_FIELD_IDX(this, PriType);
   SET_FIELD_IDX(this, Pri);
   SET_FIELD_IDX(this, Qty);
   SET_FIELD_IDX(this, PosEff);
   SET_FIELD_IDX(this, TIF);
   SET_FIELD_IDX(this, Pri2);

   SET_FIELD_IDX(this, ReqUID);
   SET_FIELD_IDX(this, CrTime);
   SET_FIELD_IDX(this, UpdateTime);
   SET_FIELD_IDX(this, OrdSt);
   SET_FIELD_IDX(this, ReqSt);
   SET_FIELD_IDX(this, UserId);
   SET_FIELD_IDX(this, FromIp);
   SET_FIELD_IDX(this, Src);
   SET_FIELD_IDX(this, SesName);
   SET_FIELD_IDX(this, IniQty);
   SET_FIELD_IDX(this, IniOType);
   SET_FIELD_IDX(this, IniTIF);

   SET_FIELD_IDX(this, ChgQty);
   SET_FIELD_IDX(this, ErrCode);
   SET_FIELD_IDX(this, Message);
   SET_FIELD_IDX(this, BeforeQty);
   SET_FIELD_IDX(this, AfterQty);
   SET_FIELD_IDX(this, LeavesQty);
   SET_FIELD_IDX(this, LastExgTime);

   SET_FIELD_IDX(this, LastFilledTime);
   SET_FIELD_IDX(this, CumQty);
   SET_FIELD_IDX(this, CumAmt);
   SET_FIELD_IDX(this, MatchQty);
   SET_FIELD_IDX(this, MatchPri);
   SET_FIELD_IDX(this, MatchKey);
   SET_FIELD_IDX(this, MatchTime);

   SET_FIELD_IDX(this, CumQty1);
   SET_FIELD_IDX(this, CumAmt1);
   SET_FIELD_IDX(this, MatchQty1);
   SET_FIELD_IDX(this, MatchPri1);

   SET_FIELD_IDX(this, CumQty2);
   SET_FIELD_IDX(this, CumAmt2);
   SET_FIELD_IDX(this, MatchQty2);
   SET_FIELD_IDX(this, MatchPri2);
   memset(this->IdxUserFields_, 0xff, sizeof(this->IdxUserFields_));
}
static void ParseReqLayout(fon9::StrView& cfgstr, OmsRcReqLayouts& layouts) {
   fon9::Named        named = fon9::DeserializeNamed(cfgstr, ',', '{');
   fon9::StrView      fldscfg = fon9::SbrFetchNoTrim(cfgstr, '}');
   fon9::seed::Fields fields;
   fon9::seed::MakeFields(fldscfg, ',', '\n', fields);
   layouts.Add(new OmsRcLayout(static_cast<f9OmsRc_TableIndex>(layouts.size() + 1), named, std::move(fields)));
}
static void ParseRptLayout(fon9::StrView& cfgstr, OmsRcRptLayouts& layouts) {
   fon9::StrView      ex;
   fon9::Named        named = fon9::DeserializeNamed(cfgstr, ',', '{', &ex);
   fon9::StrView      fldscfg = fon9::SbrFetchNoTrim(cfgstr, '}');
   fon9::seed::Fields fields;
   fon9::seed::MakeFields(fldscfg, ',', '\n', fields);
   f9OmsRc_LayoutKind layoutKind = f9OmsRc_LayoutKind_ReportRequest;
   if (!ex.empty()) {
      ex.SetBegin(ex.begin() + 1);
      if (ex == "event")
         layoutKind = f9OmsRc_LayoutKind_ReportEvent;
      else if (ex == "abandon")
         layoutKind = f9OmsRc_LayoutKind_ReportRequestAbandon;
      else if (ex == "ini")
         layoutKind = f9OmsRc_LayoutKind_ReportRequestIni;
      else
         layoutKind = f9OmsRc_LayoutKind_ReportOrder;
   }
   layouts.emplace_back(new OmsRcRptLayout(ex, layoutKind, static_cast<f9OmsRc_TableIndex>(layouts.size() + 1),
                                           named, std::move(fields)));
}
static void LogConfig(fon9::LogLevel lv, f9OmsRc_ClientSession* ses, const OmsRcClientConfig& cfg) {
   if (fon9_UNLIKELY(lv < fon9::LogLevel_))
      return;
   fon9::RevBufferList rbuf_{fon9::kLogBlockNodeSize};
   fon9::RevPutChar(rbuf_, '\n');
   if (cfg.LayoutsStr_.empty())
      fon9::RevPrint(rbuf_, "|Layouts=(empty)");
   else
      fon9::RevPrint(rbuf_, "\n" "Layouts=\n", cfg.LayoutsStr_);

   if (cfg.TablesStr_.empty())
      fon9::RevPrint(rbuf_, "|Tables=(empty)");
   else
      fon9::RevPrint(rbuf_, "\n" "Tables=\n", cfg.TablesStr_);

   fon9::RevPrint(rbuf_, "OmsRcClient.OnConfig|ses=", fon9::ToPtr(ses),
                  "|TDay=", cfg.TDay_, fon9::FmtTS("f-t"),
                  "|SeedPath=", cfg.OmsSeedPath_,
                  "|HostId=", cfg.CoreTDay_.HostId_);
   fon9::LogWrite(lv, std::move(rbuf_));
}
void OmsRcClientNote::OnRecvConfig(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param) {
   OmsRcClientConfig cfg;
   fon9::BitvTo(param.RecvBuffer_, cfg.TDay_);
   fon9::BitvTo(param.RecvBuffer_, cfg.OmsSeedPath_);
   fon9::BitvTo(param.RecvBuffer_, cfg.CoreTDay_.HostId_);
   fon9::BitvTo(param.RecvBuffer_, cfg.LayoutsStr_);
   fon9::BitvTo(param.RecvBuffer_, cfg.TablesStr_);

   assert(dynamic_cast<OmsRcClientSession*>(&ses) != nullptr);
   OmsRcClientSession* clises = static_cast<OmsRcClientSession*>(&ses);
   if (clises->LogFlags_ & (f9OmsRc_ClientLogFlag_Config | f9OmsRc_ClientLogFlag_Report | f9OmsRc_ClientLogFlag_Request))
      LogConfig(fon9::LogLevel::Info, clises, cfg);

   if (this->ChangingTDay_ > cfg.TDay_) {
      this->SendTDayConfirm(ses);
      return;
   }
   cfg.CoreTDay_.YYYYMMDD_ = fon9::GetYYYYMMDD(cfg.TDay_);
   cfg.CoreTDay_.UpdatedCount_ = static_cast<uint32_t>(cfg.TDay_.GetIntPart()
                    - fon9::YYYYMMDDHHMMSS_ToTimeStamp(cfg.CoreTDay_.YYYYMMDD_, 0).GetIntPart());

   std::vector<OmsRcLayoutSP> oldReqLayouts;
   OmsRcRptLayouts            oldRptLayouts;
   if (!cfg.LayoutsStr_.empty() &&  this->Config_.LayoutsStr_ != cfg.LayoutsStr_) {
      this->Config_.LayoutsStr_ = std::move(cfg.LayoutsStr_);
      // 在 FnOnConfig_() 通知後才銷毀舊的 layout.
      this->RptLayouts_.swap(oldRptLayouts);
      this->ReqLayouts_.GetAll(oldReqLayouts);
      this->ReqLayouts_.clear();
      fon9::StrView  cfgstr{&this->Config_.LayoutsStr_};
      while (!fon9::StrTrimHead(&cfgstr).empty()) {
         fon9::StrView  kind = fon9::StrFetchTrim(cfgstr, '.');
         if (kind == "REQ")
            ParseReqLayout(cfgstr, this->ReqLayouts_);
         else if (kind == "RPT") {
            ParseRptLayout(cfgstr, this->RptLayouts_);
         }
      }
      this->ReqLayouts_.GetAll(this->RequestLayoutVector_);
   }
   if ((cfg.RequestLayoutCount_ = static_cast<f9OmsRc_TableIndex>(this->RequestLayoutVector_.size())) != 0)
      cfg.RequestLayoutArray_ = &*this->RequestLayoutVector_.begin();
   else
      cfg.RequestLayoutArray_ = nullptr;

   if (!cfg.TablesStr_.empty() && this->Config_.TablesStr_ != cfg.TablesStr_) {
      this->Config_.TablesStr_ = std::move(cfg.TablesStr_);
      this->ConfigGvTablesStr_ = this->Config_.TablesStr_;
      fon9_CStrViewFrom(cfg.TablesStrView_, this->Config_.TablesStr_);
      OmsParseGvTables(this->ConfigGvTables_, this->ConfigGvTablesStr_);
      
      fon9_GCC_WARN_DISABLE("-Wold-style-cast");
      fon9_GCC_WARN_DISABLE_NO_PUSH("-Winvalid-offsetof");
      static_assert(offsetof(OmsGvTable, Name_) == offsetof(f9oms_GvTable, Table_.Name_),
                    "offsetof(OmsGvTable, Name_) != offsetof(f9oms_GvTable, Name_)");
      static_assert(offsetof(OmsGvTable, GvList_) == offsetof(f9oms_GvTable, GvList_),
                    "offsetof(OmsGvTable, GvList_) != offsetof(f9oms_GvTable, GvList_)");
      static_assert(sizeof(const OmsGvTableSP) == sizeof(const f9oms_GvTable*),
                    "OmsGvTables::NamedIxSP cannot be a plain pointer.");
      const OmsGvTableSP* ppTabs = this->ConfigGvTables_.GetVector();
      cfg.TableList_ = (const f9oms_GvTable**)(ppTabs);
      cfg.TableCount_ = static_cast<f9OmsRc_TableIndex>(this->ConfigGvTables_.size());
      fon9_GCC_WARN_POP;

      cfg.OrdTeams_.Begin_ = cfg.OrdTeams_.End_ = nullptr;
      cfg.FcReqCount_ = cfg.FcReqMS_ = 0;
      if (const OmsGvTable* tab = this->ConfigGvTables_.Get("OmsPoUserRights")) {
         if (tab->RecList_.size() > 0) {
            const fon9::StrView* rec = tab->RecList_[0];
            if (const auto* fld = tab->Fields_.Get("OrdTeams"))
               cfg.OrdTeams_ = rec[fld->GetIndex()].ToCStrView();
            if (const auto* fld = tab->Fields_.Get("FcReqCount"))
               cfg.FcReqCount_ = fon9::StrTo(rec[fld->GetIndex()], decltype(cfg.FcReqCount_){0});
            if (const auto* fld = tab->Fields_.Get("FcReqMS"))
               cfg.FcReqMS_ = fon9::StrTo(rec[fld->GetIndex()], decltype(cfg.FcReqMS_){0});
         }
      }
   }

   clises->FcReq_.Resize(cfg.FcReqCount_, fon9::TimeInterval_Millisecond(cfg.FcReqMS_));
   this->Config_.MoveFromRc(std::move(cfg));
   if (clises->Handler_.FnOnConfig_)
      clises->Handler_.FnOnConfig_(clises, &this->Config_);
}
void OmsRcClientNote::OnRecvTDayChanged(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param) {
   fon9::BitvTo(param.RecvBuffer_, this->ChangingTDay_);
   if (static_cast<OmsRcClientSession*>(&ses)->LogFlags_ & f9OmsRc_ClientLogFlag_Config)
      fon9_LOG_INFO("OmsRcClient.Recv|ses=", fon9::ToPtr(static_cast<f9OmsRc_ClientSession*>(static_cast<OmsRcClientSession*>(&ses))),
                    "TDayChanged=", this->ChangingTDay_, fon9::FmtTS("f-t"));
   if (this->Config_.TDay_.GetOrigValue() == 0) {
      // 尚未收到 Config, 就先收到 TDayChanged, 等收到 Config 時, 再回 TDayConfirm;
   }
   else if (this->ChangingTDay_> this->Config_.TDay_) {
      // 已收到 Config, 直接回 TDayConfirm, 等候新的 Config;
      this->SendTDayConfirm(ses);
   }
}
void OmsRcClientNote::SendTDayConfirm(fon9::rc::RcSession& ses) {
   if (static_cast<OmsRcClientSession*>(&ses)->LogFlags_ & f9OmsRc_ClientLogFlag_Config)
      fon9_LOG_INFO("OmsRcClient.Send|ses=", fon9::ToPtr(static_cast<f9OmsRc_ClientSession*>(static_cast<OmsRcClientSession*>(&ses))),
                    "TDayConfirm=", this->ChangingTDay_, fon9::FmtTS("f-t"));
   fon9::RevBufferList rbuf{128};
   fon9::ToBitv(rbuf, this->ChangingTDay_);
   fon9::ToBitv(rbuf, f9OmsRc_OpKind_TDayConfirm);
   fon9::RevPutBitv(rbuf, fon9_BitvV_Number0); // ReqTableId=0
   ses.Send(fon9::rc::RcFunctionCode::OmsApi, std::move(rbuf));
}

} // namespaces
