// \file f9omstw/OmsRcClient.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsErrCode.h"
#include "f9omstw/OmsBase.hpp"
#include "f9omstw/OmsRcClient.hpp"
#include "fon9/Log.hpp"
#include "fon9/seed/FieldMaker.hpp"

extern "C" {
   void f9OmsRc_SubscribeReport(f9OmsRc_ClientSession* ses, const f9OmsRc_ClientConfig* cfg, f9OmsRc_SNO from, f9OmsRc_RptFilter filter) {
      f9omstw::OmsRcClientSession* cli = static_cast<f9omstw::OmsRcClientSession*>(ses);
      fon9::RevBufferList rbuf{64};
      fon9::ToBitv(rbuf, filter);
      fon9::ToBitv(rbuf, from);
      fon9::ToBitv(rbuf, fon9::unsigned_cast(fon9::YYYYMMDDHHMMSS_ToTimeStamp(cfg->YYYYMMDD_, 0).GetIntPart() + cfg->UpdatedCount_));
      fon9::ToBitv(rbuf, f9OmsRc_OpKind_ReportSubscribe);
      fon9::RevPutBitv(rbuf, fon9_BitvV_Number0);
      cli->Send(fon9::rc::RcFunctionCode::OmsApi, std::move(rbuf));
   }
} // extern "C"

//--------------------------------------------------------------------------//

