// \file f9omstw/OmsTradingLineHelper1.hpp
// \author fonwinz@gmail.com
#ifndef f9omstw_OmsTradingLineHelper1_hpp__
#define f9omstw_OmsTradingLineHelper1_hpp__
#include "fon9/fmkt/TradingLineHelper1.hpp"
#include "f9omstw/OmsBase.hpp"

namespace f9omstw {
namespace f9fmkt = fon9::fmkt;

class OmsLocalHelpOfferEvHandler1 : public f9fmkt::LocalHelpOfferEvHandler1 {
   fon9_NON_COPY_NON_MOVE(OmsLocalHelpOfferEvHandler1);
   using base = LocalHelpOfferEvHandler1;
   fon9::SubConn  RptSubr_{};
   OmsCoreSP      RptCore_;
   void UnsubscribeRpt();
protected:
   using ThisSP = fon9::intrusive_ptr<OmsLocalHelpOfferEvHandler1>;
   void NotifyToAsker(const TLineLocker* tsvrOffer) override;
   void ClearWorkingRequests() override;
   void OnOrderReport(OmsCore& omsCore, const OmsRxItem& rxItem);
   void InCore_NotifyToAsker(OmsResource& resource);
public:
   OmsCoreMgr& CoreMgr_;
   OmsLocalHelpOfferEvHandler1(OmsCoreMgr& coreMgr, f9fmkt::TradingLgMgrBase& lgMgr, f9fmkt::TradingLineManager& offer, const fon9::StrView& name)
      : base(lgMgr, offer, name)
      , CoreMgr_(coreMgr) {
   }
   ~OmsLocalHelpOfferEvHandler1();
   f9fmkt::SendRequestResult OnAskFor_SendRequest(f9fmkt::TradingRequest& req, const TLineLocker& tsvrAsker) override;
};
//--------------------------------------------------------------------------//
struct OmsLocalHelperMaker1 : public f9fmkt::LocalHelperMaker {
   fon9_NON_COPY_NON_MOVE(OmsLocalHelperMaker1);
   OmsCoreMgr& CoreMgr_;
   OmsLocalHelperMaker1(OmsCoreMgr& coreMgr) : CoreMgr_(coreMgr) {
   }
   f9fmkt::LocalHelpOfferEvHandlerSP MakeHelpOffer(f9fmkt::TradingLgMgrBase& lgMgr, f9fmkt::TradingLineManager& lmgrOffer, const fon9::StrView& name) override;
};
//--------------------------------------------------------------------------//

} // namespaces
#endif//f9omstw_OmsTradingLineHelper1_hpp__
