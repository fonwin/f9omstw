// \file f9omstw/OmsPoUserRightsAgent.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsPoUserRightsAgent.hpp"
#include "fon9/auth/PolicyTree.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/BitvArchive.hpp"

namespace f9omstw {

struct OmsPoUserRightsPolicy;

template <class Archive>
static void SerializeVer(Archive& ar, fon9::ArchiveWorker<Archive, OmsPoUserRightsPolicy>& rec, unsigned ver) {
   (void)ver;
   // ver = 0, 原本包含 unsigned vFcQueryCount = 0, vFcQueryMS = 0; 這2個欄位,
   // 但此功能應由 SeedVisitor: PoAcl.FcQry* 處理.
   unsigned vFcQueryCount = 0, vFcQueryMS = 0;
   ar(rec.AllowOrdTeams_,
      rec.FcRequest_.FcCount_,
      rec.FcRequest_.FcTimeMS_,
      vFcQueryCount, // 佔位用, 以後若有新增數字欄位可拿來用.
      vFcQueryMS,    // 佔位用, 以後若有新增數字欄位可拿來用.
      rec.LgOut_,
      rec.Flags_,
      rec.IvDenys_
      );
}

template <class Archive>
static void PolicySerialize(const Archive& ar, fon9::ArchiveWorker<Archive, OmsPoUserRightsPolicy>& rec) {
   fon9::CompositeVer<decltype(rec)> vrec{rec, 0};
   ar(vrec);
}

struct OmsPoUserRightsPolicy : public fon9::auth::PolicyItem, public OmsUserRights {
   fon9_NON_COPY_NON_MOVE(OmsPoUserRightsPolicy);
   using base = fon9::auth::PolicyItem;
   using base::base;

   void LoadPolicyFromSyn(fon9::DcQueue& buf) override {
      const auto flagsBf = this->Flags_;
      const auto teamsBf = this->AllowOrdTeams_;
      this->LoadPolicy(buf);
      if (!IsEnumContains(flagsBf, OmsUserRightFlag::AllowOrdTeamsSyn)
       || !IsEnumContains(this->Flags_, OmsUserRightFlag::AllowOrdTeamsSyn)) {
         this->Flags_ -= OmsUserRightFlag::AllowOrdTeamsSyn;
         this->AllowOrdTeams_ = std::move(teamsBf);
      }
   }
   void LoadPolicy(fon9::DcQueue& buf) override {
      PolicySerialize(fon9::InArchiveClearBack<fon9::BitvInArchive>{buf}, *this);
   }
   void SavePolicy(fon9::RevBuffer& rbuf) override {
      PolicySerialize(fon9::BitvOutArchive{rbuf}, *this);
   }
};

//--------------------------------------------------------------------------//

static fon9::seed::Fields MakeOmsUserRightsFields() {
   fon9::seed::Fields fields;
   fields.Add(fon9_MakeField(OmsPoUserRightsPolicy, AllowOrdTeams_,       "OrdTeams"));
   fields.Add(fon9_MakeField(OmsPoUserRightsPolicy, FcRequest_.FcCount_,  "FcReqCount"));
   fields.Add(fon9_MakeField(OmsPoUserRightsPolicy, FcRequest_.FcTimeMS_, "FcReqMS"));
   fields.Add(fon9_MakeField2(OmsPoUserRightsPolicy, LgOut));
   fields.Add(fon9_MakeField2(OmsPoUserRightsPolicy, Flags));
   fields.Add(fon9_MakeField2(OmsPoUserRightsPolicy, IvDenys));
   return fields;
}

class OmsPoUserRightsTree : public fon9::auth::PolicyTree {
   fon9_NON_COPY_NON_MOVE(OmsPoUserRightsTree);
   using base = fon9::auth::PolicyTree;

protected:
   fon9::auth::PolicyItemSP MakePolicy(const fon9::StrView& policyId) override {
      return fon9::auth::PolicyItemSP{new OmsPoUserRightsPolicy{policyId}};
   }

public:
   OmsPoUserRightsTree()
      : base("Rights", "PolicyId", MakeOmsUserRightsFields(),
             fon9::seed::TabFlag::Writable | fon9::seed::TabFlag::NoSapling | fon9::seed::TabFlag::NoSeedCommand) {
   }

