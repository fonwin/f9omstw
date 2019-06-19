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
   (void)ver; assert(ver == 0);
   ar(rec.AllowOrdTeams_,
      rec.FcRequest_.Count_,
      rec.FcRequest_.IntervalMS_,
      rec.FcQuery_.Count_,
      rec.FcQuery_.IntervalMS_
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

   void LoadPolicy(fon9::DcQueue& buf) override {
      PolicySerialize(fon9::BitvInArchive{buf}, *this);
   }
   void SavePolicy(fon9::RevBuffer& rbuf) override {
      PolicySerialize(fon9::BitvOutArchive{rbuf}, *this);
   }
};

//--------------------------------------------------------------------------//

static fon9::seed::Fields MakeOmsUserRightsFields() {
   fon9::seed::Fields fields;
   fields.Add(fon9_MakeField(OmsPoUserRightsPolicy, AllowOrdTeams_,         "OrdTeams"));
   fields.Add(fon9_MakeField(OmsPoUserRightsPolicy, FcRequest_.Count_,      "FcReqCount"));
   fields.Add(fon9_MakeField(OmsPoUserRightsPolicy, FcRequest_.IntervalMS_, "FcReqMS"));
   fields.Add(fon9_MakeField(OmsPoUserRightsPolicy, FcQuery_.Count_,        "FcQryCount"));
   fields.Add(fon9_MakeField(OmsPoUserRightsPolicy, FcQuery_.IntervalMS_,   "FcQryMS"));
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

} // namespaces
