// \file f9omstw/OmsRequestPolicy.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestPolicy.hpp"
#include "f9omstw/OmsRequestRunner.hpp"
#include "f9omstw/OmsBrk.hpp"
#include "f9omstw/OmsSubac.hpp"
#include "fon9/Unaligned.hpp"

namespace f9omstw {

OmsRequestPolicy::OmsRequestPolicy() {
}
OmsRequestPolicy::~OmsRequestPolicy() {
}
bool OmsRequestPolicy::PreCheckInUser(OmsRequestRunner& reqRunner) const {
   (void)reqRunner;
   // reqRunner.Request_->RequestKind() 是否允許此類下單要求?
   // 但此時還在 user thread, 無法取得 OmsIvBase* ivr;
   return true;
}

void OmsRequestPolicy::SetOrdTeamGroupCfg(const OmsOrdTeamGroupCfg& tg) {
   this->OrdTeamGroupId_ = tg.TeamGroupId_;
   this->IsAllowAnyOrdNo_ = tg.IsAllowAnyOrdNo_;
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

} // namespaces
