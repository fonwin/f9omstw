// \file f9omstw/OmsRequestTrade.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestRunner.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsTradingLineMgr.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsRequestTrade::MakeFieldsImpl(fon9::seed::Fields& flds) {
   base::MakeFields<OmsRequestTrade>(flds);
   flds.Add(fon9_MakeField2(OmsRequestTrade, SesName));
   flds.Add(fon9_MakeField2(OmsRequestTrade, UserId));
   flds.Add(fon9_MakeField2(OmsRequestTrade, FromIp));
   flds.Add(fon9_MakeField2(OmsRequestTrade, Src));
   flds.Add(fon9_MakeField2(OmsRequestTrade, LgOut));
   flds.Add(fon9_MakeField2(OmsRequestTrade, UsrDef));
   flds.Add(fon9_MakeField2(OmsRequestTrade, ClOrdId));
}

//--------------------------------------------------------------------------//

bool OmsRequestIni::ValidateInUser(OmsRequestRunner& reqRunner) {
   if (this->RxKind_ == f9fmkt_RxKind_RequestNew && !OmsIsOrdNoEmpty(this->OrdNo_)) {
      const OmsRequestPolicy* pol = this->Policy();
      if (pol == nullptr || !pol->IsAllowAnyOrdNo()) {
         reqRunner.RequestAbandon(nullptr, OmsErrCode_OrdNoMustEmpty);
         return false;
      }
   }
   return base::ValidateInUser(reqRunner);
}
void OmsRequestIni::MakeFieldsImpl(fon9::seed::Fields& flds) {
   base::MakeFields<OmsRequestIni>(flds);
   flds.Add(fon9_MakeField2(OmsRequestIni, IvacNo));
   flds.Add(fon9_MakeField2(OmsRequestIni, SubacNo));
   flds.Add(fon9_MakeField2(OmsRequestIni, SalesNo));
   flds.Add(fon9_MakeField2(OmsRequestIni, VaTimeMS));
}
const char* OmsRequestIni::IsIniFieldEqual(const OmsRequestBase& req) const {
   if (auto r = dynamic_cast<const OmsRequestIni*>(&req))
      return this->IsIniFieldEqualImpl(*r);
   return "RequestIni";
}
const char* OmsRequestIni::IsIniFieldEqualImpl(const OmsRequestIni& req) const {
   if (this->BrkId_ != req.BrkId_)
      return "BrkId";
   if (this->IvacNo_ != req.IvacNo_)
      return "IvacNo";
   if (this->SubacNo_ != req.SubacNo_)
      return "SubacNo";
   return nullptr;
}
const OmsRequestIni* OmsRequestIni::BeforeReq_CheckOrdKey(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) {
   if (this->Market_ == f9fmkt_TradingMarket_Unknown) {
      if (!scRes.Symb_) {
         // 因為 this 沒有 SymbolId, 所以應由衍生者先取得 scRes.Symb_;
         // 如果衍生者沒先取得 scRes.Symb_, 就拒絕此次要求.
         runner.RequestAbandon(&res, OmsErrCode_Bad_MarketId_SymbNotFound);
         return nullptr;
      }
      if ((this->Market_ = scRes.Symb_->TradingMarket_) == f9fmkt_TradingMarket_Unknown) {
         runner.RequestAbandon(&res, OmsErrCode_Bad_SymbMarketId);
         return nullptr;
      }
   }
   OmsBrk* brk = res.Brks_->GetBrkRec(ToStrView(this->BrkId_));
   if (fon9_UNLIKELY(brk == nullptr)) {
      runner.RequestAbandon(&res, OmsErrCode_Bad_BrkId);
      return nullptr;
   }
   if (fon9_LIKELY(this->RxKind_ == f9fmkt_RxKind_RequestNew)) {
      // 新單的委託書號是否重複?
      // 在 OmsOrdNoMap::AllocOrdNo() 使用 Reject 方式處理.
      // => 有權限「自訂委託號」的案例不多, 再加上「自編卻重複」的情況應該更少.
      //    且在 OmsOrdNoMap::AllocOrdNo() 已有使用 Reject 方式處理.
      // => 因此沒必要在此先做檢查.
      // if (memchr(this->OrdNo_.begin(), '\0', this->OrdNo_.size()) == nullptr) {
      //    // 新單有填完整委託書號.
      //    if (const auto* ordNoMap = brk->GetOrdNoMap(*this))
      //       if (ordNoMap->GetOrder(this->OrdNo_) != nullptr) {
      //          runner.RequestAbandon(&res, OmsErrCode_OrderAlreadyExists);
      //          return nullptr;
      //       }
      // }
      return this;
   }
   // 不是新單 => 刪改查要求: 返回要操作的「初始委託要求」.
   if (OmsIsOrdNoEmpty(this->OrdNo_)) {
      runner.RequestAbandon(&res, OmsErrCode_Bad_OrdNo);
      return nullptr;
   }
   if (const auto* ordNoMap = brk->GetOrdNoMap(*this)) {
      if (OmsOrder* ord = ordNoMap->GetOrder(this->OrdNo_)) {
         if (auto ini = ord->Initiator())
            return ini;
         runner.RequestAbandon(&res, OmsErrCode_OrderInitiatorNotFound);
      }
      else {
         runner.RequestAbandon(&res, OmsErrCode_OrderNotFound);
      }
   }
   else {
      runner.RequestAbandon(&res, OmsErrCode_OrdNoMapNotFound);
   }
   return nullptr;
}
OmsIvRight OmsRequestIni::CheckIvRight(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) const {
   assert(dynamic_cast<OmsRequestTrade*>(runner.Request_.get()) != nullptr);
   if (const OmsRequestPolicy* pol = static_cast<OmsRequestTrade*>(runner.Request_.get())->Policy()) {
      if (!scRes.Ivr_)
         scRes.Ivr_ = GetIvr(res, *this);
      OmsIvConfig       ivrConfig;
      const OmsIvRight  ivrRights = pol->GetIvRights(scRes.Ivr_.get(), &ivrConfig);
      const OmsIvRight  ivRightDeny = RxKindToIvRightDeny(runner.Request_->RxKind());
      if (fon9_LIKELY(!IsEnumContains(ivrRights, ivRightDeny))) {
         if (ivrConfig.LgOut_ != LgOut::Unknown)
            static_cast<OmsRequestTrade*>(runner.Request_.get())->LgOut_ = ivrConfig.LgOut_;
         return ivrRights;
      }
      scRes.Ivr_.reset();
   }
   runner.RequestAbandon(&res, OmsErrCode_IvNoPermission);
   return OmsIvRight::DenyAll;
}
bool OmsRequestIni::BeforeReq_CheckIvRight(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) const {
   OmsIvRight  ivrRights = this->CheckIvRight(runner, res, scRes);
   if (fon9_UNLIKELY(ivrRights == OmsIvRight::DenyAll))
      return false;
   if (fon9_LIKELY(runner.Request_->RxKind() == f9fmkt_RxKind_RequestNew))
      return true;
   assert(runner.Request_.get() != this);
   // 委託已存在, 則欄位(Ivr,Side,Symbol,...)必須正確.
   if (const char* pErrField = this->IsIniFieldEqual(*runner.Request_)) {
      runner.RequestAbandon(&res, OmsErrCode_FieldNotMatch, fon9::RevPrintTo<std::string>('\'', pErrField, "' not equal."));
      return false;
   }
   return true;
}
OmsOrderRaw* OmsRequestIni::BeforeReqInCore(OmsRequestRunner& runner, OmsResource& res) {
   assert(this == runner.Request_.get());
   OmsScResource  scRes;
   if (const OmsRequestIni* iniReq = this->BeforeReq_CheckOrdKey(runner, res, scRes)) {
      if (iniReq == this) {
         if (this->BeforeReq_CheckIvRight(runner, res, scRes)) {
            OmsOrderRaw* retval = this->Creator().OrderFactory_->MakeOrder(*this, &scRes);
            retval->UpdateOrderSt_ = f9fmkt_OrderSt_NewStarting;
            return retval;
         }
      }
      else {
         OmsOrder&      order = iniReq->LastUpdated()->Order();
         OmsScResource& ordScRes = order.ScResource();
         ordScRes.CheckMoveFrom(std::move(scRes));
         if (iniReq->BeforeReq_CheckIvRight(runner, res, ordScRes))
            return order.BeginUpdate(*this);
      }
   }
   return nullptr;
}