   using PolicyConfig = OmsPoUserRightsAgent::PolicyConfig;
   bool GetPolicy(const fon9::StrView& policyId, PolicyConfig& res) const {
      using ConstLocker = fon9::auth::PolicyMaps::ConstLocker;
      ConstLocker container{this->PolicyMaps_};
      auto        ifind = container->ItemMap_.find(policyId);
      if (ifind == container->ItemMap_.end())
         return false;
      res = *static_cast<const OmsPoUserRightsPolicy*>(ifind->get());
      return true;
   }
};
//--------------------------------------------------------------------------//
OmsPoUserRightsAgent::OmsPoUserRightsAgent(fon9::seed::MaTree* authMgrAgents, std::string name)
   : base(new OmsPoUserRightsTree{}, std::move(name)) {
   (void)authMgrAgents;
}

bool OmsPoUserRightsAgent::GetPolicy(const fon9::auth::AuthResult& authr, PolicyConfig& res, fon9::CharVector* teamGroupName) {
   fon9::StrView policyId = authr.GetPolicyId(&this->Name_);
   if (!static_cast<OmsPoUserRightsTree*>(this->Sapling_.get())->GetPolicy(policyId, res)) {
      if (teamGroupName)
         teamGroupName->clear();
      return false;
   }
   if (teamGroupName) {
      teamGroupName->assign("PoUsr.");
      teamGroupName->append(policyId);
   }
   return true;
}
void OmsPoUserRightsAgent::MakeGridView(fon9::RevBuffer& rbuf, const PolicyConfig& val) {
   OmsPoUserRightsPolicy pol{fon9::StrView{}};
   *static_cast<PolicyConfig*>(&pol) = val;
   using namespace fon9::seed;
   auto* gvLayout = this->Sapling_->LayoutSP_.get();
   auto* gvTab = gvLayout->GetTab(0);
   FieldsCellRevPrint(gvTab->Fields_, SimpleRawRd{pol}, rbuf, GridViewResult::kCellSplitter);
   // 沒有 key field, 所以把最前方的「cell 分隔字元」, 改成「row 分隔字元」
   *const_cast<char*>(rbuf.GetCurrent()) = GridViewResult::kRowSplitter;
   FieldsNameRevPrint(nullptr, *gvTab, rbuf);
}
//--------------------------------------------------------------------------//
struct OmsPoUserDenysPolicy;

template <class Archive>
static void SerializeVer(Archive& ar, fon9::ArchiveWorker<Archive, OmsPoUserDenysPolicy>& rec, unsigned ver) {
   (void)ver;
   ar(rec.IvDenys_);
}

template <class Archive>
static void PolicySerialize(const Archive& ar, fon9::ArchiveWorker<Archive, OmsPoUserDenysPolicy>& rec) {
   fon9::CompositeVer<decltype(rec)> vrec{rec, 0};
   ar(vrec);
}

struct OmsPoUserDenysPolicy : public fon9::auth::PolicyItem, public OmsUserDenys {
   fon9_NON_COPY_NON_MOVE(OmsPoUserDenysPolicy);
   using base = fon9::auth::PolicyItem;
   using base::base;

   void LoadPolicy(fon9::DcQueue& buf) override {
      PolicySerialize(fon9::InArchiveClearBack<fon9::BitvInArchive>{buf}, *this);
   }
   void SavePolicy(fon9::RevBuffer& rbuf) override {
      PolicySerialize(fon9::BitvOutArchive{rbuf}, *this);
   }
};

//--------------------------------------------------------------------------//

static fon9::seed::Fields MakeOmsUserDenysFields() {
   fon9::seed::Fields fields;
   fields.Add(fon9_MakeField2(OmsPoUserDenysPolicy, IvDenys));
   return fields;
}

class OmsPoUserDenysTree : public fon9::auth::PolicyTree {
   fon9_NON_COPY_NON_MOVE(OmsPoUserDenysTree);
   using base = fon9::auth::PolicyTree;

protected:
   fon9::auth::PolicyItemSP MakePolicy(const fon9::StrView& policyId) override {
      return fon9::auth::PolicyItemSP{new OmsPoUserDenysPolicy{policyId}};
   }

public:
   OmsPoUserDenysTree()
      : base("Denys", "UserId", MakeOmsUserDenysFields(),
             fon9::seed::TabFlag::Writable | fon9::seed::TabFlag::NoSapling | fon9::seed::TabFlag::NoSeedCommand) {
   }

   using PolicyConfig = OmsPoUserDenysAgent::PolicyConfig;
   bool FetchPolicy(const fon9::StrView& userId, PolicyConfig& res) {
      using Locker = fon9::auth::PolicyMaps::Locker;
      Locker container{this->PolicyMaps_};
      auto   ifind = container->ItemMap_.find(userId);
      if (ifind == container->ItemMap_.end())
         ifind = container->ItemMap_.insert(this->MakePolicy(userId)).first;
      *static_cast<OmsUserDenys*>(&res) = *static_cast<const OmsPoUserDenysPolicy*>(ifind->get());
      res.ResetPolicyItem(**ifind);
      return true;
   }
   static void RegetPolicy(PolicyConfig& res) {
      // 因為 OmsUserDenys 只有純資料, 直接複製即可, 不用 lock.
      *static_cast<OmsUserDenys*>(&res) = *static_cast<const OmsPoUserDenysPolicy*>(res.PolicyItem_.get());
      res.PolicyChangedCount_ = res.PolicyItem_.get()->ChangedCount();
   }
};
//--------------------------------------------------------------------------//
OmsPoUserDenysAgent::OmsPoUserDenysAgent(fon9::seed::MaTree* authMgrAgents, std::string name)
   : base(new OmsPoUserDenysTree{}, std::move(name)) {
   (void)authMgrAgents;
}
bool OmsPoUserDenysAgent::FetchPolicy(const fon9::auth::AuthResult& authr, PolicyConfig& res) {
   return static_cast<OmsPoUserDenysTree*>(this->Sapling_.get())->FetchPolicy(authr.GetUserId(), res);
}
void OmsPoUserDenysAgent::RegetPolicy(PolicyConfig& res) {
   OmsPoUserDenysTree::RegetPolicy(res);
}

} // namespaces
