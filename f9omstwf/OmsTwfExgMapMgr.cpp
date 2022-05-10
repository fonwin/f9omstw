// \file f9omstwf/OmsTwfExgMapMgr.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfExgMapMgr.hpp"
#include "f9omstwf/OmsTwfTypes.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9twf/TwfTickSize.hpp"
#include "fon9/Log.hpp"

namespace f9omstw {

void TwfExgMapMgr::Ctor() {
   this->SetNeedsCurrencyConfig(true);
   auto& configTree = this->GetFileImpSapling();
   configTree.Add(new CurrencyConfig_ImpSeed(*this, fon9::seed::FileImpMonitorFlag::Reload, configTree, "0_CurrencyConfig", "TwfCurrencyIndex.cfg"));
}
TwfExgMapMgr::~TwfExgMapMgr() {
}
void TwfExgMapMgr::OnAfter_InitCoreTables(OmsResource& res) {
   fon9::intrusive_ptr<TwfExgContractTree> ctree{new TwfExgContractTree()};
   if (res.Sapling_->AddNamedSapling(ctree, fon9::Named{"Contracts"})) {
      this->ResetCoreResource(&res);
      this->ContractTree_ = std::move(ctree);
      this->SetTDay(res.TDay_);
      this->ResetCoreResource(nullptr);
   }
   res.Sapling_->AddNamedSapling(this->CurrencyTree_, fon9::Named{"Currency"});
}
void TwfExgMapMgr::OnP06Updated(const f9twf::ExgMapBrkFcmId& mapBrkFcmId, MapsLocker&& lk) {
   (void)mapBrkFcmId;
   fon9::intrusive_ptr<TwfExgMapMgr> pthis{this};
   this->RunCoreTask(std::move(lk), [pthis](OmsResource& coreResource) {
      auto  maps = pthis->Lock();
      for (size_t L = coreResource.Brks_->GetBrkCount(); L > 0;) {
         if (auto* brk = coreResource.Brks_->GetBrkRec(--L)) {
            if (auto fcmId = maps->MapBrkFcmId_.GetFcmId(ToStrView(brk->BrkId_))) {
               brk->FcmId_ = fcmId;
               // 理論上, brk->CmId_ 應該在建立 Brk 時就設定好.
               // 在此做個保險: 如果沒設定, 則使用 fcmId.
               if (brk->CmId_ == 0)
                  brk->CmId_ = fcmId;
            }
         }
      }
   });
}
void TwfExgMapMgr::OnP08Updated(const f9twf::P08Recs& src, f9twf::ExgSystemType sysType, MapsConstLocker&& lk) {
   (void)src;
   // 透過 this->OnAfter_InitCoreTables() 啟動 => 設定 this->CurrentCore_;
   // 在 coreMgr.AddCore() 之後才啟動 twfMapMgr->SetTDay(); =>此時 coreMgr.CurrentCore() 必定有效;
   fon9::intrusive_ptr<TwfExgMapMgr> pthis{this};
   this->RunCoreTask(std::move(lk), [pthis, sysType](OmsResource& coreResource) {
      auto  maps = pthis->Lock();
      auto& p08recs = maps->MapP08Recs_[f9twf::ExgSystemTypeToIndex(sysType)];
      // 以「短Id」為主, 若沒有「短Id」, 則不匯入.
      if (p08recs.empty() || p08recs.UpdatedTimeS_.IsNullOrZero())
         return;
      fon9::RevBufferList rbuf{128};
      fon9::RevPrint(rbuf, fon9::LocalNow(),
                     "|Sys=", sysType,
                     "|P08.ftime=", p08recs.UpdatedTimeS_, fon9::kFmtYMD_HH_MM_SS_us_L,
                     "|PA8.ftime=", p08recs.UpdatedTimeL_, fon9::kFmtYMD_HH_MM_SS_us_L,
                     "|items=", p08recs.size(),
                     '\n');
      coreResource.LogAppend(std::move(rbuf));

      OmsSymbTree&         symbsTree = *coreResource.Symbs_;
      OmsSymbTree::Locker  symbs{symbsTree.SymbMap_};
      auto                 ctree = pthis->ContractTree_;
      for (auto& p08 : p08recs) {
         if (p08.ShortId_.empty())
            continue;
         const auto        shortId = ToStrView(p08.ShortId_);
         OmsSymbSP         symb = symbsTree.FetchOmsSymb(shortId);
         TwfExgSymbBasic*  symbP08 = dynamic_cast<TwfExgSymbBasic*>(symb.get());
         if (symbP08 == nullptr)
            continue;
         if (!symbP08->Contract_ && ctree)
            symbP08->Contract_ = ctree->ContractMap_.FetchContract(f9twf::ContractId{shortId.begin(), 3});
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
         symb->FlowGroup_ = fon9::Pic9StrTo<fon9::fmkt::SymbFlowGroup_t>(p08.Fields_.flow_group_);
         symb->TradingMarket_ = f9twf::ExgSystemTypeToMarket(sysType);
         symb->TradingSessionId_ = f9twf::ExgSystemTypeToSessionId(sysType);
         fon9::DecScaleT scale = fon9::Pic9StrTo<fon9::DecScaleT>(p08.Fields_.decimal_locator_);
         if (scale <= 9)
            symbP08->LastPrice_.Assign(fon9::Pic9StrTo<uint32_t>(p08.Fields_.premium_), scale);
         if ((scale = static_cast<fon9::DecScaleT>(OmsTwfPri::Scale - scale)) <= 9)
            symb->PriceOrigDiv_ = static_cast<uint32_t>(fon9::GetDecDivisor(scale));
         if ((scale = fon9::Pic9StrTo<fon9::DecScaleT>(p08.Fields_.strike_price_decimal_locator_)) <= 9)
            symb->StrikePriceDiv_ = static_cast<uint32_t>(fon9::GetDecDivisor(scale));
         symb->ExgSymbSeq_ = fon9::Pic9StrTo<f9twf::TmpSymbolSeq_t>(p08.Fields_.pseq_);
         symb->EndYYYYMMDD_ = fon9::Pic9StrTo<uint32_t>(p08.Fields_.end_date_);
         if ((symb->CallPut_ = p08.Fields_.call_put_code_.Chars_[0]) == ' ')
            symb->CallPut_ = '\0';
         symb->SettleYYYYMM_ = fon9::Pic9StrTo<uint32_t>(p08.Fields_.settle_date_);
         symb->StrikePrice_.Assign<4>(fon9::Pic9StrTo<uint32_t>(p08.Fields_.strike_price_v4_));
         symb->TDayYYYYMMDD_ = (symb->TradingSessionId_ == f9fmkt_TradingSessionId_AfterHour
                                ? coreResource.NextTDayYYYYMMDD_
                                : coreResource.TDayYYYYMMDD_);
         symb->MaxQtyMarket_ = fon9::Pic9StrTo<f9twf::TmpQty_t>(p08.Fields_.market_order_ceiling_);
         if (p08.Fields_.prod_kind_.Chars_[0] == 'S')
            symb->MaxQtyLimit_ = 499;
         else if (symb->TradingMarket_ == f9fmkt_TradingMarket_TwFUT)
            symb->MaxQtyLimit_ = 100;
         else {
            assert(symb->TradingMarket_ == f9fmkt_TradingMarket_TwOPT);
            symb->MaxQtyLimit_ = 200;
         }
      }
   });
}
void TwfExgMapMgr::OnP09Updated(const f9twf::P09Recs& src, f9twf::ExgSystemType sysType, MapsConstLocker&& lk) {
   (void)src;
   fon9::intrusive_ptr<TwfExgMapMgr> pthis{this};
   this->RunCoreTask(std::move(lk), [pthis, sysType](OmsResource&) {
      auto  maps = pthis->Lock();
      auto& p09recs = maps->MapP09Recs_[f9twf::ExgSystemTypeToIndex(sysType)];
      if (p09recs.empty())
         return;
      auto ctree = pthis->ContractTree_;
      if (!ctree)
         return;
      for (auto& p09 : p09recs) {
         auto contract = ctree->ContractMap_.FetchContract(ToStrView(p09.ContractId_));
         if (!contract)
            continue;
         contract->TradingMarket_ = f9twf::ExgSystemTypeToMarket(sysType);
         if (!p09.stock_id_.empty1st()) // P09 未設定 stock_id_, 不應清除 contract->TargetId_, 因為可能在其他地方有設定.
            contract->TargetId_.AssignFrom(ToStrView(p09.stock_id_));
         contract->SubType_ = static_cast<f9twf::ExgContractType>(p09.subtype_);
         contract->ExpiryType_ = static_cast<f9twf::ExgExpiryType>(p09.expiry_type_);
         contract->ContractSize_.Assign<4>(fon9::Pic9StrTo<uint64_t>(p09.contract_size_v4_));
         contract->AcceptQuote_ = (p09.accept_quote_flag_ == 'Y' ? fon9::EnabledYN::Yes : fon9::EnabledYN{});
         contract->CanBlockTrade_ = (p09.block_trade_flag_ == 'Y' ? fon9::EnabledYN::Yes : fon9::EnabledYN{});
         if (auto* cidx = pthis->CurrencyTree_->GetCurrencyConfig(p09.currency_type_))
            contract->CurrencyIndex_ = cidx->CurrencyIndex_;
         else {
            contract->CurrencyIndex_ = CurrencyIndex_Unsupport;
            // fon9_LOG_ERROR("TwfExgMapMgr:OnP09Updated|err=Unknown currency_type:", p09.currency_type_,
            //                "|'CurrencyIndex' not loaded or set incorrectly.");
         }
         if (p09.status_code_ != 'N') // 狀態碼 N：正常, P：暫停交易, U：即將上市;
            contract->DenyReason_.assign(&p09.status_code_, 1);
         if (contract->SubType_ == f9twf::ExgContractType::StockGen) {
            if (p09.underlying_type_ == 'E')
               contract->SubType_ = f9twf::ExgContractType::StockETF;
         }
         contract->LvPriSteps_ = f9twf::GetLvPriStep(contract->ContractId_, contract->TradingMarket_, contract->SubType_);
         contract->LvPriStepsStr_ = fon9::fmkt::LvPriStep_ToStr(contract->LvPriSteps_);
         if (fon9::isdigit(p09.decimal_locator_.Chars_[0]))
            contract->PriDecLoc_ = static_cast<uint8_t>(p09.decimal_locator_.Chars_[0] - '0');
         if (fon9::isdigit(p09.strike_price_decimal_locator_.Chars_[0]))
            contract->SpDecLoc_ = static_cast<uint8_t>(p09.strike_price_decimal_locator_.Chars_[0] - '0');
      }
   });
}

} // namespaces
