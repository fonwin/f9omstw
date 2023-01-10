// \file f9omstw/OmsRequestPolicy.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestPolicy_hpp__
#define __f9omstw_OmsRequestPolicy_hpp__
#include "f9omstw/OmsPoIvListAgent.hpp"
#include "f9omstw/OmsPoUserRights.hpp"
#include "f9omstw/OmsIvBase.hpp"

namespace f9omstw {

/// 每次使用者登入後, 都會從「使用者管理員」 **複製** 一份下單權限的設定.
/// 只能在 OmsCore thread 安全的使用.
class OmsRequestPolicy : public fon9::intrusive_ref_counter<OmsRequestPolicy> {
   fon9_NON_COPY_NON_MOVE(OmsRequestPolicy);
   OmsOrdTeamGroupId OrdTeamGroupId_{0};
   bool              IsAllowAnyOrdNo_{false};
   OmsIvConfig       IvConfig_{OmsIvRight::DenyAll};
   /// 額外禁止項目.
   mutable OmsIvRight         IvDenys_{};
   mutable OmsUserRightFlag   UserRightFlags_{};
   char                       padding___[3];

   struct IvRec {
      OmsIvBase*        Iv_;
      /// 如果有多組 SubWilds_, 中間用 '\n' 分隔, 例如: ("A*" + "\n" + IvConcig) + ("B*" + "\n" + IvConcig);
      fon9::CharVector  SubWilds_;
      OmsIvConfig       Config_{OmsIvRight::DenyAll};
      char              Padding____[6];

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

   bool IsAllowAnyOrdNo() const {
      return this->IsAllowAnyOrdNo_;
   }
   OmsOrdTeamGroupId OrdTeamGroupId() const {
      return this->OrdTeamGroupId_;
   }
   void SetOrdTeamGroupCfg(const OmsOrdTeamGroupCfg* tg);

   /// 設定額外禁止項目.
   /// 可能在 OmsPoUserRights 有異動時立即更新.
   void SetIvDenys(OmsIvRight v) const {
      this->IvDenys_ = (v & OmsIvRight::DenyAll);
   }
   OmsUserRightFlag UserRightFlags() const {
      return this->UserRightFlags_;
   }
   void SetUserRightFlags(OmsUserRightFlag uRightFlags) const {
      this->UserRightFlags_ = uRightFlags;
   }

   /// - 如果 ivr 是 Subac:
   ///   - 則 subWilds 必定是 empty().
   /// - 如果 ivr 是 Ivac:
   ///   - 如果 subWilds 是 empty(), 則不包含任何子帳權限, "BRKX-1234567" 與 "BRKX-1234567-*" 須分別設定.
   ///   - 如果 subWilds 是 "SUBAC"; 則子帳(SUBAC)部分必定有 '*' 或 '?'.
   /// - 如果 ivr 是 Brk:
   ///   - 如果 subWilds 是 empty() 或 "*"; 則表示針對該券商的全部帳號.
   ///   - 如果 subWilds 是 "1234567-SUBAC"; 帳號(1234567)部分必定有 '*' 或 '?'.
   /// - 如果 ivr 是 nullptr:
   ///   - 如果 subWilds 是 empty() 或 "*", 則設定 this->IvConfig_.Rights_ = IsAdmin | config.Rights_; 不加入 this->IvMap_;
   ///   - 如果 subWilds 是 "BRKX-1234567-SUBAC"; 券商代號(BRKX)部分必定有 '*' 或 '?'.
   void AddIvConfig(OmsIvBase* ivr, fon9::StrView subWilds, OmsIvConfig config);

   /// 返回值會額外加上 this->IvDenys_;
   OmsIvRight GetIvRights(OmsIvBase* ivr, OmsIvConfig* ivConfig) const;

   /// 必須是 this->IvMap_.empty() 且 IsEnumContains(this->IvConfig_.Rights_, OmsIvRight::IsAdmin) 才會傳回 this->IvConfig_.Rights_;
   OmsIvRight GetIvrAdminRights() const {
      return this->IvMap_.empty() && IsEnumContains(this->IvConfig_.Rights_, OmsIvRight::IsAdmin)
         ? (this->IvConfig_.Rights_ | this->IvDenys_) : OmsIvRight::DenyAll;
   }
   bool HasAdminRight() const {
      return IsEnumContains(this->IvConfig_.Rights_, OmsIvRight::IsAdmin);
   }
};
using OmsRequestPolicySP = fon9::intrusive_ptr<const OmsRequestPolicy>;

inline OmsIvRight RxKindToIvRightDeny(f9fmkt_RxKind rxKind) {
   fon9_WARN_DISABLE_SWITCH;
   switch (rxKind) {
   case f9fmkt_RxKind_RequestNew:      return OmsIvRight::DenyTradingNew;
   case f9fmkt_RxKind_RequestDelete:
   case f9fmkt_RxKind_RequestChgQty:   return OmsIvRight::DenyTradingChgQty;
   case f9fmkt_RxKind_RequestChgPri:   return OmsIvRight::DenyTradingChgPri;
   case f9fmkt_RxKind_RequestQuery:    return OmsIvRight::DenyTradingQuery;
   default:                            return OmsIvRight::DenyTradingAll;
   }
   fon9_WARN_POP;
}

/// 將 srcIvKey 設定的 ivRights 加入 dst.
/// \retval OmsIvKind::Unknow 成功加入一個設定.
/// \retval OmsIvKind::Brk    brks.GetBrkRec(); 失敗, 無法取得券商資料.
/// \retval OmsIvKind::Ivac   brk->FetchIvac(); 失敗.
/// \retval OmsIvKind::Subac  ivac->FetchSubac(); 失敗.
OmsIvKind OmsAddIvConfig(OmsRequestPolicy& dst, const fon9::StrView srcIvKey, const OmsIvConfig& ivConfig, OmsBrkTree& brks);

struct OmsRequestPolicyCfg {
   fon9::CharVector  TeamGroupName_;
   OmsUserRights     UserRights_;

   using IvListPolicy = OmsPoIvListAgent::PolicyConfig;
   IvListPolicy      IvList_;

   OmsRequestPolicySP MakePolicy(OmsResource& res,
                                 fon9::intrusive_ptr<OmsRequestPolicy> pol = new OmsRequestPolicy) const;
};

} // namespaces
#endif//__f9omstw_OmsRequestPolicy_hpp__
