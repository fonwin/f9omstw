﻿// \file f9omstw/OmsPoIvListAgent.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsPoIvListAgent.hpp"
#include "fon9/auth/PolicyMaster.hpp"
#include "fon9/auth/PolicyAcl.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/BitvArchive.hpp"

namespace fon9 { namespace seed {

fon9::StrView OmsIvKeyField::GetTypeId(fon9::NumOutBuf&) const {
   return fon9::StrView{fon9_kCSTR_UDStrFieldMaker_Head fon9_kCSTR_UDFieldMaker_OmsIvKey};
}

// static fon9::seed::FieldSP OmsIvKeyFieldMaker(fon9::StrView& fldcfg, char chSpl, char chTail) {
//    fon9::Named named{DeserializeNamed(fldcfg, chSpl, chTail)};
//    if (named.Name_.empty())
//       return fon9::seed::FieldSP{};
//    using OmsIvKeyField_Dy = ;
//    return fon9::seed::FieldSP{new OmsIvKeyField_Dy(std::move(named), fon9::seed::FieldType::Chars)};
// }
// static fon9::seed::FieldMakerRegister reg{fon9::StrView{fon9_kCSTR_UDFieldMaker_OmsIvKey}, &OmsIvKeyFieldMaker};

} } // namespace fon9::seed;

//--------------------------------------------------------------------------//

namespace f9omstw {

/// 用來存取 OmsIvList 的 layout.
/// - KeyField  = OmsIvKey;
/// - Tab[0]    = "IvRights";
/// - Fields[0] = "Rights";
/// - Fields[1] = "LgOut";
fon9::seed::LayoutSP MakeOmsIvListLayout() {
   using namespace fon9::seed;
   Fields fields;
   fields.Add(fon9_MakeField(OmsIvList::value_type, second.Rights_, "Rights"));
   fields.Add(fon9_MakeField(OmsIvList::value_type, second.LgOut_,  "LgOut"));

   return LayoutSP{new Layout1(fon9_MakeField(OmsIvList::value_type, first, "IvKey"),
            TreeFlag::AddableRemovable,
            new Tab{fon9::Named{"IvRights"}, std::move(fields), TabFlag::NoSapling | TabFlag::Writable}
            )};
}
//--------------------------------------------------------------------------//
void ToBitv(fon9::RevBuffer& rbuf, const OmsIvConfig& v) {
   ToBitv(rbuf, v.LgOut_);
   ToBitv(rbuf, v.Rights_);
}

class PolicyIvListTree : public fon9::auth::MasterPolicyTree {
   fon9_NON_COPY_NON_MOVE(PolicyIvListTree);
   using base = fon9::auth::MasterPolicyTree;

   struct KeyMakerOmsIvKey {
      static OmsIvKey StrToKey(const fon9::StrView& keyText) {
         return OmsIvKey{keyText};
      }
   };
   using DetailTableImpl = OmsIvList;
   using DetailTree = fon9::auth::DetailPolicyTreeTable<DetailTableImpl, KeyMakerOmsIvKey>;
   using DetailTable = DetailTree::DetailTable;

   struct MasterItem : public fon9::auth::MasterPolicyItem {
      fon9_NON_COPY_NON_MOVE(MasterItem);
      using base = fon9::auth::MasterPolicyItem;
      MasterItem(const fon9::StrView& policyId, fon9::auth::MasterPolicyTreeSP owner)
         : base(policyId, std::move(owner)) {
         this->DetailSapling_.reset(new DetailTree{*this});
      }
      void LoadPolicy(fon9::DcQueue& buf) override {
         unsigned ver = 0;
         fon9::BitvTo(buf, ver);
         DetailTable::Locker pmap{static_cast<DetailTree*>(this->DetailSapling())->DetailTable_};
         pmap->clear();
         if (size_t sz = BitvToContainerElementCount(buf)) {
            ContainerReserve(*pmap, sz);
            OmsIvKey    key;
            OmsIvConfig value;
            do {
               BitvTo(buf, key);
               BitvTo(buf, value.Rights_);
               if (ver > 0)
                  BitvTo(buf, value.LgOut_);
               pmap->insert(pmap->end(), DetailTableImpl::value_type{key, value});
            } while (--sz > 0);
         }
      }
      void SavePolicy(fon9::RevBuffer& rbuf) override {
         const unsigned ver = 1;
         DetailTable::ConstLocker pmap{static_cast<DetailTree*>(this->DetailSapling())->DetailTable_};
         fon9::BitvOutArchive{rbuf}(ver, *pmap);
      }
   };
   using MasterItemSP = fon9::intrusive_ptr<MasterItem>;

