// \file f9omsrc/OmsRcServer.cpp
// \author fonwinz@gmail.com
#include "f9omsrc/OmsRcServer.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "fon9/ConfigLoader.hpp"
#include "fon9/RevPrint.hpp"
#include "fon9/seed/FieldInt.hpp"

namespace f9omstw {

FnPutApiField FnPutApiField_Register::Register(fon9::StrView fnName, FnPutApiField fnPutApiField) {
   return SimpleFactoryRegister(fnName, fnPutApiField);
}
void FnPutApiField_FixedValue(const ApiReqFieldCfg& cfg, const ApiReqFieldArg& arg) {
   if (cfg.Field_->IsNull(arg))
      cfg.Field_->StrToCell(arg, ToStrView(cfg.ExtParam_));
}

//--------------------------------------------------------------------------//

FnRptApiField FnRptApiField_Register::Register(fon9::StrView fnName, FnRptApiField fnRptApiField) {
   return SimpleFactoryRegister(fnName, fnRptApiField);
}
static void FnRptApiField_FixedValue(fon9::RevBuffer& rbuf, const ApiRptFieldCfg& cfg, const ApiRptFieldArg* arg) {
   if (fon9_LIKELY(arg))
      fon9::RevPrint(rbuf, cfg.ExtParam_);
   else { // TypeId: 固定長度字串.
      if (size_t sz = cfg.ExtParam_.size())
         fon9::RevPrint(rbuf, 'C', sz);
      else
         fon9::RevPrint(rbuf, "C1");
   }
}
static void FnRptApiField_AbandonErrCode(fon9::RevBuffer& rbuf, const ApiRptFieldCfg&, const ApiRptFieldArg* arg) {
   if (fon9_LIKELY(arg)) {
      if (const OmsRequestBase* req = static_cast<const OmsRequestBase*>(arg->Item_.CastToRequest()))
         fon9::RevPrint(rbuf, req->ErrCode());
   }
   else
      fon9::RevPrint(rbuf, fon9::seed::GetFieldIntTypeId<OmsErrCode>());
}
static void FnRptApiField_AbandonReason(fon9::RevBuffer& rbuf, const ApiRptFieldCfg&, const ApiRptFieldArg* arg) {
   if (fon9_LIKELY(arg)) {
      if (const OmsRequestBase* req = static_cast<const OmsRequestBase*>(arg->Item_.CastToRequest()))
         if (const auto* res = req->AbandonReason())
            fon9::RevPrint(rbuf, *res);
   }
   else
      fon9::RevPrint(rbuf, "C0");
}
static void FnRptApiField_IniField(fon9::RevBuffer& rbuf, const ApiRptFieldCfg& cfg, const ApiRptFieldArg* arg) {
   if (fon9_LIKELY(arg)) {
      if (const OmsOrderRaw* ordraw = static_cast<const OmsOrderRaw*>(arg->Item_.CastToOrder())) {
         if (const OmsRequestIni* ini = ordraw->Order().Initiator())
            if (const fon9::seed::Field* fld = ini->Creator().Fields_.Get(ToStrView(cfg.ExtParam_)))
               fld->CellRevPrint(fon9::seed::SimpleRawRd{*ini}, nullptr, rbuf);
      }
   }
   else
      fon9::RevPrint(rbuf, "C0");
}
static FnRptApiField_Register regFnRptApiField_Abandon{
   "IniField",       &FnRptApiField_IniField,
   "AbandonErrCode", &FnRptApiField_AbandonErrCode,
   "AbandonReason",  &FnRptApiField_AbandonReason
};

//--------------------------------------------------------------------------//

struct ApiNamed {
   fon9::StrView  SysName_;
   fon9::StrView  ApiName_;
   std::string    Title_;
   std::string    Description_;

