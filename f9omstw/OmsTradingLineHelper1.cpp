// \file f9omstw/OmsTradingLineHelper1.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsTradingLineHelper1.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsTradingLineMgr.hpp"

namespace f9omstw {

OmsLocalHelpOfferEvHandler1::~OmsLocalHelpOfferEvHandler1() {
   this->UnsubscribeRpt();
}
void OmsLocalHelpOfferEvHandler1::InCore_NotifyToAsker(OmsResource& resource) {
   if (this->IsBusyWorking())
      return;
   for (char chLg : this->AskerLgList()) {
      auto* asker = this->LgMgr_.GetLineMgr(static_cast<LgOut>(chLg), this->Offer_);
      if (fon9_UNLIKELY(asker == nullptr || asker == &this->Offer_))
         continue;
      if (auto* omsAsker = dynamic_cast<OmsTradingLineMgrBase*>(asker)) {
         omsAsker->InCore_SendReqQueue(resource);
      }
      else {
         asker->SendReqQueue(asker->Lock());
      }
      if (this->IsBusyWorking())
         break;
   }
}
void OmsLocalHelpOfferEvHandler1::NotifyToAsker(const TLineLocker* tsvrOffer) {
   (void)tsvrOffer;
   if (auto core = this->CoreMgr_->CurrentCore())
      core->RunCoreTask(std::bind(&OmsLocalHelpOfferEvHandler1::InCore_NotifyToAsker, ThisSP{this}, std::placeholders::_1));
}
void OmsLocalHelpOfferEvHandler1::UnsubscribeRpt() {
   if (this->RptSubr_) {
      this->RptCore_->ReportSubject().Unsubscribe(&this->RptSubr_);
      this->RptCore_.reset();
   }
}
void OmsLocalHelpOfferEvHandler1::OnOrderReport(OmsCore& omsCore, const OmsRxItem& rxItem) {
   (void)omsCore;
   // 注意: 現在是在回報 thread 裡面!
   auto* ordraw = static_cast<const OmsOrderRaw*>(rxItem.CastToOrder());
   if (ordraw == nullptr)
      return;
   if (&ordraw->Request() != this->WorkingRequest_.get())
      return;
   if (ordraw->RequestSt_ < f9fmkt_TradingRequestSt_Done)
      return;
   this->UnsubscribeRpt();
   this->OnWorkingRequestDone();
}
f9fmkt::SendRequestResult OmsLocalHelpOfferEvHandler1::OnAskFor_SendRequest(f9fmkt::TradingRequest& req, const TLineLocker& tsvrAsker) {
   // 這裡提供的是 Helper 的範例, 並沒有要求極致的[協助]低延遲.
   // 如果需要降低延遲, 可從底下方向考慮:
   // - 不要透過 ReportSubject 得知 this->WorkingRequest_ 完成,
   //    - 也許可在 Order 或 Request 建立[異動立即通知]機制.
   //    - 或 OmsCore 另建其他通知機制: 在 OmsCore 交給 Backend 前, 直接在 OmsCore 裡面通知.
   const auto retval = base::OnAskFor_SendRequest(req, tsvrAsker);
   if (this->WorkingRequest_ && !this->RptSubr_) {
      if ((this->RptCore_ = this->CoreMgr_->CurrentCore()).get()) {
         this->RptCore_->ReportSubject().Subscribe(
            &this->RptSubr_,
            std::bind(&OmsLocalHelpOfferEvHandler1::OnOrderReport, ThisSP{this},
                      std::placeholders::_1, std::placeholders::_2));
      }
   }
   return retval;
}
void OmsLocalHelpOfferEvHandler1::ClearWorkingRequests() {
   this->UnsubscribeRpt();
   base::ClearWorkingRequests();
}
//--------------------------------------------------------------------------//
f9fmkt::LocalHelpOfferEvHandlerSP OmsLocalHelperMaker1::MakeHelpOffer(f9fmkt::TradingLgMgrBase& lgMgr, f9fmkt::TradingLineManager& lmgrOffer, const fon9::StrView& name) {
   return new OmsLocalHelpOfferEvHandler1{*this->CoreMgr_, lgMgr, lmgrOffer, name};
}

} // namespaces
