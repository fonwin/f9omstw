// \file f9omstw/OmsRequestPolicy.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestPolicy.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/Unaligned.hpp"
#include "fon9/Log.hpp"

namespace f9omstw {

OmsRequestPolicy::OmsRequestPolicy() {
}
OmsRequestPolicy::~OmsRequestPolicy() {
}
void OmsRequestPolicy::SetOrdTeamGroupCfg(const OmsOrdTeamGroupCfg* tg) {
   if (tg) {
      this->OrdTeamGroupId_ = tg->TeamGroupId_;
      this->IsAllowAnyOrdNo_ = tg->IsAllowAnyOrdNo_;
   }
   else {
      this->OrdTeamGroupId_ = 0;
      this->IsAllowAnyOrdNo_ = false;
   }
}
void OmsRequestPolicy::AddIvConfig(OmsIvBase* ivr, fon9::StrView subWilds, OmsIvConfig config) {
   if (ivr == nullptr) {
      if ((subWilds.empty() || subWilds == "*")) {
         this->IvConfig_ = config;
         this->IvConfig_.Rights_ |= OmsIvRight::IsAdmin;
         return;
      }
   }
   else if (ivr->IvKind_ == OmsIvKind::Brk && subWilds == "*") {
      subWilds.Reset(nullptr);
   }
   IvRec& rec = this->IvMap_.kfetch(ivr);
   if (subWilds.empty())
      rec.Config_ = config;
   else {
      rec.SubWilds_.append(subWilds);
      rec.SubWilds_.push_back('\n');
      rec.SubWilds_.append(&config, sizeof(config));
   }
}
OmsIvRight OmsRequestPolicy::GetIvRights(OmsIvBase* ivr, OmsIvConfig* ivConfig) const {
   if (fon9_LIKELY(!this->IvMap_.empty())) {
      OmsIvBase*  ivSrc = ivr;
      for (;;) {
         auto ifind = this->IvMap_.find(ivr);
         if (fon9_LIKELY(ifind != this->IvMap_.end())) {
            if (ivConfig)
               *ivConfig = ifind->Config_;
            if (fon9_LIKELY(ivSrc == ivr))
               return ifind->Config_.Rights_ | this->IvDenys_;
            if (fon9_LIKELY(ifind->SubWilds_.empty())) {
               if (ivr == nullptr || ivr->IvKind_ == OmsIvKind::Brk)
                  return ifind->Config_.Rights_ | this->IvDenys_;
               break; // 找不到 wild 的設定, 使用 this->IvRights_
            }
            fon9::RevBufferFixedSize<1024> rbuf;
            RevPrintIvKey(rbuf, ivSrc, ivr);
            fon9::StrView subw = ToStrView(ifind->SubWilds_);
            #define kTailSize    (1 + sizeof(OmsIvConfig))
            while (const char* pspl = subw.Find('\n')) {
               if (fon9::IsStrWildMatch(ToStrView(rbuf), fon9::StrView{subw.begin(), pspl})) {
                  if (pspl + kTailSize > subw.end()) // '\n' 已到尾端: '\n' 之後沒有 IvRight; => 不會發生此情況!
                     return ifind->Config_.Rights_ | this->IvDenys_; // 直接使用 ifind->Rights_;
                  return fon9::GetUnaligned(reinterpret_cast<const OmsIvRight*>(pspl + 1)) | this->IvDenys_;
               }
               if (pspl + kTailSize >= subw.end())
                  break;
               subw.SetBegin(pspl + kTailSize);
            } // for (search ifind->SubWilds_
            break; // 找不到 wild 的設定, 使用 this->IvRights_
         } // if (found ivr
         if (ivr == nullptr)
            break; // 使用 this->IvRights_
         ivr = ivr->Parent_.get();
      } // for (search ivr
   } // if (!this->IvMap_.empty()
   if (ivConfig)
      *ivConfig = this->IvConfig_;
   return IsEnumContains(this->IvConfig_.Rights_, OmsIvRight::IsAdmin)
      ? (this->IvConfig_.Rights_ | this->IvDenys_)
      : OmsIvRight::DenyAll;
}
void OmsRequestPolicy::SetCondAllows(fon9::StrView value) {
   this->CondAllows_.clear();
   if (value.Get1st() == '*') {
      this->CondAllows_.push_back('*');
      return;
   }
   for (char c : value) {
      if (c == '-')
         break;
      if (fon9::isspace(c))
         continue;
      this->CondAllows_.push_back(c);
   }
}
bool OmsRequestPolicy::IsCondAllowed(fon9::StrView condName) const {
   const char* const pbeg = this->CondAllows_.begin();
   if (*pbeg == '*' || condName.empty())
      return true;
   const char* const pend = this->CondAllows_.end();
   if (pbeg == pend)
      return false;
   const auto condNameSz = condName.size();
   const char* pfind = pbeg;
   for (;;) {
      pfind = std::search(pfind, pend, condName.begin(), condName.end());
      if (pfind == pend)
         return false;
      if (pfind + condNameSz == pend || *(pfind + condNameSz) == ',') {
         if (pfind == pbeg || *(pfind - 1) == ',')
            return true;
      }
      ++pfind;
   }
}
//--------------------------------------------------------------------------//
OmsIvKind OmsAddIvConfig(OmsRequestPolicy& dst, const fon9::StrView srcIvKey, const OmsIvConfig& ivConfig, OmsBrkTree& brks) {
   OmsIvBase*     ivBase;
   fon9::StrView  subWilds;
   fon9::StrView  ivKey = srcIvKey;
   fon9::StrView  brkId = fon9::StrFetchTrim(ivKey, '-');
   if (fon9::FindWildcard(brkId)) {
      ivBase = nullptr;
      subWilds = srcIvKey;
   }
   else if (auto* brk = brks.GetBrkRec(brkId)) {
      subWilds = ivKey;
      fon9::StrView strIvacNo = fon9::StrFetchTrim(ivKey, '-');
      // 不考慮 "BRKX- -SUBAC" 這種設定, 正確應該是: "BRKX-*-SUBAC";
      if (fon9::StrTrim(&strIvacNo).empty() || fon9::FindWildcard(strIvacNo))
         ivBase = brk;
      else if (auto* ivac = brk->FetchIvac(fon9::StrTo(strIvacNo, IvacNo{}))) {
         fon9::StrTrim(&ivKey);
         if (ivKey.empty()) {
            subWilds.Reset(nullptr);
            ivBase = ivac;
         }
         else if (fon9::FindWildcard(ivKey)) {
            subWilds = ivKey;
            ivBase = ivac;
         }
         else if ((ivBase = ivac->FetchSubac(ivKey)) != nullptr)
            subWilds.Reset(nullptr);
         else {
            return OmsIvKind::Subac;
         }
      }
      else {
         return OmsIvKind::Ivac;
      }
   }
   else {
      return OmsIvKind::Brk;
   }
   dst.AddIvConfig(ivBase, subWilds, ivConfig);
   return OmsIvKind::Unknown;
}

OmsRequestPolicySP OmsRequestPolicyCfg::MakePolicy(OmsResource& res, fon9::intrusive_ptr<OmsRequestPolicy> pol) const {
   pol->SetIvDenys(this->UserRights_.IvDenys_);
   pol->SetUserRightFlags(this->UserRights_.Flags_);
   pol->SetScForceFlags(this->UserRights_.ScForceFlags_);
   pol->SetOrdTeamGroupCfg(res.OrdTeamGroupMgr_.SetTeamGroup(
      ToStrView(this->TeamGroupName_), ToStrView(this->UserRights_.AllowOrdTeams_)));
   pol->SetCondAllows(ToStrView(this->UserRights_.CondAllows_));
   pol->SetCondPriorityM(this->UserRights_.CondPriorityM_);
   pol->SetCondPriorityL(this->UserRights_.CondPriorityL_);
   pol->SetCondExpMaxC(this->UserRights_.CondExpMaxC_);
   pol->SetCondGrpMaxC(this->UserRights_.CondGrpMaxC_);

   for (const auto& item : this->IvList_) {
      auto ec = OmsAddIvConfig(*pol, ToStrView(item.first), item.second, *res.Brks_);
      if (ec != OmsIvKind::Unknown)
         fon9_LOG_ERROR("OmsRequestPolicyCfg.FetchPolicy|IvKey=", item.first, "|ec=", ec);
   }
   return pol;
}

} // namespaces
