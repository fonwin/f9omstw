// \file f9omstw/OmsRequestPolicy.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestPolicy.hpp"
#include "f9omstw/OmsBrkTree.hpp"
#include "fon9/Unaligned.hpp"

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
void OmsRequestPolicy::AddIvRights(OmsIvBase* ivr, fon9::StrView subWilds, OmsIvRight rights) {
   if (ivr == nullptr) {
      if ((subWilds.empty() || subWilds == "*")) {
         this->IvRights_ = rights | OmsIvRight::IsAdmin;
         return;
      }
   }
   else if (ivr->IvKind_ == OmsIvKind::Brk && subWilds == "*") {
      subWilds.Reset(nullptr);
   }
   IvRec& rec = this->IvMap_.kfetch(ivr);
   if (subWilds.empty())
      rec.Rights_ = rights;
   else {
      rec.SubWilds_.append(subWilds);
      rec.SubWilds_.push_back('\n');
      rec.SubWilds_.append(&rights, sizeof(rights));
   }
}
OmsIvRight OmsRequestPolicy::GetIvRights(OmsIvBase* ivr) const {
   if (!this->IvMap_.empty()) {
      OmsIvBase*  ivSrc = ivr;
      for (;;) {
         auto ifind = this->IvMap_.find(ivr);
         if (fon9_LIKELY(ifind != this->IvMap_.end())) {
            if (fon9_LIKELY(ivSrc == ivr))
               return ifind->Rights_;
            if (fon9_LIKELY(ifind->SubWilds_.empty())) {
               if (ivr == nullptr || ivr->IvKind_ == OmsIvKind::Brk)
                  return ifind->Rights_;
               break; // 找不到 wild 的設定, 使用 this->IvRights_
            }
            fon9::RevBufferFixedSize<1024> rbuf;
            RevPrintIvKey(rbuf, ivSrc, ivr);
            fon9::StrView subw = ToStrView(ifind->SubWilds_);
            #define kTailSize    (1 + sizeof(OmsIvRight))
            for (;;) {
               const char* pspl = subw.Find('\n');
               if (pspl == nullptr)
                  pspl = subw.end();
               if (fon9::IsStrWildMatch(ToStrView(rbuf), fon9::StrView{subw.begin(), pspl})) {
                  if (pspl + kTailSize > subw.end()) // '\n' 已到尾端: '\n' 之後沒有 IvRight; => 不會發生此情況!
                     return ifind->Rights_;          // 直接使用 ifind->Rights_;
                  return fon9::GetUnaligned(reinterpret_cast<const OmsIvRight*>(pspl + 1));
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
   return IsEnumContains(this->IvRights_, OmsIvRight::IsAdmin) ? this->IvRights_ : OmsIvRight::DenyAll;
}

OmsIvKind OmsAddIvRights(OmsRequestPolicy& dst, const fon9::StrView srcIvKey, OmsIvRight ivRights, OmsBrkTree& brks) {
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
   dst.AddIvRights(ivBase, subWilds, ivRights);
   return OmsIvKind::Unknown;
}

} // namespaces
