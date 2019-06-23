// \file f9omstw/OmsRcClient.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsErrCode.h"
#include "f9omstw/OmsRcClient.hpp"
#include "fon9/Log.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

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
   note->OnSessionLinkReady(ses);
}
//--------------------------------------------------------------------------//
void OmsRcClientNote::OnSessionLinkBroken(fon9::rc::RcSession& ses) {
   (void)ses;
   this->Config_.TDay_.Assign0();
   this->ChangingTDay_.Assign0();
}
void OmsRcClientNote::OnSessionLinkReady(fon9::rc::RcSession& ses) {
   assert(this->Config_.TDay_.GetOrigValue() == 0 && this->ChangingTDay_.GetOrigValue() == 0);
   fon9::RevBufferList rbuf{128};
   fon9::ToBitv(rbuf, f9_OmsRcOpKind_Config);
   fon9::RevPutBitv(rbuf, fon9_BitvV_Number0); // ReqTableId=0
   ses.Send(fon9::rc::RcFunctionCode::OmsApi, std::move(rbuf));
}
void OmsRcClientNote::OnRecvFunctionCall(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param) {
   unsigned tableId{~0u};
   fon9::BitvTo(param.RecvBuffer_, tableId);
   if (fon9_UNLIKELY(tableId == 0))
      return this->OnRecvOmsOpResult(ses, param);
   if (tableId == ~0u) {
      OmsErrCode  errc{};
      std::string errmsg;
      std::string reqstr;
      fon9::BitvTo(param.RecvBuffer_, tableId);
      fon9::BitvTo(param.RecvBuffer_, errc);
      fon9::BitvTo(param.RecvBuffer_, errmsg);
      fon9::BitvTo(param.RecvBuffer_, reqstr);
      if (0);// TODO: request abandon(reject) event.
      fon9_LOG_WARN("OmsRcClient:RxRequestErr|err=", errc, ':', errmsg, "|reqstr=", reqstr);
   }
   else { // RPT.

   }
}
void OmsRcClientNote::OnRecvOmsOpResult(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param) {
   f9_OmsRcOpKind opkind{};
   fon9::BitvTo(param.RecvBuffer_, opkind);
   switch (opkind) {
   case f9_OmsRcOpKind_Config:
      this->OnRecvConfig(ses, param);
      break;
   case f9_OmsRcOpKind_TDayChanged:
      this->OnRecvTDayChanged(ses, param);
      break;
   case f9_OmsRcOpKind_ReportRecover:
   case f9_OmsRcOpKind_ReportSubscribe:
      if (0);// ReportRecover/ReportSubscribe;
      return;
   default:
      ses.ForceLogout("Unknown OmsRcOpKind");
      return;
   }
}

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
}
void OmsRcClientNote::ParseLayout(fon9::StrView& cfgstr, OmsRcLayouts& layouts) {
   fon9::Named named = fon9::DeserializeNamed(cfgstr, ',', '{');
   if (*(cfgstr.begin() - 1) != '{')
      return;
   cfgstr.SetBegin(cfgstr.begin() - 1);
   fon9::StrView      fldscfg = fon9::SbrFetchInsideNoTrim(cfgstr);
   fon9::seed::Fields fields;
   fon9::seed::MakeFields(fldscfg, ',', '\n', fields);
   layouts.Add(new OmsRcLayout(named, std::move(fields)));
}
void OmsRcClientNote::OnRecvConfig(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param) {
   OmsRcClientConfig cfg;
   fon9::BitvTo(param.RecvBuffer_, cfg.TDay_);
   fon9::BitvTo(param.RecvBuffer_, cfg.OmsSeedPath_);
   fon9::BitvTo(param.RecvBuffer_, cfg.HostId_);
   fon9::BitvTo(param.RecvBuffer_, cfg.LayoutsStr_);
   fon9::BitvTo(param.RecvBuffer_, cfg.TablesStr_);

   //fon9_LOG_INFO("OmsCli.Recv:Config:TDay = ", cfg.TDay_, fon9::FmtTS("f-T"), ""
   //              "\n" "SeedPath = [", cfg.OmsSeedPath_, "]"
   //              "\n" "HostId   = ",  cfg.HostId_,
   //              "\n" "Layouts  = [", cfg.LayoutsStr_, "]"
   //              "\n" "Tables   = [", cfg.TablesStr_, "]"
   //);

   if (this->ChangingTDay_ > cfg.TDay_) {
      this->SendTDayConfirm(ses);
      return;
   }
   if (this->Config_.LayoutsStr_ != cfg.LayoutsStr_) {
      this->ReqLayouts_.clear();
      this->RptLayouts_.clear();
      fon9::StrView  cfgstr{&cfg.LayoutsStr_};
      while (!fon9::StrTrimHead(&cfgstr).empty()) {
         fon9::StrView  kind = fon9::StrFetchTrim(cfgstr, '.');
         if (kind == "REQ")
            this->ParseLayout(cfgstr, this->ReqLayouts_);
         else if (kind == "RPT")
            this->ParseLayout(cfgstr, this->RptLayouts_);
      }
   }
   this->Config_ = std::move(cfg);
   if (0);// TODO: TDay changed event.
}
void OmsRcClientNote::OnRecvTDayChanged(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param) {
   fon9::BitvTo(param.RecvBuffer_, this->ChangingTDay_);
   //fon9_LOG_INFO("OmsCli.Recv:TDayChanged = ", this->ChangingTDay_, fon9::FmtTS("f-T"));
   if (this->Config_.TDay_.GetOrigValue() == 0) {
      // 尚未收到 Config, 就先收到 TDayChanged, 等收到 Config 時, 再回 TDayConfirm;
   }
   else if (this->ChangingTDay_> this->Config_.TDay_) {
      // 已收到 Config, 直接回 TDayConfirm, 等候新的 Config;
      this->SendTDayConfirm(ses);
   }
}
void OmsRcClientNote::SendTDayConfirm(fon9::rc::RcSession& ses) {
   //fon9_LOG_INFO("OmsCli.Send:TDayConfirm = ", this->ChangingTDay_, fon9::FmtTS("f-T"));
   fon9::RevBufferList rbuf{128};
   fon9::ToBitv(rbuf, this->ChangingTDay_);
   fon9::ToBitv(rbuf, f9_OmsRcOpKind_TDayConfirm);
   fon9::RevPutBitv(rbuf, fon9_BitvV_Number0); // ReqTableId=0
   ses.Send(fon9::rc::RcFunctionCode::OmsApi, std::move(rbuf));
}

} // namespaces
