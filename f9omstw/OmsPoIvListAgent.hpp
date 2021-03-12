/// \file f9omstw/OmsPoIvListAgent.hpp
/// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsPoIvListAgent_hpp__
#define __f9omstw_OmsPoIvListAgent_hpp__
#include "f9omstw/OmsPoIvList.hpp"
#include "fon9/auth/PolicyAcl.hpp"
#include "fon9/seed/FieldString.hpp"

namespace fon9 { namespace seed {

class OmsIvKeyField : public FieldString<f9omstw::OmsIvKey::BaseStrT> {
   fon9_NON_COPY_NON_MOVE(OmsIvKeyField);
   using base = FieldString<f9omstw::OmsIvKey::BaseStrT>;
public:
   using base::base;

   #define fon9_kCSTR_UDFieldMaker_OmsIvKey  "IvKey"
   fon9::StrView GetTypeId(fon9::NumOutBuf&) const override;
};
inline FieldSPT<OmsIvKeyField> MakeField(fon9::Named&& named, int32_t ofs, f9omstw::OmsIvKey&) {
   return FieldSPT<OmsIvKeyField>{new OmsIvKeyField{std::move(named), ofs}};
}
inline FieldSPT<OmsIvKeyField> MakeField(fon9::Named&& named, int32_t ofs, const f9omstw::OmsIvKey&) {
   return FieldSPT<OmsIvKeyField>{new FieldConst<OmsIvKeyField>{std::move(named), ofs}};
}

} } // namespace fon9::seed;

namespace f9omstw {

/// 「Oms可用帳號」政策管理.
/// \code
/// AuthMgr/OmsIvList.Layout =
///   { KeyName = "PolicyId",
///     Tab[0] = { TabName = "IvList", Fields[] = {},
///                Sapling.Layout =
///                  { KeyName = "IvKey",
///                    Tab[0] = { TabName = "IvRights", Fields[] = {"Rights"} }
///                  }
///              }
///   }
/// \endcode
/// | PolicyId |
/// |----------|
/// | admin    |
/// | user     |
///
/// (detail):
/// | IvKey          | Rights                  |
/// |----------------|-------------------------|
/// | 8610-0000010   | x0                      |
/// | 8610-0000021   | x0                      |
class OmsPoIvListAgent : public fon9::auth::PolicyAgent {
   fon9_NON_COPY_NON_MOVE(OmsPoIvListAgent);
   using base = fon9::auth::PolicyAgent;
   using PoAclAgentSP = fon9::intrusive_ptr<fon9::auth::PolicyAclAgent>;
   PoAclAgentSP   PoAclAgent_;
   fon9::SubConn  PoAclConn_{};

   void PoAclAdjust(const fon9::auth::AuthResult& authr, fon9::auth::PolicyAclAgent::PolicyConfig& res);

public:
   OmsPoIvListAgent(fon9::seed::MaTree* authMgrAgents, std::string name);
   ~OmsPoIvListAgent();

   #define fon9_kCSTR_OmsPoIvListAgent_Name    "OmsPoIvList"
   /// 在 authMgr.Agents_ 上面新增一個 OmsPoIvListAgent.
   /// \retval nullptr   name 已存在.
   /// \retval !=nullptr 新增成功的 OmsPoIvListAgent 物件, 返回前會先執行 LinkStorage().
   static fon9::intrusive_ptr<OmsPoIvListAgent> Plant(fon9::auth::AuthMgr& authMgr, std::string name = fon9_kCSTR_OmsPoIvListAgent_Name) {
      auto res = authMgr.Agents_->Plant<OmsPoIvListAgent>("OmsPoIvListAgent.Plant", std::move(name));
      if (res)
         res->LinkStorage(authMgr.Storage_, 128);
      return res;
   }

   using PolicyConfig = OmsIvList;
   bool GetPolicy(const fon9::auth::AuthResult& authr, PolicyConfig& res);
   void MakeGridView(fon9::RevBuffer& rbuf, const PolicyConfig& ivList);
};

} // namespaces
#endif//__f9omstw_OmsPoIvListAgent_hpp__