   void MoveTo(fon9::Named& named) {
      if (!this->Title_.empty())
         named.SetTitle(std::move(this->Title_));
      if (!this->Description_.empty())
         named.SetDescription(std::move(this->Description_));
   }
   fon9::Named ToNamed(const fon9::Named* sysRef) {
      return fon9::Named(this->ApiName_.ToString(),
         (this->Title_.empty() && sysRef) ? sysRef->GetTitle() : std::move(this->Title_),
         (this->Description_.empty() && sysRef) ? sysRef->GetDescription() : std::move(this->Description_));
   }
};

/// [ApiName/]SysName[=ex][,[Title][,Description]]
/// \retval nullptr  success
/// \retval !nullptr error position.
static const char* DeserializeApiNamed(ApiNamed& dst, fon9::StrView& cfgstr, char chTail, fon9::StrView* ex) {
   fon9::StrView  descr = fon9::SbrFetchNoTrim(fon9::StrTrimHead(&cfgstr), chTail);
   fon9::StrView  name = fon9::SbrFetchNoTrim(descr, ',');
   fon9::StrView  title = fon9::SbrFetchNoTrim(descr, ',');

   StrTrimTail(&name);
   if (const char* pInvalid = fon9::FindInvalidNameChar(name)) {
      dst.ApiName_.Reset(name.begin(), pInvalid);
      switch (StrTrimHead(&name, pInvalid).Get1st()) {
      case EOF:
         dst.SysName_ = dst.ApiName_;
         break;
      case '/':
         StrTrimHead(&name, name.begin() + 1);
         if ((pInvalid = fon9::FindInvalidNameChar(name)) == nullptr) {
            dst.SysName_ = name;
            break;
         }
         dst.SysName_.Reset(name.begin(), pInvalid);
         switch (StrTrimHead(&name, pInvalid).Get1st()) {
         case EOF:
            break;
         default:
            return name.begin();
         case '=': case ':':
            goto __CHECK_EX;
         }
         break;
      case '=': case ':':
         dst.SysName_ = dst.ApiName_;
      __CHECK_EX:
         if (ex == nullptr)
            return name.begin();
         StrTrimHead(&name, name.begin() + 1);
         *ex = name;
         break;
      }
   }
   else {
      dst.ApiName_ = dst.SysName_ = name;
      if (ex)
         ex->Reset(nullptr);
   }
   dst.Title_ = StrView_ToNormalizeStr(StrTrimRemoveQuotes(title));
   dst.Description_ = StrView_ToNormalizeStr(StrTrimRemoveQuotes(descr));
   return nullptr;
}
static void DeserializeApiNamed(ApiNamed& dst, fon9::StrView& cfgstr, char chTail, fon9::StrView* ex,
                                const fon9::ConfigLoader& cfgldr, const char* throwErrMsg) {
   if (const char* perr = DeserializeApiNamed(dst, cfgstr, chTail, ex)) {
      fon9::Raise<fon9::ConfigLoader::Err>(throwErrMsg,
                                           cfgldr.GetLineFrom(perr),
                                           std::errc::bad_message);
   }
}

//--------------------------------------------------------------------------//

static ApiReqFieldCfg* SetReqApiField(const fon9::ConfigLoader& cfgldr, OmsRequestFactory& fac, ApiReqCfg::ApiReqFields& flds, ApiNamed& apiNamed) {
   assert(!apiNamed.ApiName_.empty());
   const auto* fld = fac.Fields_.Get(apiNamed.SysName_);
   if (fld == nullptr)
      return nullptr;
   for (auto& fldcfg : flds) {
      if (apiNamed.ApiName_ == &fldcfg.ApiNamed_.Name_) {
         if (apiNamed.SysName_ != &fldcfg.Field_->Name_) {
            fon9::Raise<fon9::ConfigLoader::Err>("REQ field dup defined, but SysName not match.",
                                                 cfgldr.GetLineFrom(apiNamed.SysName_.begin()),
                                                 std::errc::bad_message);
         }
         apiNamed.MoveTo(fldcfg.ApiNamed_);
         return &fldcfg;
      }
   }
   flds.emplace_back(fld, apiNamed.ToNamed(fld));
   return &flds.back();
}
static ApiReqFieldCfg* SetReqSysField(OmsRequestFactory& fac, ApiReqCfg::ApiReqFields& flds, fon9::StrView sysName) {
   const auto* fld = fac.Fields_.Get(sysName);
   if (fld == nullptr)
      return nullptr;
   for (auto& fldcfg : flds) {
      if (fldcfg.Field_ == fld)
         return &fldcfg;
   }
   flds.emplace_back(fld);
   return &flds.back();
}
static void ParseReqCfgs(const fon9::ConfigLoader& cfgldr, ApiSesCfg& cfgs, fon9::StrView& cfgstr) {
   const char* const pcfgbeg = fon9::StrTrimHead(&cfgstr).begin();
   ApiNamed          apiNamed;
   DeserializeApiNamed(apiNamed, cfgstr, '{', nullptr, cfgldr, "REQ named define error.");

   ApiReqCfg* pcfg = nullptr;
   for (auto& cfg : cfgs.ApiReqCfgs_) {
      if (apiNamed.ApiName_ == &cfg.ApiNamed_.Name_) {
         if (apiNamed.SysName_ != &cfg.Factory_->Name_) {
            fon9::Raise<fon9::ConfigLoader::Err>("REQ dup defined, but SysName not match.",
                                                 cfgldr.GetLineFrom(pcfgbeg),
                                                 std::errc::bad_message);
         }
         apiNamed.MoveTo(cfg.ApiNamed_);
         pcfg = &cfg;
      }
   }
   if (pcfg == nullptr) {
      auto* fac = cfgs.CoreMgr_->RequestFactoryPark().GetFactory(apiNamed.SysName_);
      if (fac == nullptr) {
         fon9::Raise<fon9::ConfigLoader::Err>("REQ factory not found.",
                                              cfgldr.GetLineFrom(pcfgbeg),
                                              std::errc::bad_message);
      }
      cfgs.ApiReqCfgs_.emplace_back(*fac, apiNamed.ToNamed(fac));
      pcfg = &cfgs.ApiReqCfgs_.back();
   }
   fon9::StrView ex;
   fon9::StrView rxcfg = fon9::SbrFetchNoTrim(cfgstr, '}');
   while (!fon9::StrTrimHead(&rxcfg).empty()) {
      DeserializeApiNamed(apiNamed, rxcfg, '\n', &ex, cfgldr, "REQ field named define error.");
      ApiReqFieldCfg* fldcfg = (apiNamed.ApiName_.empty()
                             ? SetReqSysField(*pcfg->Factory_, pcfg->SysFields_, apiNamed.SysName_)
                             : SetReqApiField(cfgldr, *pcfg->Factory_, pcfg->ApiFields_, apiNamed));
      if (fldcfg == nullptr) {
         fon9::Raise<fon9::ConfigLoader::Err>("REQ field not found.",
                                              cfgldr.GetLineFrom(apiNamed.SysName_.begin()),
                                              std::errc::bad_message);
      }
      if (ex.empty())
         continue;
      if (*ex.begin() != '%') {
         fldcfg->ExtParam_.assign(ex);
         fldcfg->FnPut_ = &FnPutApiField_FixedValue;
      }
      else { // %fnName:ExParam
         fon9::StrTrimHead(&ex, ex.begin() + 1);
         fon9::StrView fnName = fon9::StrFetchTrim(ex, ':');
         fldcfg->FnPut_ = FnPutApiField_Register::Register(fnName, nullptr);
         if (fldcfg->FnPut_ == nullptr) {
            fon9::Raise<fon9::ConfigLoader::Err>("REQ %function not found.",
                                                 cfgldr.GetLineFrom(fnName.begin()),
                                                 std::errc::bad_message);
         }
         fldcfg->ExtParam_.assign(fon9::StrTrimHead(&ex));
      }
   }
}

//--------------------------------------------------------------------------//

struct ApiRptCfgs::Parser : public std::vector<fon9::DyObj<ApiRptCfg>> {
   fon9_NON_COPY_NON_MOVE(Parser);
   using base = std::vector<fon9::DyObj<ApiRptCfg>>;
   OmsCoreMgr&    CoreMgr_;
   FactoryToIndex ToIndex_;
   char           padding____[4];

