﻿// \file f9omstw/OmsOrdNoMap.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsOrdNoMap.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsOrder.hpp"

namespace f9omstw {

char OmsOrdNoStr_BeginValue[] = "\0" "00000";

OmsOrdNoMap::~OmsOrdNoMap() {
}
bool OmsOrdNoMap::GetNextOrdNo(const OmsOrdTeam team, OmsOrdNo& out, FnIncOrdNo fnIncOrdNo) {
   assert(team.size() > 0);
   // 尋找 team 的最後號.
   struct OrdNoStr : public fon9::CharAryL<OmsOrdNo::size()> {
      void clear() {
         #define kCSTR_FirstOrdNo   OmsOrdNoStr_BeginValue
         static_assert(sizeof(kCSTR_FirstOrdNo) - 1 == sizeof(*this), "");
         memcpy(this, kCSTR_FirstOrdNo, sizeof(*this));
         static_assert(sizeof(*this) == sizeof(OmsOrdNo) + 1, "");
         assert(this->size() == 0);
      }
      void PutToOrdNo(OmsOrdNo& dst) {
         // 此時雖然 this->size() 可能不是 max_size();
         // 但因為 clear() 時, 已填妥初始值, 所以可直接使用 this->Ary_;
         dst = *static_cast<const OmsOrdNo*>(&this->Ary_);
      }
   };
   OrdNoStr strOrdNo;
   this->find_tail_keystr(ToStrView(team), strOrdNo);

   #ifdef F9OMS_ORDNO_IS_NUM
      fnIncOrdNo = f9omstw_IncStrDec;
   #endif

   if (fon9_UNLIKELY(strOrdNo.size() < team.size())) {
      do {
         strOrdNo.push_back(team[strOrdNo.size()]);
      } while (strOrdNo.size() < team.size());
   }
   else if (fon9_UNLIKELY(!fnIncOrdNo(strOrdNo.begin() + team.size(), strOrdNo.begin() + strOrdNo.max_size()))) {
      return false;
   }
   strOrdNo.PutToOrdNo(out);
   return true;
}
bool OmsOrdNoMap::AllocByTeam(OmsRequestRunnerInCore& runner, const OmsOrdTeam team, FnIncOrdNo fnIncOrdNo) {
   OmsOrdNo& ordno = runner.OrderRaw().OrdNo_;
   if (fon9_LIKELY(this->GetNextOrdNo(team, ordno, fnIncOrdNo))) {
      auto ires = this->emplace(fon9::StrView{ordno.begin(), ordno.end()}, &runner.OrderRaw().Order());
      (void)ires; assert(ires.second == true);
      return true;
   }
   return false;
}
bool OmsOrdNoMap::AllocOrdNo(OmsRequestRunnerInCore& runner, OmsOrdTeamGroupId tgId) {
   auto tgCfg = runner.Resource_.OrdTeamGroupMgr_.GetTeamGroupCfg(tgId);
   if (fon9_UNLIKELY(tgCfg == nullptr))
      return this->Reject(runner, OmsErrCode_OrdTeamGroupId);
   auto teams = this->TeamGroups_.FetchTeamList(*tgCfg);
   assert(teams != nullptr);
   while (!teams->empty()) {
      if (this->AllocByTeam(runner, teams->back(), tgCfg->FnIncOrdNo_))
         return true;
      teams->pop_back();
   }
   return this->Reject(runner, OmsErrCode_OrdTeamUsedUp);
}
bool OmsOrdNoMap::AllocOrdNo(OmsRequestRunnerInCore& runner, const OmsOrdNo reqOrdNo, const OmsRequestTrade& iniReq) {
   OmsOrdTeamGroupId tgId;
   if (const auto* reqPolicy = iniReq.Policy()) {
      if ((tgId = reqPolicy->OrdTeamGroupId()) != 0)
         goto __READY_GroupId;
   }
__ERROR_OrdTeamGroupId:
   return this->Reject(runner, OmsErrCode_OrdTeamGroupId);

__READY_GroupId:
   if (OmsIsOrdNoEmpty(reqOrdNo))
      return this->AllocOrdNo(runner, tgId);
   auto tgCfg = runner.Resource_.OrdTeamGroupMgr_.GetTeamGroupCfg(tgId);
   if (fon9_UNLIKELY(tgCfg == nullptr))
      goto __ERROR_OrdTeamGroupId;
   if (!tgCfg->IsAllowAnyOrdNo_)
      return this->Reject(runner, OmsErrCode_OrdNoMustEmpty);
   const char* peos = reinterpret_cast<const char*>(memchr(reqOrdNo.begin(), '\0', reqOrdNo.size()));
   const auto  szReqOrdNo = (peos ? static_cast<uint8_t>(peos - reqOrdNo.begin()) : reqOrdNo.size());
   if (!tgCfg->TeamList_.empty()) {
      // 如果有指定櫃號列表, 則必須在列表內編號.
      for (const OmsOrdTeam cfg : tgCfg->TeamList_) {
         if (cfg.size() <= szReqOrdNo) {
            if (memcmp(cfg.begin(), reqOrdNo.begin(), cfg.size()) == 0)
               goto __ORD_TEAM_ALLOW;
         }
      }
      return this->Reject(runner, OmsErrCode_OrdTeamDeny);
   }
__ORD_TEAM_ALLOW:;
   if (peos == nullptr) { // req = 完整委託書號.
      auto ires = this->emplace(fon9::StrView{reqOrdNo.begin(), reqOrdNo.end()}, &runner.OrderRaw().Order());
      if (ires.second) {  // 成功將 order 設定在指定的委託書號.
         runner.OrderRaw().OrdNo_ = reqOrdNo;
         return true;
      }
      return this->Reject(runner, OmsErrCode_OrderAlreadyExists);
   }
   if (this->AllocByTeam(runner, OmsOrdTeam{reqOrdNo.begin(), szReqOrdNo}, tgCfg->FnIncOrdNo_))
      return true;
   return this->Reject(runner, OmsErrCode_OrdNoOverflow);
}
bool OmsOrdNoMap::Reject(OmsRequestRunnerInCore& runner, OmsErrCode errc) {
   // runner.OrderRaw_.ErrCode_ = errc;
   // runner.OrderRaw_.UpdateOrderSt_ = f9fmkt_OrderSt_NewOrdNoRejected;
   // runner.OrderRaw_.RequestSt_ = f9fmkt_TradingRequestSt_OrdNoRejected;
   runner.Reject(f9fmkt_TradingRequestSt_OrdNoRejected, errc, nullptr);
   return false;
}

bool OmsOrdNoMap::EmplaceOrder(OmsOrdNo ordno, OmsOrder* order) {
   assert(!OmsIsOrdNoEmpty(ordno) && order != nullptr);
   const auto ires = this->emplace(fon9::StrView{ordno.begin(), ordno.end()}, order);
   return(ires.second || order == ires.first->value());
}
OmsOrder* OmsOrdNoMap::GetOrder(OmsOrdNo ordno) const {
   auto ifind = this->find(ToStrView(ordno));
   return(ifind == this->end() ? nullptr : ifind->value());
}

} // namespaces
