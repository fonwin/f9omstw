/// \file f9omstw/OmsPoUserRightsAgent.hpp
/// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsPoUserRightsAgent_hpp__
#define __f9omstw_OmsPoUserRightsAgent_hpp__
#include "f9omstw/OmsPoUserRights.hpp"
#include "fon9/auth/AuthMgr.hpp"

namespace f9omstw {

/// 「OmsUserRights」政策管理.
/// \code
/// AuthMgr/PoOmsUser.Layout =
///   { KeyName = "PolicyId",
///     Tab[0] = { TabName  = "Rights",
///                Fields[] = {"OrdTeams", "FcReqCount", "FcReqMS", "LgOut"}
///              }
///   }
/// \endcode
class OmsPoUserRightsAgent : public fon9::auth::PolicyAgent {
   fon9_NON_COPY_NON_MOVE(OmsPoUserRightsAgent);
   using base = fon9::auth::PolicyAgent;

public:
   OmsPoUserRightsAgent(fon9::seed::MaTree* authMgrAgents, std::string name);

   #define fon9_kCSTR_OmsPoUserRightsAgent_Name    "OmsPoUserRights"
   /// 在 authMgr.Agents_ 上面新增一個 OmsPoUserRightsAgent.
   /// \retval nullptr   name 已存在.
   /// \retval !=nullptr 新增成功的 OmsPoUserRightsAgent 物件, 返回前會先執行 LinkStorage().
   static fon9::intrusive_ptr<OmsPoUserRightsAgent> Plant(fon9::auth::AuthMgr& authMgr, std::string name = fon9_kCSTR_OmsPoUserRightsAgent_Name) {
      auto res = authMgr.Agents_->Plant<OmsPoUserRightsAgent>("OmsPoUserRightsAgent.Plant", std::move(name));
      if (res)
         res->LinkStorage(authMgr.Storage_, 128);
      return res;
   }

   using PolicyConfig = OmsUserRights;
   /// 如果 teamGroupName!=nullptr, 則會填入 "PoUsr." + PolicyId;
   bool GetPolicy(const fon9::auth::AuthResult& authr, PolicyConfig& res, fon9::CharVector* teamGroupName);
   void MakeGridView(fon9::RevBuffer& rbuf, const PolicyConfig& val);
};

} // namespaces
#endif//__f9omstw_OmsPoUserRightsAgent_hpp__