//--------------------------------------------------------------------------//

void OmsRequestUpd::MakeFieldsImpl(fon9::seed::Fields& flds) {
   flds.Add(fon9_MakeField2(OmsRequestUpd, IniSNO));
   base::MakeFields<OmsRequestUpd>(flds);
}
OmsOrder* OmsRequestUpd::BeforeReqInCore_GetOrder(OmsRequestRunner& runner, OmsResource& res) {
   assert(this == runner.Request_.get());
   if (const OmsRequestIni* iniReq = this->BeforeReq_GetInitiator(runner, res)) {
      OmsOrder& order = iniReq->LastUpdated()->Order();
      if (fon9_LIKELY(order.Initiator()->BeforeReq_CheckIvRight(runner, res))) {
         if (fon9_LIKELY(!f9omstw::OmsIsOrdNoEmpty(order.Tail()->OrdNo_)))
            return &order;
         runner.RequestAbandon(&res, OmsErrCode_Bad_OrdNo);
      }
   }
   return nullptr;
}
OmsOrderRaw* OmsRequestUpd::BeforeReqInCore(OmsRequestRunner& runner, OmsResource& res) {
   if(auto* order = this->BeforeReqInCore_GetOrder(runner, res))
      return order->BeginUpdate(*this);
   return nullptr;
}
bool OmsRequestUpd::PreOpQueuingRequest(fon9::fmkt::TradingLineManager& from) const {
   if (auto* lmgr = dynamic_cast<OmsTradingLineMgrBase*>(&from)) {
      if (OmsRequestRunnerInCore* currRunner = lmgr->CurrRunner()) {
         assert(this == &currRunner->OrderRaw_.Request());
         if (currRunner->OrderRaw_.Order().LastOrderSt() < f9fmkt_OrderSt_NewSending)
            return true;
      }
   }
   return false;
}

} // namespaces
