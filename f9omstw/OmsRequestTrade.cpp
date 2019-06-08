// \file f9omstw/OmsRequestTrade.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestRunner.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsRequestTrade::MakeFieldsImpl(fon9::seed::Fields& flds) {
   base::MakeFields<OmsRequestTrade>(flds);
   flds.Add(fon9_MakeField2(OmsRequestTrade, SesName));
   flds.Add(fon9_MakeField2(OmsRequestTrade, UserId));
   flds.Add(fon9_MakeField2(OmsRequestTrade, FromIp));
   flds.Add(fon9_MakeField2(OmsRequestTrade, Src));
   flds.Add(fon9_MakeField2(OmsRequestTrade, UsrDef));
   flds.Add(fon9_MakeField2(OmsRequestTrade, ClOrdId));
}

//--------------------------------------------------------------------------//

bool OmsRequestIni::ValidateInUser(OmsRequestRunner& reqRunner) {
   if (this->RxKind_ == f9fmkt_RxKind_RequestNew && !this->OrdNo_.empty1st()) {
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
const OmsRequestIni* OmsRequestIni::PreCheck_OrdKey(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) {
   if (this->Market_ == f9fmkt_TradingMarket_Unknown) {
      if (!scRes.Symb_) {
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
      // 在 OmsOrdNoMap::AllocOrdNo() 使用 Reject 方式處理.
      // => 有權限「自訂委託號」的案例不多, 再加上「自編卻重複」的情況應該更少.
      //    且在 OmsOrdNoMap::AllocOrdNo() 已有使用 Reject 方式處理.
      // => 因此沒必要在此先做檢查.
      // if (memchr(this->OrdNo_.begin(), '\0', this->OrdNo_.size()) == nullptr) {
      //    // 新單有填完整委託書號.
      //    if (const auto* ordNoMap = brk->GetOrdNoMap(*this))
      //       if (ordNoMap->GetOrder(this->OrdNo_) != nullptr) {
      //          runner.RequestAbandon(&res, OmsErrCode_OrderAlreadyExists);
      //          return false;
      //       }
      // }
      return this;
   }
   // 不是新單 => 刪改查要求: 返回要操作的「初始委託要求」.
   if (this->OrdNo_.empty1st()) {
      runner.RequestAbandon(&res, OmsErrCode_Bad_OrdNo);
      return nullptr;
   }
   if (const auto* ordNoMap = brk->GetOrdNoMap(*this)) {
      if (OmsOrder* ord = ordNoMap->GetOrder(this->OrdNo_))
         return ord->Initiator_;
      // 找不到「初始委託要求」, 則有可能是「補單的刪改查」, 此時必須要有 OmsIvRight::AllowRequestIni 權限.
   }
   return this;
}
OmsIvRight OmsRequestIni::CheckIvRight(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) const {
   if (const OmsRequestPolicy* pol = runner.Request_->Policy()) {
      OmsIvRight  ivrRights = pol->GetIvrAdminRights();
      if (!IsEnumContains(ivrRights, OmsIvRight::IsAdmin)) {
         // 不是 admin 權限, 則必須是可用帳號.
         if (!scRes.Ivr_)
            scRes.Ivr_ = res.Brks_->GetIvr(ToStrView(this->BrkId_), this->IvacNo_, ToStrView(this->SubacNo_));
         ivrRights = pol->GetIvRights(scRes.Ivr_.get());
      }
      OmsIvRight  ivRightDeny = RxKindToIvRightDeny(runner.Request_->RxKind());
      if (!IsEnumContains(ivrRights, ivRightDeny))
         return ivrRights;
   }
   runner.RequestAbandon(&res, OmsErrCode_IvNoPermission);
   return OmsIvRight::DenyAll;
}
bool OmsRequestIni::PreCheck_IvRight(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) const {
   OmsIvRight  ivrRights = this->CheckIvRight(runner, res, scRes);
   if (ivrRights == OmsIvRight::DenyAll)
      return false;
   if (fon9_LIKELY(runner.Request_->RxKind() == f9fmkt_RxKind_RequestNew))
      return true;
   // runner 使用 OmsRequestIni 但不是新單(而是: 刪、改、查...).
   if (runner.Request_.get() != this) {
      // 若委託已存在, 則欄位(Ivr,Side,Symbol,...)必須正確.
      if (const char* pErrField = this->IsIniFieldEqual(*runner.Request_)) {
         runner.RequestAbandon(&res, OmsErrCode_FieldNotMatch, fon9::RevPrintTo<std::string>('\'', pErrField, "' not equal."));
         return false;
      }
      return true;
   }
   // 若委託不存在(用 OrdKey 找不到委託)
   // => 此 this 為「委託遺失」的補單操作.
   // => 必須有 AllowRequestIni 權限.
   if (IsEnumContains(ivrRights, OmsIvRight::AllowRequestIni))
      return true;
   runner.RequestAbandon(&res, OmsErrCode_DenyRequestIni);
   return false;
}
bool OmsRequestIni::PreCheck_IvRight(OmsRequestRunner& runner, OmsResource& res) const {
   assert(runner.Request_.get() != this);
   assert(this->LastUpdated() != nullptr);
   return this->CheckIvRight(runner, res, this->LastUpdated()->Order_->ScResource())
      != OmsIvRight::DenyAll;
}
OmsOrderRaw* OmsRequestIni::BeforeRunInCore(OmsRequestRunner& runner, OmsResource& res) {
   assert(this == runner.Request_.get());
   OmsScResource  scRes;
   if (const OmsRequestIni* iniReq = this->PreCheck_OrdKey(runner, res, scRes)) {
      if (iniReq == runner.Request_.get()) {
         if (iniReq->PreCheck_IvRight(runner, res, scRes))
            return iniReq->Creator_->OrderFactory_->MakeOrder(*this, &scRes);
      }
      else {
         OmsOrder*      ord = iniReq->LastUpdated()->Order_;
         OmsScResource& ordScRes = ord->ScResource();
         ordScRes.CheckMoveFrom(std::move(scRes));
         if (iniReq->PreCheck_IvRight(runner, res, ordScRes))
            return ord->BeginUpdate(*this);
      }
   }
   return nullptr;
}

//--------------------------------------------------------------------------//

void OmsRequestUpd::MakeFieldsImpl(fon9::seed::Fields& flds) {
   flds.Add(fon9_MakeField2(OmsRequestUpd, IniSNO));
   base::MakeFields<OmsRequestUpd>(flds);
}
OmsOrderRaw* OmsRequestUpd::BeforeRunInCore(OmsRequestRunner& runner, OmsResource& res) {
   assert(this == runner.Request_.get());
   if (const OmsRequestBase* iniReq = this->PreCheck_GetRequestInitiator(runner, res)) {
      OmsOrder* ord = iniReq->LastUpdated()->Order_;
      if (ord->Initiator_->PreCheck_IvRight(runner, res))
         return ord->BeginUpdate(*runner.Request_);
   }
   return nullptr;
}

} // namespaces
