// \file f9omstw/OmsCoreMgr.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCoreMgr.hpp"

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
OmsCoreMgr::OmsCoreMgr(FnSetRequestLgOut fnSetRequestLgOut)
   : base{"coremgr"/*tabName*/}
   , FnSetRequestLgOut_{fnSetRequestLgOut} {
   this->Add(this->ErrCodeActSeed_ = new ErrCodeActSeed("ErrCodeAct"));
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
      cur->SetCoreSt(OmsCoreSt::CurrentCore);
      if (old)
         old->SetCoreSt(OmsCoreSt::Disposing);
      treeLocker.unlock();

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
   this->IsTDayChanging_ = false;
}
void OmsCoreMgr::UpdateSc(OmsRequestRunnerInCore& runner) {
   (void)runner;
}
void OmsCoreMgr::RecalcSc(OmsResource& resource, OmsOrder& order) {
   (void)resource; (void)order;
}

} // namespaces
