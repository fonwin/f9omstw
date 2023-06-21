// \file f9omstw/OmsPoRequest.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsPoRequest_hpp__
#define __f9omstw_OmsPoRequest_hpp__
#include "f9omstw/OmsRequestPolicy.hpp"
#include "f9omstw/OmsPoUserRightsAgent.hpp"
#include "fon9/FlowCounter.hpp"

namespace f9omstw {

class OmsPoRequestHandler;
using OmsPoRequestHandlerSP = fon9::intrusive_ptr<OmsPoRequestHandler>;

/// 取得「下單要求」需要的 OmsRequestPolicySP;
/// - 當 PolicyConfig_.IvList_.PolicyItem_ 有異動時, 主動更新(重建) RequestPolicy_;
class OmsPoRequestHandler : public fon9::intrusive_ref_counter<OmsPoRequestHandler> {
   fon9_NON_COPY_NON_MOVE(OmsPoRequestHandler);
   void SubrIvListChanged();
   bool CheckReloadIvList();
   void RebuildRequestPolicy(OmsResource& res);

   char                 Padding____[4];
   fon9::SubConn        PoIvListSubr_{};
   OmsRequestPolicyCfg  PolicyConfig_;

protected:
   /// 預設 do nothing.
   virtual void OnRequestPolicyUpdated(OmsRequestPolicySP before);

public:
   OmsRequestPolicySP      RequestPolicy_;
   const OmsCoreSP         Core_;
   const fon9::CharVector  UserId_;
   const fon9::CharVector  AuthzId_; // 不會為空,至少會與UserId_相同;

   OmsPoRequestHandler(const OmsRequestPolicyCfg& cfg, OmsCoreSP core, const fon9::StrView& userId, const fon9::StrView& authzId)
      : PolicyConfig_(cfg)
      , Core_{std::move(core)}
      , UserId_{userId}
      , AuthzId_{authzId} {
      assert(!authzId.empty() || authzId == userId);
   }
   virtual ~OmsPoRequestHandler();

   virtual void ClearResource();

   /// - 透過 handler->Core_ 建立 handler->RequestPolicy_;
   /// - 並且訂閱 PolicyConfig_.IvList_.PolicyItem_->AfterChangedEvent_;
   ///   當 IvList 有異動時, 會主動重建 handler->RequestPolicy_;
   ///
   /// \retval false  handler->Core_ == nullptr; 或重複呼叫;
   /// \retval true   透過 handler->Core_->RunCoreTask() 建立 handler->RequestPolicy_;
   ///                返回前可能尚未建立成功; 也有可能已建立, 並已呼叫 handler->OnRequestPolicyUpdated();
   friend bool OmsPoRequestHandler_Start(OmsPoRequestHandlerSP handler);
};

//--------------------------------------------------------------------------//

/// 建立「下單要求」之後, 還需要依照底下步驟處理:
/// - OmsPoRequestConfig.UpdateIvDenys(pol); 更新 pol->IvDenys_;
///   因為可能盤中臨時禁止某 user 部分下單權限;
/// - 設定 LgOut 下單線路群組: OmsRequestTrade.LgOut_ = OmsPoRequestConfig.UserRights_.LgOut_;
/// - 流量管制:
///   \code
///   if (fon9_LIKELY(OmsRequestTrade.FcReq_.Fetch().GetOrigValue() <= 0)) {
///      static_cast<OmsRequestTrade*>(runner.Request_.get())->SetPolicy(std::move(pol));
///      if (fon9_LIKELY(OmsCore.MoveToCore(std::move(runner))))
///         return;
///      ...
///   }
///   else {
///      if (++OmsRequestTrade.FcReqOverCount_ > kFcOverCountForceLogout) {
///         // 超過流量, 若超過次數超過 n 則強制斷線.
///         // 避免有惡意連線, 大量下單壓垮系統.
///         runner.RequestAbandon(nullptr, OmsErrCode_OverFlowControl, "FlowControl: ForceLogout.");
///         ClientSession.ForceLogout();
///      }
///      else {
///         runner.RequestAbandon(nullptr, OmsErrCode_OverFlowControl);
///      }
///   }
///   \endcode
struct OmsPoRequestConfig : OmsRequestPolicyCfg {
   using OmsPoUserDenys = OmsPoUserDenysAgent::PolicyConfig;

   fon9::FlowCounter FcReq_;
   unsigned          FcReqOverCount_{};
   OmsIvRight        OrigIvDenys_;
   char              Padding___[3];
   OmsPoUserDenys    PoIvDenys_;

   fon9::intrusive_ptr<OmsPoUserRightsAgent> LoadPoUserRights(const fon9::auth::AuthResult& authr);
   fon9::intrusive_ptr<OmsPoIvListAgent> LoadPoIvList(const fon9::auth::AuthResult& authr);
   void UpdateIvDenys(const OmsRequestPolicy* pol) {
      if (fon9_UNLIKELY(this->PoIvDenys_.IsPolicyChanged())) {
         if (fon9_LIKELY(pol)) {
            OmsPoUserDenysAgent::RegetPolicy(this->PoIvDenys_);
            pol->SetIvDenys(this->PoIvDenys_.IvDenys_ | this->OrigIvDenys_);
         }
      }
   }
};

} // namespaces
#endif//__f9omstw_OmsPoRequest_hpp__
