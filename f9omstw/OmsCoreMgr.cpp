// \file f9omstw/OmsCoreMgr.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsEventSessionSt.hpp"
#include "f9omstw/OmsRequestFactory.hpp"

namespace f9omstw {

void OmsSetRequestLgOut_UseIvac(OmsResource& res, OmsRequestTrade& req, OmsOrder& order) {
   // 可能在收單程序(例如:OmsRcServer)就已填妥.
   if (IsValidateLgOut(req.LgOut_))
      return;
   if (OmsIvBase* ivr = FetchScResourceIvr(res, order)) {
      if (ivr->IvKind_ == OmsIvKind::Subac)
         ivr = ivr->Parent_.get();
      assert(dynamic_cast<OmsIvac*>(ivr) != nullptr);
      req.LgOut_ = static_cast<OmsIvac*>(ivr)->LgOut_;
   }
}
//--------------------------------------------------------------------------//
fon9::seed::TreeSP  OmsCoreMgr::CurrentCoreSapling::GetSapling() {
   return this->Sapling_;
}
//--------------------------------------------------------------------------//
OmsCoreMgr::OmsCoreMgr(FnSetRequestLgOut fnSetRequestLgOut)
   : base{"coremgr"/*tabName*/}
   , CurrentCoreSapling_{*new CurrentCoreSapling{"CurrentCore"}}
   , FnSetRequestLgOut_{fnSetRequestLgOut} {
   this->Add(this->ErrCodeActSeed_ = new ErrCodeActSeed("ErrCodeAct"));
   this->Add(&this->CurrentCoreSapling_);
}
OmsCoreSP OmsCoreMgr::RemoveCurrentCore() {
   if (OmsCoreSP cur = std::move(this->CurrentCore_)) {
      Locker locker{this->Container_};
      if (this->CurrentCore_.get() != nullptr)
         return nullptr;
      this->CurrentCoreSapling_.Sapling_.reset();
      this->CurrentCoreSapling_.SetTitle(cur->Name_ + ": Removed");
      auto ifind = locker->find(&cur->Name_);
      if (ifind != locker->end())
         locker->erase(ifind);
      cur->OnParentTreeClear(*this);
      return cur;
   }
   return nullptr;
}
void OmsCoreMgr::OnMaTree_AfterClear() {
   ConstLocker locker{this->Container_};
   this->CurrentCore_.reset();
}
void OmsCoreMgr::OnMaTree_AfterAdd(Locker& treeLocker, fon9::seed::NamedSeed& seed) {
   static_assert(std::is_base_of<fon9::seed::NamedSeed, OmsResource>::value, "OmsResource is not derived from NamedSeed?");
   OmsResource* coreResource = dynamic_cast<OmsResource*>(&seed);
   if (coreResource == nullptr)
      return;
   // 如果真有必要重新啟動 TDay core, 則使用 TDay + SecondOfDay.
   // OmsCore::Start() 設定 TDay 的地方也要略為調整.
   if (this->CurrentCore_ && this->CurrentCore_->TDay() >= coreResource->TDay())
      return;
   OmsCoreSP old = std::move(this->CurrentCore_);
   this->CurrentCore_.reset(&coreResource->Core_);
   if (this->IsTDayChanging_)
      return;
   this->IsTDayChanging_ = true;
   for (;;) {
      OmsCoreSP cur = this->CurrentCore_;
      if (cur->CoreSt() < OmsCoreSt::CurrentCoreReady)
         cur->SetCoreSt(OmsCoreSt::CurrentCoreReady);
      if (old)
         old->SetCoreSt(OmsCoreSt::Disposing);
      treeLocker.unlock();

      if (this->RequestFactoryPark_) { // 某些特殊應用(例: UintTest), 沒有 RequestFactoryPark;
         for (unsigned L = 0;; ++L) {
            auto* fac = this->RequestFactoryPark_->GetFactory(L);
            if (!fac)
               break;
            if (fac->RunStep_) // 某些特殊應用(例: UintTest), 沒有 RunStep;
               fac->RunStep_->OnCurrentCoreChanged(*cur);
         }
      }
      this->TDayChangedEvent_.Publish(*cur);
      if (old)
         this->Remove(&old->Name_);

      treeLocker.lock();
      if (cur == this->CurrentCore_)
         break;
      // 在 Publish() 或 Remove(old) 期間, CurrentCore 又改變了?!
      // 即使這裡一輩子也用不到, 但也不能不考慮.
      old = std::move(cur);
   }
   this->CurrentCoreSapling_.Sapling_ = this->CurrentCore_->GetSapling();
   this->CurrentCoreSapling_.SetTitle(this->CurrentCore_->Name_);
   this->CurrentCoreSapling_.SetDescription(this->CurrentCore_->GetDescription());
   this->IsTDayChanging_ = false;
}
void OmsCoreMgr::UpdateSc(OmsRequestRunnerInCore& runner) {
   (void)runner;
}
void OmsCoreMgr::RecalcSc(OmsResource& resource, OmsOrder& order) {
   (void)resource; (void)order;
}

void OmsCoreMgr::OnEventInCore(OmsResource& resource, OmsEvent& omsEvent, fon9::RevBuffer& rbuf) {
   if (const OmsEventSessionSt* evSesSt = dynamic_cast<const OmsEventSessionSt*>(&omsEvent)) {
      this->OnEventSessionSt(resource, *evSesSt, &rbuf, nullptr);
   }
}
void OmsCoreMgr::ReloadEvent(OmsResource& resource, const OmsEvent& omsEvent, const OmsBackend::Locker& reloadItems) {
   if (const OmsEventSessionSt* evSesSt = dynamic_cast<const OmsEventSessionSt*>(&omsEvent)) {
      this->OnEventSessionSt(resource, *evSesSt, nullptr, &reloadItems);
   }
   this->OmsEvent_.Publish(resource, omsEvent, &reloadItems);
}
void OmsCoreMgr::OnEventSessionSt(OmsResource& resource, const OmsEventSessionSt& evSesSt,
                                  fon9::RevBuffer* rbuf, const OmsBackend::Locker* isReload) {
   resource.Core_.OnEventSessionSt(evSesSt, isReload);
   this->OmsSessionStEvent_.Publish(resource, evSesSt, rbuf, isReload);
}

} // namespaces