namespace f9omstw {

OmsRcClientSession::OmsRcClientSession(fon9::rc::RcFunctionMgrSP funcMgr, fon9::StrView userid, std::string passwd,
                                       const f9OmsRc_ClientHandler& handler, const f9OmsRc_ClientSession& cfg)
   : base(std::move(funcMgr), fon9::rc::RcSessionRole::User)
   , Password_{std::move(passwd)}
   , Handler_(handler) {
   this->SetUserId(userid);
   *static_cast<f9OmsRc_ClientSession*>(this) = cfg;
}
fon9::StrView OmsRcClientSession::GetAuthPassword() const {
   return fon9::StrView{&this->Password_};
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
   if (clises->LogFlags_ & f9OmsRc_ClientLogFlag_Report)
      fon9_LOG_INFO("OmsRcClient:RxReport|tab=", tableId,
                    ':', tab ? fon9::StrView{&tab->Name_} : fon9::StrView{"?"},
                    "|sno=", rpt.ReportSNO_, "|ref=", rpt.ReferenceSNO_, "|rpt=", rptstr);

   if (tab == nullptr)
      return;
   if (!clises->Handler_.FnOnReport_)
      return;
      
   rpt.Layout_ = tab;
   char*            strBeg = &*rptstr.begin();
   char* const      strEnd = strBeg + rptstr.size();
   f9OmsRc_StrView* pval = &*tab->RptValues_.begin();
   rpt.FieldArray_ = pval;
   rpt.FieldCount_ = static_cast<unsigned>(tab->Fields_.size());
   for (unsigned L = 0; L < rpt.FieldCount_; ++L) {
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
      fon9::TimeStamp tday;
      OmsRxSNO        publishedSNO{};
      fon9::BitvTo(param.RecvBuffer_, tday);
      fon9::BitvTo(param.RecvBuffer_, publishedSNO);
      return;
   }
}
//--------------------------------------------------------------------------//
void OmsRcLayout::Initialize() {
   /// 將欄位內容依照 reqLayouts 的順序填入下單要求.
   /// - 將下單所需要的欄位 index 取出.
   /// - 將欄位內容(字串)存到 vector 對應的位置.
   /// - 建立下單要求字串.
   #define SET_FIELD_IDX(layout, fldName)           \
      if (auto fld = layout->Fields_.Get(#fldName)) \
         layout->fldName##_ = fld->GetIndex();      \
      else                                          \
         layout->fldName##_ = -1;                   \
   //------------------------------------------------
   SET_FIELD_IDX(this, Kind);
   SET_FIELD_IDX(this, Market);
   SET_FIELD_IDX(this, SessionId);
   SET_FIELD_IDX(this, BrkId);
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
   SET_FIELD_IDX(this, Pri2);
   SET_FIELD_IDX(this, Qty);
   SET_FIELD_IDX(this, OrdNo);
   SET_FIELD_IDX(this, PosEff);
   SET_FIELD_IDX(this, TIF);
   SET_FIELD_IDX(this, IniSNO);

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
   SET_FIELD_IDX(this, MatchKey);
   SET_FIELD_IDX(this, MatchTime);
   SET_FIELD_IDX(this, MatchQty);
   SET_FIELD_IDX(this, MatchPri);
   SET_FIELD_IDX(this, CumQty2);
   SET_FIELD_IDX(this, CumAmt2);
   SET_FIELD_IDX(this, MatchQty2);
   SET_FIELD_IDX(this, MatchPri2);
}
static void ParseReqLayout(fon9::StrView& cfgstr, OmsRcReqLayouts& layouts) {
   fon9::Named        named = fon9::DeserializeNamed(cfgstr, ',', '{');
   fon9::StrView      fldscfg = fon9::SbrFetchNoTrim(cfgstr, '}');
   fon9::seed::Fields fields;
   fon9::seed::MakeFields(fldscfg, ',', '\n', fields);
   layouts.Add(new OmsRcLayout(named, std::move(fields)));
}
static void ParseRptLayout(fon9::StrView& cfgstr, OmsRcRptLayouts& layouts) {
   fon9::StrView      ex;
   fon9::Named        named = fon9::DeserializeNamed(cfgstr, ',', '{', &ex);
   fon9::StrView      fldscfg = fon9::SbrFetchNoTrim(cfgstr, '}');
   fon9::seed::Fields fields;
   fon9::seed::MakeFields(fldscfg, ',', '\n', fields);
   f9OmsRc_RptKind rptKind = f9OmsRc_RptKind_Request;
   if (!ex.empty()) {
      ex.SetBegin(ex.begin() + 1);
      if (ex == "event")
         rptKind = f9OmsRc_RptKind_Event;
      else if (ex == "abandon")
         rptKind = f9OmsRc_RptKind_RequestAbandon;
      else if (ex == "ini")
         rptKind = f9OmsRc_RptKind_RequestIni;
      else
         rptKind = f9OmsRc_RptKind_Order;
   }
   layouts.emplace_back(new OmsRcRptLayout(ex, rptKind, named, std::move(fields)));
}
static void LogConfig(fon9::LogLevel lv, const OmsRcClientConfig& cfg) {
   if (fon9_UNLIKELY(lv >= fon9::LogLevel_)) {
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

      fon9::RevPrint(rbuf_, "OmsRcClient.Recv.Config|TDay=", cfg.TDay_, fon9::FmtTS("f-t"),
                     "|SeedPath=", cfg.OmsSeedPath_,
                     "|HostId=", cfg.HostId_);
      fon9::LogWrite(lv, std::move(rbuf_));
   }
}
void OmsRcClientNote::OnRecvConfig(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param) {
   OmsRcClientConfig cfg;
   fon9::BitvTo(param.RecvBuffer_, cfg.TDay_);
   fon9::BitvTo(param.RecvBuffer_, cfg.OmsSeedPath_);
   fon9::BitvTo(param.RecvBuffer_, cfg.HostId_);
   fon9::BitvTo(param.RecvBuffer_, cfg.LayoutsStr_);
   fon9::BitvTo(param.RecvBuffer_, cfg.TablesStr_);

   assert(dynamic_cast<OmsRcClientSession*>(&ses) != nullptr);
   OmsRcClientSession* clises = static_cast<OmsRcClientSession*>(&ses);
   if (clises->LogFlags_ & f9OmsRc_ClientLogFlag_Config)
      LogConfig(fon9::LogLevel::Info, cfg);

   if (this->ChangingTDay_ > cfg.TDay_) {
      this->SendTDayConfirm(ses);
      return;
   }

   if (cfg.LayoutsStr_.empty())
      cfg.LayoutsStr_ = std::move(this->Config_.LayoutsStr_);
   else if (this->Config_.LayoutsStr_ != cfg.LayoutsStr_) {
      this->ReqLayouts_.clear();
      this->RptLayouts_.clear();
      fon9::StrView  cfgstr{&cfg.LayoutsStr_};
      while (!fon9::StrTrimHead(&cfgstr).empty()) {
         fon9::StrView  kind = fon9::StrFetchTrim(cfgstr, '.');
         if (kind == "REQ")
            ParseReqLayout(cfgstr, this->ReqLayouts_);
         else if (kind == "RPT") {
            ParseRptLayout(cfgstr, this->RptLayouts_);
         }
      }
   }
   if (cfg.TablesStr_.empty())
      cfg.TablesStr_ = std::move(this->Config_.TablesStr_);
   this->Config_ = std::move(cfg);
   this->Config_.YYYYMMDD_ = fon9::GetYYYYMMDD(this->Config_.TDay_);
   this->Config_.UpdatedCount_ = static_cast<uint32_t>(this->Config_.TDay_.GetIntPart()
            - fon9::YYYYMMDDHHMMSS_ToTimeStamp(this->Config_.YYYYMMDD_, 0).GetIntPart());
   if (clises->Handler_.FnOnConfig_)
      clises->Handler_.FnOnConfig_(clises, &this->Config_);
}
void OmsRcClientNote::OnRecvTDayChanged(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param) {
   fon9::BitvTo(param.RecvBuffer_, this->ChangingTDay_);
   if (static_cast<OmsRcClientSession*>(&ses)->LogFlags_ & f9OmsRc_ClientLogFlag_Config)
      fon9_LOG_INFO("OmsRcClient.Recv|TDayChanged=", this->ChangingTDay_, fon9::FmtTS("f-t"));
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
      fon9_LOG_INFO("OmsRcClient.Send|TDayConfirm=", this->ChangingTDay_, fon9::FmtTS("f-t"));
   fon9::RevBufferList rbuf{128};
   fon9::ToBitv(rbuf, this->ChangingTDay_);
   fon9::ToBitv(rbuf, f9OmsRc_OpKind_TDayConfirm);
   fon9::RevPutBitv(rbuf, fon9_BitvV_Number0); // ReqTableId=0
   ses.Send(fon9::rc::RcFunctionCode::OmsApi, std::move(rbuf));
}

} // namespaces
