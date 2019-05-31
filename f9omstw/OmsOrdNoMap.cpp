﻿// \file f9omstw/OmsOrdNoMap.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsOrdNoMap.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsOrder.hpp"

namespace f9omstw {

OmsOrdNoMap::~OmsOrdNoMap() {
}

bool OmsOrdNoMap::AllocByTeam(OmsRequestRunnerInCore& runner, const OmsOrdTeam team) {
   assert(team.size() > 0);
   // 尋找 team 的最後號.
   struct OrdNoStr : public fon9::CharAryL<OmsOrdNo::size()> {
      using base = fon9::CharAryL<OmsOrdNo::size()>;
      void clear() {
         base::clear();
         memset(this->begin(), '0', this->max_size());
      }
      bool IncOrdNo(size_t idxfrom) {
         return IncStrAlpha(this->begin() + idxfrom, this->begin() + this->max_size());
      }
      void PutOrdNo(OmsOrdNo& dst) {
         // 此時雖然 this->size() 不是 max_size(); 但因為 clear() 時, 已填 '0',
         // 所以可已直接取用 ordno.GetOrigCharAry();
         dst = this->Ary_;
      }
   };
   OrdNoStr ordno;
   this->find_tail_keystr(ToStrView(team), ordno);
   if (fon9_UNLIKELY(ordno.size() < team.size())) {
      do {
         ordno.push_back(team[ordno.size()]);
      } while (ordno.size() < team.size());
   }
   else if (fon9_UNLIKELY(!ordno.IncOrdNo(team.size()))) {
      return false;
   }
   ordno.PutOrdNo(runner.OrderRaw_.OrdNo_);
   auto ires = this->emplace(fon9::StrView{ordno.begin(), ordno.begin() + ordno.max_size()}, runner.OrderRaw_.Order_);
   (void)ires; assert(ires.second == true);
   return true;
}
bool OmsOrdNoMap::AllocOrdNo(OmsRequestRunnerInCore& runner, OmsOrdTeamGroupId tgId) {
   auto tgCfg = runner.Resource_.OrdTeamGroupMgr_.GetTeamGroupCfg(tgId);
   if (fon9_UNLIKELY(tgCfg == nullptr))
      return this->Reject(runner, OmsErrCode_OrdTeamGroupId);
   auto teams = this->TeamGroups_.FetchTeamList(*tgCfg);
   assert(teams != nullptr);
   while (!teams->empty()) {
      if (this->AllocByTeam(runner, teams->back()))
         return true;
      teams->pop_back();
   }
   return this->Reject(runner, OmsErrCode_OrdTeamUsedUp);
}
bool OmsOrdNoMap::AllocOrdNo(OmsRequestRunnerInCore& runner, const OmsOrdNo req) {
   if (*req.begin() == '\0')
      return this->AllocOrdNo(runner, runner.OrderRaw_.Order_->ScResource().OrdTeamGroupId_);
   auto tgCfg = runner.Resource_.OrdTeamGroupMgr_.GetTeamGroupCfg(runner.OrderRaw_.Order_->ScResource().OrdTeamGroupId_);
   if (fon9_UNLIKELY(tgCfg == nullptr))
      return this->Reject(runner, OmsErrCode_OrdTeamGroupId);
   if (!tgCfg->IsAllowAnyOrdNo_)
      return this->Reject(runner, OmsErrCode_OrdNoMustEmpty);
   const char* peos = reinterpret_cast<const char*>(memchr(req.begin(), '\0', req.size()));
   if (peos == nullptr) { // req = 完整委託書號.
      auto ires = this->emplace(fon9::StrView{req.begin(), req.end()}, runner.OrderRaw_.Order_);
      if (ires.second) {  // 成功將 order 設定在指定的委託書號.
         runner.OrderRaw_.OrdNo_ = req;
         return true;
      }
      return this->Reject(runner, OmsErrCode_OrderAlreadyExists);
   }
   if (this->AllocByTeam(runner, OmsOrdTeam{req.begin(), static_cast<uint8_t>(peos - req.begin())}))
      return true;
   return this->Reject(runner, OmsErrCode_OrdNoOverflow);
}
bool OmsOrdNoMap::Reject(OmsRequestRunnerInCore& runner, OmsErrCode errc) {
   runner.OrderRaw_.ErrCode_ = errc;
   runner.OrderRaw_.OrderSt_ = f9fmkt_OrderSt_NewOrdNoRejected;
   runner.OrderRaw_.RequestSt_ = f9fmkt_TradingRequestSt_OrdNoRejected;
   return false;
}

bool OmsOrdNoMap::EmplaceOrder(const OmsOrderRaw& ord) {
   assert(*ord.OrdNo_.begin() != '\0');
   const auto ires = this->emplace(fon9::StrView{ord.OrdNo_.begin(), ord.OrdNo_.end()}, ord.Order_);
   return(ires.second || ord.Order_ == ires.first->value());
}
OmsOrder* OmsOrdNoMap::GetOrder(OmsOrdNo ordno) const {
   auto ifind = this->find(ToStrView(ordno));
   return(ifind == this->end() ? nullptr : ifind->value());
}

} // namespaces