   fon9::auth::PolicyItemSP MakePolicy(const fon9::StrView& policyId) override {
      return fon9::auth::PolicyItemSP{new MasterItem(policyId, this)};
   }

   static fon9::seed::LayoutSP MakeLayout() {
      using namespace fon9::seed;
      return new Layout1(fon9_MakeField2(fon9::auth::PolicyItem, PolicyId),
                         new Tab(fon9::Named{"IvList"}, Fields{}, MakeOmsIvListLayout(), TabFlag::Writable | TabFlag::HasSapling),
                         TreeFlag::AddableRemovable);
   }

public:
   PolicyIvListTree() : base{MakeLayout()} {
   }

   using PolicyConfig = OmsPoIvListAgent::PolicyConfig;
   bool GetPolicy(const fon9::StrView& policyId, PolicyConfig& res) const {
      struct ResultHandler {
         PolicyConfig* Result_;
         void InLocking(const fon9::auth::PolicyItem& master) {
            this->Result_->ResetPolicyItem(master);
         }
         void OnUnlocked(fon9::auth::DetailPolicyTree& detailTree) {
            DetailTable::Locker pmap{static_cast<DetailTree*>(&detailTree)->DetailTable_};
            *static_cast<OmsIvList*>(this->Result_) = *pmap;
         }
      };
      return base::GetPolicy(policyId, ResultHandler{&res});
   }

