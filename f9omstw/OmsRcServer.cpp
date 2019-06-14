// \file f9omstw/OmsRcServer.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRcServer.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "fon9/ConfigLoader.hpp"

namespace f9omstw {

FnPutApiField FnPutApiField_Register::Register(fon9::StrView fnName, FnPutApiField fnPutApiField) {
   return SimpleFactoryRegister(fnName, fnPutApiField);
}
void FnPutApiField_FixedValue(const ApiReqFieldCfg& cfg, const ApiReqFieldArg& arg) {
   if (cfg.Field_->IsNull(arg))
      cfg.Field_->StrToCell(arg, ToStrView(cfg.ExtParam_));
}

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
   fon9::Named ToNamed() {
      return fon9::Named(this->ApiName_.ToString(),
                         std::move(this->Title_),
                         std::move(this->Description_));
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
         case '=':
            goto __CHECK_EX;
         }
         break;
      case '=':
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
   flds.emplace_back(fld, apiNamed.ToNamed());
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
   ApiNamed apiNamed;
   if (const char* perr = DeserializeApiNamed(apiNamed, cfgstr, '{', nullptr)) {
      fon9::Raise<fon9::ConfigLoader::Err>("REQ named define error.",
                                           cfgldr.GetLineFrom(perr),
                                           std::errc::bad_message);
   }
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
         break;
      }
   }
   if (pcfg == nullptr) {
      auto* fac = cfgs.CoreMgr_->RequestFactoryPark().GetFactory(apiNamed.SysName_);
      if (fac == nullptr) {
         fon9::Raise<fon9::ConfigLoader::Err>("REQ factory not found.",
                                              cfgldr.GetLineFrom(pcfgbeg),
                                              std::errc::bad_message);
      }
      cfgs.ApiReqCfgs_.emplace_back(*fac, apiNamed.ToNamed());
      pcfg = &cfgs.ApiReqCfgs_.back();
   }
   fon9::StrView ex;
   fon9::StrView rxcfg = fon9::SbrFetchNoTrim(cfgstr, '}');
   while (!fon9::StrTrimHead(&rxcfg).empty()) {
      if (const char* perr = DeserializeApiNamed(apiNamed, rxcfg, '\n', &ex)) {
         fon9::Raise<fon9::ConfigLoader::Err>("REQ field named define error.",
                                              cfgldr.GetLineFrom(perr),
                                              std::errc::bad_message);
      }
      ApiReqFieldCfg* fld = (apiNamed.ApiName_.empty()
                             ? SetReqSysField(*pcfg->Factory_, pcfg->SysFields_, apiNamed.SysName_)
                             : SetReqApiField(cfgldr, *pcfg->Factory_, pcfg->ApiFields_, apiNamed));
      if (fld == nullptr) {
         fon9::Raise<fon9::ConfigLoader::Err>("REQ field not found.",
                                              cfgldr.GetLineFrom(apiNamed.SysName_.begin()),
                                              std::errc::bad_message);
      }
      if (ex.empty())
         continue;
      if (*ex.begin() != '%') {
         fld->ExtParam_.assign(ex);
         fld->FnPut_ = &FnPutApiField_FixedValue;
      }
      else { // %fnName:ExParam
         fon9::StrTrimHead(&ex, ex.begin() + 1);
         fon9::StrView fnName = fon9::StrFetchTrim(ex, ':');
         fld->FnPut_ = FnPutApiField_Register::Register(fnName, nullptr);
         if (fld->FnPut_ == nullptr) {
            fon9::Raise<fon9::ConfigLoader::Err>("REQ %function not found.",
                                                 cfgldr.GetLineFrom(fnName.begin()),
                                                 std::errc::bad_message);
         }
         fld->ExtParam_.assign(fon9::StrTrimHead(&ex));
      }
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

   cfgstr = &cfgldr.GetCfgStr();
   while (!fon9::StrTrimHead(&cfgstr).empty()) {
      fon9::StrView  kind = fon9::StrFetchTrim(cfgstr, '.');
      if (kind == "REQ")
         ParseReqCfgs(cfgldr, *retval, cfgstr);
      else {
         fon9::Raise<fon9::ConfigLoader::Err>("MakeApiSesCfg|err=Unknown message.",
                                              cfgldr.GetLineFrom(kind.begin()),
                                              std::errc::bad_message);
      }
   }

   fon9::RevBufferList rbuf{256};
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

} // namespaces
