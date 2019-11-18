// \file f9omstwf/OmsTwfExgMapMgr.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfExgMapMgr.hpp"
#include "f9omstwf/OmsTwfExgSymbBasic.hpp"
#include "f9omstwf/OmsTwfTypes.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "fon9/fmkt/Symb.hpp"

namespace f9omstw {

void TwfExgMapMgr::OnP08Updated(const f9twf::P08Recs& src, f9twf::ExgSystemType sysType, MapsConstLocker&& lk) {
   (void)src; (void)lk;
   namespace f9fmkt = fon9::fmkt;
   // 在 coreMgr.AddCore() 之後才啟動 twfMapMgr->SetTDay();
   // 所以此時 coreMgr.CurrentCore() 必定有效;
   fon9::intrusive_ptr<TwfExgMapMgr> pthis{this};
   this->CoreMgr_.CurrentCore()->RunCoreTask([pthis, sysType](OmsResource& coreResource) {
      auto  maps = pthis->Lock();
      auto& p08recs = maps->MapP08Recs_[f9twf::ExgSystemTypeToIndex(sysType)];
      // 以「短Id」為主, 若沒有「短Id」, 則不匯入.
      if (p08recs.empty() || p08recs.UpdatedTimeS_.IsNullOrZero())
         return;
      OmsSymbTree&         symbsTree = *coreResource.Symbs_;
      OmsSymbTree::Locker  symbs{symbsTree.SymbMap_};
      for (auto& p08 : p08recs) {
         if (p08.ShortId_.empty())
            continue;
         const auto         shortId = ToStrView(p08.ShortId_);
         fon9::fmkt::SymbSP symb = symbsTree.FetchSymb(shortId).get();
         TwfExgSymbBasic*   symbP08 = dynamic_cast<TwfExgSymbBasic*>(symb.get());
         if (symbP08 == nullptr)
            continue;
         fon9::StrView newLongId = ToStrView(p08.LongId_);
         fon9::StrView oldLongId = ToStrView(symbP08->LongId_);
         const bool    isLongIdChanged = (newLongId != oldLongId);
         if (p08.UpdatedTime_ <= symbP08->P08UpdatedTime_ && !isLongIdChanged)
            continue;
         symbP08->P08UpdatedTime_ = p08.UpdatedTime_;
         if (isLongIdChanged) {
            // symbs 的 key 使用 ToStrView{symb->LongId_};
            // 而 StrView 只是 const char* 的指標, 並沒有複製字串內容,
            // 所以更改 LongId_ 的步驟:
            //   1. 移除 symbs[oldLongId];
            //   2. symb->LongId_ = 新的 longId;
            //   3. 插入 symbs[ToStrView{symb->LongId_}]
            if (!oldLongId.empty() && oldLongId != shortId) {
               auto ifind = symbs->find(oldLongId);
               if (ifind != symbs->end())
                  symbs->erase(ifind);
            }
            symbP08->LongId_.assign(newLongId);
            if (!newLongId.empty() && newLongId != shortId)
               symbs->emplace(ToStrView(symbP08->LongId_), symb);
         }
         symb->FlowGroup_ = fon9::Pic9StrTo<f9fmkt::SymbFlowGroup_t>(p08.Fields_.flow_group_);
         symb->TradingMarket_ = f9twf::ExgSystemTypeToMarket(sysType);
         symb->TradingSessionId_ = f9twf::ExgSystemTypeToSessionId(sysType);
         symb->PriceOrigDiv_ = static_cast<uint32_t>(fon9::GetDecDivisor(
            static_cast<uint8_t>(OmsTwfPri::Scale
                                 - fon9::Pic9StrTo<uint8_t>(p08.Fields_.decimal_locator_))));
         symb->StrikePriceDiv_ = static_cast<uint32_t>(fon9::GetDecDivisor(
            fon9::Pic9StrTo<uint8_t>(p08.Fields_.strike_price_decimal_locator_)));
         symb->ExgSymbSeq_ = fon9::Pic9StrTo<f9twf::TmpSymbolSeq_t>(p08.Fields_.pseq_);
      }
   });
}

} // namespaces