   Parser(OmsCoreMgr& coreMgr)
      : CoreMgr_(coreMgr)
      , ToIndex_{coreMgr} {
      this->resize(this->ToIndex_.EventTableIdOffset_ + coreMgr.EventFactoryPark().GetTabCount());
   }

   void Parse(const fon9::ConfigLoader& cfgldr, fon9::StrView& cfgstr);
   void MoveTo(fon9::RevBuffer& rbuf, ApiRptCfgs& cfgs);
};
void ApiRptCfgs::Parser::Parse(const fon9::ConfigLoader& cfgldr, fon9::StrView& cfgstr) {
   const char* const pcfgbeg = fon9::StrTrimHead(&cfgstr).begin();
   ApiNamed          apiNamed;
   fon9::StrView     ex;
   DeserializeApiNamed(apiNamed, cfgstr, '{', &ex, cfgldr, "RPT named define error.");

   unsigned         idx;
   fon9::seed::Tab* fac = this->CoreMgr_.RequestFactoryPark().GetFactory(apiNamed.SysName_);
   if (fac != nullptr) {
      if (ex == "abandon")
         idx = this->ToIndex_.RequestIndex(*static_cast<OmsRequestFactory*>(fac), RequestE_Abandon);
      else if (ex.empty())
         idx = this->ToIndex_.RequestIndex(*static_cast<OmsRequestFactory*>(fac), RequestE_Normal);
      else
         fon9::Raise<fon9::ConfigLoader::Err>("RPT unknown request GR config.",
                                              cfgldr.GetLineFrom(ex.begin()),
                                              std::errc::bad_message);
   }
   else {
      if ((fac = this->CoreMgr_.OrderFactoryPark().GetFactory(apiNamed.SysName_)) != nullptr) {
         auto* reqfac = this->CoreMgr_.RequestFactoryPark().GetFactory(ex);
         if (reqfac == nullptr)
            fon9::Raise<fon9::ConfigLoader::Err>("RPT unknown order GR config.",
                                                 cfgldr.GetLineFrom(ex.begin()),
                                                 std::errc::bad_message);
         idx = this->ToIndex_.OrderIndex(*static_cast<OmsOrderFactory*>(fac), *reqfac);
      }
      else if ((fac = this->CoreMgr_.EventFactoryPark().GetFactory(apiNamed.SysName_)) != nullptr) {
         if (!ex.empty()) {
            fon9::Raise<fon9::ConfigLoader::Err>("RPT unknown event GR config.",
                                                 cfgldr.GetLineFrom(ex.begin()),
                                                 std::errc::bad_message);
         }
         idx = this->ToIndex_.EventIndex(*static_cast<OmsEventFactory*>(fac));
      }
      else
         fon9::Raise<fon9::ConfigLoader::Err>("RPT factory not found.",
                                              cfgldr.GetLineFrom(pcfgbeg),
                                              std::errc::bad_message);
   }

   assert(idx < this->size());
   auto&      dcfg = (*this)[idx];
   ApiRptCfg* pcfg = dcfg.get();
   if (pcfg == nullptr)
      pcfg = dcfg.emplace(*fac, apiNamed.ToNamed(fac));
   else {
      if (apiNamed.ApiName_ != &pcfg->ApiNamed_.Name_)
         fon9::Raise<fon9::ConfigLoader::Err>("RPT dup defined, but ApiName not match.",
                                              cfgldr.GetLineFrom(apiNamed.ApiName_.begin()),
                                              std::errc::bad_message);
      apiNamed.MoveTo(pcfg->ApiNamed_);
   }

   fon9::StrView rxcfg = fon9::SbrFetchNoTrim(cfgstr, '}');
   while (!fon9::StrTrimHead(&rxcfg).empty()) {
      DeserializeApiNamed(apiNamed, rxcfg, '\n', &ex, cfgldr, "RPT field named define error.");
      if (apiNamed.ApiName_.empty())
         fon9::Raise<fon9::ConfigLoader::Err>("RPT field ApiName cannot be empty.",
                                              cfgldr.GetLineFrom(apiNamed.ApiName_.begin()),
                                              std::errc::bad_message);
      const fon9::seed::Field* sysfld = nullptr;
      if (!apiNamed.SysName_.empty()) {
         if ((sysfld = fac->Fields_.Get(apiNamed.SysName_)) == nullptr)
            fon9::Raise<fon9::ConfigLoader::Err>("RPT field not found.",
                                                 cfgldr.GetLineFrom(apiNamed.SysName_.begin()),
                                                 std::errc::bad_message);
      }
      ApiRptFieldCfg* fldcfg = nullptr;
      for (auto& fcfg : pcfg->RptFields_) {
         if (apiNamed.ApiName_ != &fcfg.ApiNamed_.Name_)
            continue;
         if (apiNamed.SysName_ != (fcfg.Field_ ? fon9::StrView{&fcfg.Field_->Name_} : fon9::StrView{}))
            fon9::Raise<fon9::ConfigLoader::Err>("RPT field dup defined, but SysName not match.",
                                                 cfgldr.GetLineFrom(apiNamed.SysName_.begin()),
                                                 std::errc::bad_message);
         apiNamed.MoveTo(fcfg.ApiNamed_);
         fldcfg = &fcfg;
         break;
      }
      if (fldcfg == nullptr) {
         pcfg->RptFields_.emplace_back(sysfld, apiNamed.ToNamed(sysfld));
         fldcfg = &pcfg->RptFields_.back();
      }
      if (ex.empty())
         continue;
      if (*ex.begin() != '%') {
         fldcfg->ExtParam_.assign(ex);
         fldcfg->FnRpt_ = &FnRptApiField_FixedValue;
      }
      else { // %fnName:ExParam
         fon9::StrTrimHead(&ex, ex.begin() + 1);
         fon9::StrView fnName = fon9::StrFetchTrim(ex, ':');
         fldcfg->FnRpt_ = FnRptApiField_Register::Register(fnName, nullptr);
         if (fldcfg->FnRpt_ == nullptr) {
            fon9::Raise<fon9::ConfigLoader::Err>("RPT %function not found.",
                                                 cfgldr.GetLineFrom(fnName.begin()),
                                                 std::errc::bad_message);
         }
         fldcfg->ExtParam_.assign(fon9::StrTrimHead(&ex));
      }
   }
}
void ApiRptCfgs::Parser::MoveTo(fon9::RevBuffer& rbuf, ApiRptCfgs& cfgs) {
   cfgs.Configs_.reserve(this->size());
   cfgs.IndexMap_.resize(this->size());
   using ExInfos = std::vector<fon9::StrView>;
   ExInfos  exInfos;
   exInfos.reserve(this->size());

   unsigned rptidx = 0;
   unsigned imap = 0;
   for (auto& dycfg : *this) {
      if (ApiRptCfg* cfg = dycfg.get()) {
         cfgs.Configs_.emplace_back(std::move(*cfg));
         cfgs.IndexMap_[imap] = ++rptidx;
         fon9::StrView  exInfo;
         if (imap < this->ToIndex_.OrderTableIdOffset_) {
            if ((imap % RequestE_Size) == RequestE_Abandon)
               exInfo = fon9::StrView{"abandon"};
            else if (static_cast<OmsRequestFactory*>(cfg->Factory_)->OrderFactory_.get() != nullptr) {
               if (static_cast<OmsRequestFactory*>(cfg->Factory_)->RunStep_.get() != nullptr)
                  exInfo = fon9::StrView{"ini"};
               else
                  exInfo = fon9::StrView{"rpt"};
            }
         }
         else if (imap >= this->ToIndex_.EventTableIdOffset_)
            exInfo = fon9::StrView{"event"};
         else {
            unsigned ridx = RequestE_Normal +
               (((imap - this->ToIndex_.OrderTableIdOffset_) % this->ToIndex_.RequestFactoryCount_)
                * RequestE_Size);
            if (auto* preq = (*this)[ridx].get())
               exInfo = &preq->ApiNamed_.Name_;
            else // Request 沒設定回報.
               exInfo = "-";
         }
         exInfos.push_back(exInfo);
      }
      ++imap;
   }

   Configs::const_iterator iend = cfgs.Configs_.cend();
   Configs::const_iterator ibeg = cfgs.Configs_.cbegin();
   while (iend != ibeg) {
      fon9::RevPrint(rbuf, "\n}\n");
      const auto& cfg = *--iend;
      auto ifldend = cfg.RptFields_.cend();
      auto ifldbeg = cfg.RptFields_.cbegin();
      while (ifldend != ifldbeg) {
         const auto& fld = *--ifldend;
         fon9::NumOutBuf nbuf;
         fon9::RevPrintNamed(rbuf, fld.ApiNamed_, ',');
         fon9::RevPrint(rbuf, '\t');
         if (fld.FnRpt_)
            fld.FnRpt_(rbuf, fld, nullptr);
         else if (fld.Field_)
            fon9::RevPrint(rbuf, fld.Field_->GetTypeId(nbuf));
         fon9::RevPrint(rbuf, '\n');
      }
      fon9::RevPrint(rbuf, '{');
      fon9::RevPrintNamedDesc(rbuf, cfg.ApiNamed_, ',');
      // ex = ":abandon", ":ini", ":ReqFactoryName", ":event";
      fon9::StrView ex = exInfos[--rptidx];
      if (!ex.empty())
         fon9::RevPrint(rbuf, ':', ex);
      fon9::RevPrint(rbuf, "RPT.", cfg.ApiNamed_.Name_);
   }
}

//--------------------------------------------------------------------------//

ApiSesCfgSP MakeApiSesCfg(OmsCoreMgrSP coreMgr, fon9::StrView cfgstr) {
   fon9::ConfigLoader cfgldr{""};
   cfgldr.CheckLoad(cfgstr);

   using CfgSP = fon9::intrusive_ptr<ApiSesCfg>;
   CfgSP  retval{new ApiSesCfg{coreMgr}};
   if (auto var = cfgldr.GetVariable("SessionName")) {
      fon9::StrView varstr{&var->Value_.Str_};
      if (!fon9::StrTrim(&varstr).empty())
         retval->SessionName_ = varstr.ToString();
   }

   ApiRptCfgs::Parser rpt{*coreMgr};
   cfgstr = &cfgldr.GetCfgStr();
   while (!fon9::StrTrimHead(&cfgstr).empty()) {
      fon9::StrView  kind = fon9::StrFetchTrim(cfgstr, '.');
      if (kind == "REQ")
         ParseReqCfgs(cfgldr, *retval, cfgstr);
      else if (kind == "RPT") {
         rpt.Parse(cfgldr, cfgstr);
      }
      else {
         fon9::Raise<fon9::ConfigLoader::Err>("MakeApiSesCfg|err=Unknown message.",
                                              cfgldr.GetLineFrom(kind.begin()),
                                              std::errc::bad_message);
      }
   }

   fon9::RevBufferList rbuf{256};
   rpt.MoveTo(rbuf, retval->ApiRptCfgs_);

   retval->ApiReqCfgs_.shrink_to_fit();
   ApiReqCfgs::const_iterator iend = retval->ApiReqCfgs_.cend();
   ApiReqCfgs::const_iterator ibeg = retval->ApiReqCfgs_.cbegin();
   while (iend != ibeg) {
      fon9::RevPrint(rbuf, "\n}\n");
      const auto& cfg = *--iend;
      auto ifldend = cfg.ApiFields_.cend();
      auto ifldbeg = cfg.ApiFields_.cbegin();
      while (ifldend != ifldbeg) {
         const auto& fld = *--ifldend;
         fon9::NumOutBuf nbuf;
         fon9::RevPrintNamed(rbuf, fld.ApiNamed_, ',');
         fon9::RevPrint(rbuf, '\n', fld.Field_->GetTypeId(nbuf), '\t');
      }
      fon9::RevPrint(rbuf, '{');
      fon9::RevPrintNamed(rbuf, cfg.ApiNamed_, ',');
      fon9::RevPrint(rbuf, "REQ.");
   }

   retval->ApiSesCfgStr_ = fon9::BufferTo<std::string>(rbuf.MoveOut());
   if (!retval->ApiSesCfgStr_.empty() && retval->ApiSesCfgStr_.back() == '\n')
      retval->ApiSesCfgStr_.pop_back();
   return retval;
}

//--------------------------------------------------------------------------//

ApiSesCfg::~ApiSesCfg() {
   assert(this->SubrTimes_ == 0);
}
void ApiSesCfg::Unsubscribe() {
   if (--this->SubrTimes_ == 0) {
      this->CoreMgr_->TDayChangedEvent_.Unsubscribe(&this->SubrTDay_);
      this->UnsubscribeRpt();
   }
}
void ApiSesCfg::Subscribe() {
   if (++this->SubrTimes_ != 1)
      return;
   if (this->SubrTDay_ || this->ApiRptCfgs_.empty())
      return;
   using namespace std::placeholders;
   this->CoreMgr_->TDayChangedEvent_.Subscribe(&this->SubrTDay_,
                                               std::bind(&ApiSesCfg::SubscribeRpt, this, _1));
   auto tdayLocker = this->CoreMgr_->TDayChangedEvent_.Lock();
   if (auto core = this->CoreMgr_->CurrentCore())
      this->SubscribeRpt(*core);
}
void ApiSesCfg::UnsubscribeRpt() {
   if (this->CurrentCore_)
      this->CurrentCore_->ReportSubject().Unsubscribe(&this->SubrRpt_);
}
void ApiSesCfg::SubscribeRpt(OmsCore& core) {
   if (this->CurrentCore_.get() == &core)
      return;
   this->UnsubscribeRpt();
   this->CurrentCore_.reset(&core);
   using namespace std::placeholders;
   core.ReportSubject().Subscribe(&this->SubrRpt_, std::bind(&ApiSesCfg::OnReport, this, _1, _2));
}
bool ApiSesCfg::MakeReportMessage(fon9::RevBufferList& rbuf, const OmsRxItem& item) const {
   if (this->ReportMessageFor_ == &item) {
      fon9::RevPrint(rbuf, this->ReportMessage_);
      return true;
   }
   const ApiRptCfg* rptcfg;
   OmsRxSNO         refSNO;
   fon9_WARN_DISABLE_SWITCH;
   switch (item.RxKind()) {
   case f9fmkt_RxKind_Event:
      refSNO = 0;
      rptcfg = this->ApiRptCfgs_.GetRptCfgEvent(*static_cast<const OmsEvent*>(&item)->Creator_);
      break;
   case f9fmkt_RxKind_Order:
      refSNO = static_cast<const OmsOrderRaw*>(&item)->Request().RxSNO();
      rptcfg = this->ApiRptCfgs_.GetRptCfgOrder(static_cast<const OmsOrderRaw*>(&item)->Order().Creator(),
                                                static_cast<const OmsOrderRaw*>(&item)->Request().Creator());
      break;
   default:
      const OmsOrderRaw* ordraw = static_cast<const OmsRequestBase*>(&item)->LastUpdated();
      assert(ordraw != nullptr || static_cast<const OmsRequestBase*>(&item)->IsAbandoned());
      if (ordraw == nullptr)
         refSNO = 0;
      else if(const auto* inireq = ordraw->Order().Initiator())
         refSNO = inireq->RxSNO();
      else
         refSNO = 0;
      rptcfg = this->ApiRptCfgs_.GetRptCfgRequest(static_cast<const OmsRequestBase*>(&item)->Creator(),
                                                  ordraw == nullptr ? ApiRptCfgs::RequestE_Abandon : ApiRptCfgs::RequestE_Normal);
      break;
   }
   fon9_WARN_POP;
   if (rptcfg == nullptr)
      return false;
   ApiRptFieldArg arg{item};
   auto iend = rptcfg->RptFields_.cend();
   auto ibeg = rptcfg->RptFields_.cbegin();
   if (ibeg != iend) {
      for (;;) {
         auto& cfg = *--iend;
         if (cfg.FnRpt_)
            cfg.FnRpt_(rbuf, cfg, &arg);
         else if (cfg.Field_)
            cfg.Field_->CellRevPrint(arg, nullptr, rbuf);
         if (ibeg == iend)
            break;
         fon9::RevPrint(rbuf, *fon9_kCSTR_CELLSPL);
      }
   }
   fon9::ByteArraySizeToBitvT(rbuf, fon9::CalcDataSize(rbuf.cfront()));
   if (refSNO == item.RxSNO() || refSNO == 0)
      fon9::RevPutBitv(rbuf, fon9_BitvV_Number0);
   else
      fon9::ToBitv(rbuf, refSNO);
   fon9::ToBitv(rbuf, item.RxSNO());
   fon9::ToBitv(rbuf, this->ApiRptCfgs_.RptTableId(*rptcfg) + 1u);
   return true;
}
void ApiSesCfg::OnReport(OmsCore& core, const OmsRxItem& item) {
   if (this->CurrentCore_.get() != &core)
      return;
   fon9::RevBufferList  rbuf{128};
   if (this->MakeReportMessage(rbuf, item) && item.RxSNO() != 0) {
      // this->ReportMessage_ 使用 clear 之後 append 的方式,
      // 在 this->ReportMessage_ 已分配空間足夠的情況下, 可能可以少一次記憶體分配.
      this->ReportMessage_.clear();
      fon9::BufferAppendTo(rbuf.MoveOut(), this->ReportMessage_);
      this->ReportMessageFor_ = &item;
   }
}

} // namespaces
