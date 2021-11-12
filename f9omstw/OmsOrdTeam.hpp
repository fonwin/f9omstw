/// \file f9omstw/OmsOrdTeam.hpp
/// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsOrdTeam_hpp__
#define __f9omstw_OmsOrdTeam_hpp__
#include "f9omstw/OmsBase.hpp"
#include "fon9/CharAryL.hpp"
#include "fon9/CharVector.hpp"
#include "fon9/SortedVector.hpp"

namespace f9omstw {

using OmsOrdTeam = fon9::CharAryL<OmsOrdNo::size()>;
using OmsOrdTeamList = std::vector<OmsOrdTeam>;
typedef int (*FnIncOrdNo)(char* pbeg, char* pend);

/// cfg = 允許的委託櫃號, 使用 "," 分隔, 或使用 "-" 設定範圍.
/// - "A-B" 表示可編號 A0000..Bzzzz
/// - "A-C9" 等同於: "A-B,C0-C9" 表示可編號 A0000..Bzzzz; C0000-C9zzz
/// - "A9-C" 等同於: "A9-Az,B-C" 表示可編號 A9000..Czzzz;
/// - "C-A" 等同於 "A-C", 也就是處理時會把小的放前面.
/// - "X-" 等同於 "X"; 或許您應該改成 "X-z"?
/// - "-X" 等同於 "X"; 或許您應該改成 "0-X"?
/// - 前面的設定, 會放到 list 的尾端, 因為 list.pop_back(); 較省時.
void ConfigToTeamList(OmsOrdTeamList& list, fon9::StrView cfg);

class OmsOrdTeamGroupCfg {
   fon9_NON_COPYABLE(OmsOrdTeamGroupCfg); // 為了避免誤用, 所以禁止複製.
public:
   OmsOrdTeamGroupCfg(OmsOrdTeamGroupCfg&&) = default;
   OmsOrdTeamGroupCfg& operator=(OmsOrdTeamGroupCfg&&) = default;
   OmsOrdTeamGroupCfg() = default;

   /// 例: "usr.PolicyId"; "src.SrcCode"; "tln.BrkId.PvcId"...
   fon9::CharVector  Name_;
   fon9::CharVector  Config_;
   OmsOrdTeamList    TeamList_;
   uint16_t          UpdatedCount_{0};
   bool              IsAllowAnyOrdNo_{false};
   char              padding_____[1];
   OmsOrdTeamGroupId TeamGroupId_;

   /// 序號的計算方法. 若有定義 F9OMS_ORDNO_IS_NUM 則此處設定無效;
   /// 預設=文數字: int f9omstw_IncStrAlpha(char* pbeg, char* pend);
   /// 純數字: int f9omstw_IncStrDec(char* pbeg, char* pend);
   FnIncOrdNo  FnIncOrdNo_;
};
class OmsOrdTeamGroupMgr {
   using TeamGroupCfgs = std::vector<OmsOrdTeamGroupCfg>;
   using TeamNameMap = fon9::SortedVector<fon9::CharVector, OmsOrdTeamGroupId>;
   TeamGroupCfgs  TeamGroupCfgs_;
   TeamNameMap    TeamNameMap_;
public:
   const OmsOrdTeamGroupCfg* GetTeamGroupCfg(OmsOrdTeamGroupId tgId) const {
      return --tgId >= this->TeamGroupCfgs_.size() ? nullptr : &this->TeamGroupCfgs_[tgId];
   }
   /// 若 cfgstr 第一碼為 '*', 則表示有 IsAllowAnyOrdNo_ 權限.
   /// 若 name.empty() 則傳回 nullptr;
   /// 傳回值須立即使用, 下次呼叫 SetTeamGroup() 時, 可能會失效!
   /// 下次要用時, 應使用 retval->TeamGroupId_ 再次取得;
   const OmsOrdTeamGroupCfg* SetTeamGroup(fon9::StrView name, fon9::StrView cfgstr);
};

/// 放在 OrdNoMap 裡面, 要分配委託書號時:
/// - 用新單要求的「櫃號群組Id」, 取得櫃號群組.
/// - 櫃號群組使用前, 先檢查內容是否有異動: OmsOrdTeamGroupCfg::UpdatedCount_;
class OmsOrdTeamGroups {
   fon9_NON_COPY_NON_MOVE(OmsOrdTeamGroups);
   struct TeamList : public OmsOrdTeamList {
      uint16_t UpdatedCount_{0};
      char     padding_____[6];
   };
   using TeamGroups = std::vector<TeamList>;
   TeamGroups  TeamGroups_;
public:
   OmsOrdTeamGroups() = default;

   OmsOrdTeamList* FetchTeamList(const OmsOrdTeamGroupCfg& cfg) {
      if (fon9_UNLIKELY(this->TeamGroups_.size() < cfg.TeamGroupId_))
         this->TeamGroups_.resize(cfg.TeamGroupId_);
      auto& res = this->TeamGroups_[cfg.TeamGroupId_ - 1];
      if (fon9_UNLIKELY(res.UpdatedCount_ != cfg.UpdatedCount_)) {
         res.UpdatedCount_ = cfg.UpdatedCount_;
         *static_cast<OmsOrdTeamList*>(&res) = cfg.TeamList_;
      }
      return &res;
   }
};

} // namespaces
#endif//__f9omstw_OmsOrdTeam_hpp__
