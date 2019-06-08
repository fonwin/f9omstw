// \file f9omstw/OmsOrdNoMap.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsOrdNoMap_hpp__
#define __f9omstw_OmsOrdNoMap_hpp__
#include "f9omstw/OmsOrdTeam.hpp"
#include "f9omstw/OmsBase.hpp"
#include "f9omstw/OmsErrCode.h"
#include "fon9/Trie.hpp"

namespace f9omstw {

class OmsOrdNoMap : public fon9::intrusive_ref_counter<OmsOrdNoMap>,
                    private fon9::Trie<fon9::TrieKeyAlNum, OmsOrder*, fon9::TrieDummyPtrValue<OmsOrder> > {
   fon9_NON_COPY_NON_MOVE(OmsOrdNoMap);
   OmsOrdTeamGroups  TeamGroups_;
   bool AllocByTeam(OmsRequestRunnerInCore& runner, const OmsOrdTeam team);
public:
   OmsOrdNoMap() = default;
   ~OmsOrdNoMap();

   /// 使用第 tgId 的櫃號設定分配委託書號, 不考慮 runner.OrderRaw_.Order_->Initiator_->Policy()->OrdTeamGroupId(); 的櫃號設定.
   bool AllocOrdNo(OmsRequestRunnerInCore& runner, OmsOrdTeamGroupId tgId);

   /// 使用 runner.OrderRaw_.Order_->Initiator_->Policy()->OrdTeamGroupId(); 的櫃號設定.
   /// - 如果沒有 Policy, 或 Policy 沒設定 OrdTeamGroupId, 則 OmsErrCode_OrdTeamGroupId;
   /// - 如果 (!req.empty1st()) 則必須要有 IsAllowAnyOrdNo_ 權限.
   bool AllocOrdNo(OmsRequestRunnerInCore& runner, OmsOrdNo req);

   /// 固定傳回 false;
   static bool Reject(OmsRequestRunnerInCore& runner, OmsErrCode errc);

   /// 建立 ord.OrdNo_ 對照.
   /// - 您必須先確定 ord.OrdNo_ 必須不是空的, 此函式不檢查.
   /// - 通常用在重新載入, 或是外部新單回報.
   bool EmplaceOrder(const OmsOrderRaw& ord);

   /// 用委託書號取的委託, 通常用於「刪、改、查」要求、或回報(委託、成交),
   /// 提供委託書找回原委託, 然後進行後續處理。
   OmsOrder* GetOrder(OmsOrdNo ordno) const;
};

} // namespaces
#endif//__f9omstw_OmsOrdNoMap_hpp__
