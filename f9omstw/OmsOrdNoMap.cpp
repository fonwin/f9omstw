// \file f9omstw/OmsOrdNoMap.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsOrdNoMap.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsOrder.hpp"

namespace f9omstw {

OmsOrdNoMap::~OmsOrdNoMap() {
}

bool OmsOrdNoMap::AllocOrdNo(OmsRequestRunnerInCore& runner, const OmsOrdTeam team) {
   assert(team.Length_ > 0);
   // 尋找 team 的最後號.
   struct OrdNoStr : public OmsOrdNo {
      uint8_t  Length_;
      void clear() {
         this->Length_ = 0;
         memset(this, '0', this->size());
      }
      void push_back(char ch) {
         assert(this->Length_ < this->size());
         this->Chars_[this->Length_++] = ch;
      }
   };
   OrdNoStr ordno;
   this->find_tail_keystr(fon9::StrView{team.Chars_, team.Length_}, ordno);
   if (fon9_UNLIKELY(ordno.Length_ < team.Length_)) {
      do {
         ordno.Chars_[ordno.Length_] = team.Chars_[ordno.Length_];
      } while (++ordno.Length_ < team.Length_);
   }
   else if (fon9_LIKELY(IncStrAlpha(ordno.begin() + team.Length_, ordno.end()))) {
      assert(ordno.Length_ == ordno.size());
   }
   else {
      return false;
   }
   runner.OrderRaw_.OrdNo_ = ordno;
   auto ires = this->emplace(fon9::StrView{ordno.begin(), ordno.end()}, runner.OrderRaw_.Order_);
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
      if (this->AllocOrdNo(runner, teams->back()))
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
   if (this->AllocOrdNo(runner, OmsOrdTeam{req, static_cast<uint8_t>(peos - req.begin())}))
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