   static void RegetPolicy(PolicyConfig& res) {
      assert(dynamic_cast<const MasterItem*>(res.PolicyItem_.get()) != nullptr);
      const MasterItem* masterItem = static_cast<const MasterItem*>(res.PolicyItem_.get());
      if (DetailTree* detailSapling = static_cast<DetailTree*>(masterItem->DetailSapling())) {
         DetailTable::Locker  pmap{detailSapling->DetailTable_};
         *static_cast<OmsIvList*>(&res) = *pmap;
         res.PolicyChangedCount_ = masterItem->ChangedCount();
      }
      else {
         res.clear();
         res.PolicyChangedCount_ = masterItem->ChangedCount();
      }
   }
};
//--------------------------------------------------------------------------//
OmsPoIvListAgent::OmsPoIvListAgent(fon9::seed::MaTree* authMgrAgents, std::string name)
   : base(new PolicyIvListTree{}, std::move(name))
   , PoAclAgent_{authMgrAgents->Get<fon9::auth::PolicyAclAgent>(fon9_kCSTR_PolicyAclAgent_Name)} {
   if (this->PoAclAgent_)
      this->PoAclConn_ = this->PoAclAgent_->PoAclAdjusters_.Subscribe(
         std::bind(&OmsPoIvListAgent::PoAclAdjust, this, std::placeholders::_1, std::placeholders::_2));
}
OmsPoIvListAgent::~OmsPoIvListAgent() {
   if (this->PoAclAgent_)
      this->PoAclAgent_->PoAclAdjusters_.Unsubscribe(&this->PoAclConn_);
}
void OmsPoIvListAgent::PoAclAdjust(const fon9::auth::AuthResult& authr, fon9::auth::PolicyAclAgent::PolicyConfig& res) {
   PolicyConfig ivList;
   if (!this->GetPolicy(authr, ivList) || ivList.empty())
      return;
   std::vector<fon9::CharVector> ivKeys;
   ivKeys.resize(ivList.size());
   size_t idx = 0;
   for (const auto& i : ivList) {
      const OmsIvRight rights = i.second.Rights_;
      if ((rights & OmsIvRight::DenyTradingAll) == OmsIvRight::DenyTradingAll
          && !IsEnumContains(rights, OmsIvRight::AllowAddReport)
          && !IsEnumContains(rights, OmsIvRight::AllowSubscribeReport)
          && !IsEnumContains(rights, OmsIvRight::IsAdmin)) {
         // 沒權限.
         continue;
      }
      ivKeys[idx++] = i.first.ToShortStr('/');
   }
   if (idx == 0)
      return;
   ivKeys.resize(idx);

   const fon9::StrView    kIvac{"{Ivac}"};
   fon9::seed::AccessList acl;
   for (const auto& v : res.Acl_) {
      fon9::StrView aclKey{ToStrView(v.first)};
      const char*   pfind = aclKey.Find(*kIvac.begin());
      const auto    ptailSz = aclKey.end() - pfind - fon9::signed_cast(kIvac.size());
      if (pfind && ptailSz >= 0 && memcmp(pfind + 1, kIvac.begin() + 1, kIvac.size() - 1) == 0) {
         for (const fon9::CharVector& ikey : ivKeys) {
            fon9::CharVector nkey;
            char* pkey = static_cast<char*>(nkey.alloc(aclKey.size() - kIvac.size() + ikey.size()));
            auto  sz = static_cast<size_t>(pfind - aclKey.begin());
            memcpy(pkey, aclKey.begin(), sz);
            memcpy(pkey += sz, ikey.cbegin(), ikey.size());
            memcpy(pkey += ikey.size(), pfind + kIvac.size(), fon9::unsigned_cast(ptailSz));
            acl.kfetch(nkey).second = v.second;
         }
      }
      else {
         acl.insert(v);
      }
   }
   res.Acl_ = std::move(acl);
}

static void IvList_ReplaceUserId(OmsIvList& dst, fon9::StrView userId, fon9::StrView authzId) {
   static const fon9::StrView kOldStrA[] = {"UserId}", "Authz}"};
   const fon9::StrView        newStrA[] = {userId,     authzId.empty() ? userId : authzId};
   const OmsIvList   src = std::move(dst);
   for (auto& v : src) {
      dst.kfetch(ToStrView(fon9::CharVectorReplaceA(ToStrView(v.first), '{', kOldStrA, newStrA, fon9::numofele(kOldStrA)))).second = v.second;
   }
}
bool OmsPoIvListAgent::GetPolicy(const fon9::auth::AuthResult& authr, PolicyConfig& res) {
   if (!static_cast<PolicyIvListTree*>(this->Sapling_.get())->GetPolicy(authr.GetPolicyId(&this->Name_), res))
      return false;
   IvList_ReplaceUserId(res, ToStrView(authr.AuthcId_), authr.GetUserIdForAuthz());
   return true;
}
void OmsPoIvListAgent::RegetPolicy(PolicyConfig& res, fon9::StrView userId, fon9::StrView authzId) {
   PolicyIvListTree::RegetPolicy(res);
   IvList_ReplaceUserId(res, userId, authzId);
}
void OmsPoIvListAgent::MakeGridView(fon9::RevBuffer& rbuf, const OmsIvList& ivList) {
   auto* gvLayout = this->Sapling_->LayoutSP_->GetTab(0)->SaplingLayout_.get();
   auto* gvTab = gvLayout->GetTab(0);
   fon9::seed::SimpleMakeFullGridView(ivList, *gvTab, rbuf);
   fon9::seed::FieldsNameRevPrint(gvLayout, *gvTab, rbuf);
}

} // namespaces
