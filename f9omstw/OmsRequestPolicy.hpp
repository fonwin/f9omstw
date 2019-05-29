// \file f9omstw/OmsRequestPolicy.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestPolicy_hpp__
#define __f9omstw_OmsRequestPolicy_hpp__
#include "f9omstw/OmsPoIvList.hpp"
#include "f9omstw/OmsPoUserRights.hpp"
#include "f9omstw/OmsIvBase.hpp"
#include "f9omstw/OmsBase.hpp"

namespace f9omstw {

/// 每次使用者登入後, 都會從「使用者管理員」 **複製** 一份下單權限的設定.
class OmsRequestPolicy : public fon9::intrusive_ref_counter<OmsRequestPolicy> {
   fon9_NON_COPY_NON_MOVE(OmsRequestPolicy);
   OmsOrdTeamGroupId OrdTeamGroupId_{0};
   bool              IsAllowAnyOrdNo_{false};
   OmsIvRight        IvRights_{OmsIvRight::DenyAll};
   char              padding___[6];

   struct IvRec {
      OmsIvBase*        Iv_;
      /// 如果有多組 SubWilds_, 中間用 '\n' 分隔, 例如: ("A*" + "\n" + IvRights) + ("B*" + "\n" + IvRights);
      fon9::CharVector  SubWilds_;
      OmsIvRight        Rights_{OmsIvRight::DenyAll};
      char              padding__[7];

      IvRec(OmsIvBase* iv) : Iv_{iv} {
      }
   };
   struct IvRecCmpr {
      bool operator()(const IvRec& lhs, const IvRec& rhs) const {
         return lhs.Iv_ < rhs.Iv_;
      }
      bool operator()(const IvRec& lhs, OmsIvBase* rhs) const {
         return lhs.Iv_ < rhs;
      }
      bool operator()(OmsIvBase* lhs, const IvRec& rhs) const {
         return lhs < rhs.Iv_;
      }
   };
   using IvMap = fon9::SortedVectorSet<IvRec, IvRecCmpr>;
   IvMap IvMap_;

public:
   OmsRequestPolicy();
   virtual ~OmsRequestPolicy();

   bool PreCheckInUser(OmsRequestRunner& reqRunner) const;

   OmsOrdTeamGroupId OrdTeamGroupId() const {
      return this->OrdTeamGroupId_;
   }
   bool IsAllowAnyOrdNo() const {
      return this->IsAllowAnyOrdNo_;
   }
   void SetOrdTeamGroupCfg(const OmsOrdTeamGroupCfg& tg);

   /// - 如果 iv 是 Subac:
   ///   - 則 subWilds 必定是 empty().
   /// - 如果 iv 是 Ivac:
   ///   - 如果 subWilds 是 empty(), 則不包含任何子帳權限, "BRKX-1234567" 與 "BRKX-1234567-*" 須分別設定.
   ///   - 如果 subWilds 是 "SUBAC"; 則子帳(SUBAC)部分必定有 '*' 或 '?'.
   /// - 如果 iv 是 Brk:
   ///   - 如果 subWilds 是 empty() 或 "*"; 則表示針對該券商的全部帳號.
   ///   - 如果 subWilds 是 "1234567-SUBAC"; 帳號(1234567)部分必定有 '*' 或 '?'.
   /// - 如果 iv 是 nullptr:
   ///   - 如果 subWilds 是 empty() 或 "*", 則設定 this->IvRights_ = IsAdmin | rights; 不加入 this->IvMap_;
   ///   - 如果 subWilds 是 "BRKX-1234567-SUBAC"; 券商代號(BRKX)部分必定有 '*' 或 '?'.
   void AddIvRights(OmsIvBase* ivr, fon9::StrView subWilds, OmsIvRight rights);

   OmsIvRight GetIvRights(OmsIvBase* ivr) const;
};
using OmsRequestPolicySP = fon9::intrusive_ptr<const OmsRequestPolicy>;

} // namespaces
#endif//__f9omstw_OmsRequestPolicy_hpp__
