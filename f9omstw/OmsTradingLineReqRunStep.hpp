// \file f9omstw/OmsTradingLineReqRunStep.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsTradingLineReqRunStep_hpp__
#define __f9omstw_OmsTradingLineReqRunStep_hpp__
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "fon9/fmkt/TradingLine.hpp"
#include "fon9/framework/IoManager.hpp"

namespace f9omstw {

template <class LineGroupMgr>
class OmsTradingLineReqRunStepT : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(OmsTradingLineReqRunStepT);
   using base = OmsRequestRunStep;
   using base::base;

public:
   LineGroupMgr&  LineGroupMgr_;
   OmsTradingLineReqRunStepT(LineGroupMgr& lineMgr)
      : LineGroupMgr_(lineMgr) {
   }
   void RunRequest(OmsRequestRunnerInCore&& runner) override {
      if (auto* lmgr = this->LineGroupMgr_.GetLineMgr(runner))
         lmgr->RunRequest(std::move(runner));
   }
   void RerunRequest(OmsReportRunnerInCore&& runner) override {
      if (auto* lmgr = this->LineGroupMgr_.GetLineMgr(runner)) {
         if (runner.ErrCodeAct_->IsUseNewLine_) {
            if (auto* inireq = runner.OrderRaw_.Order().Initiator())
               lmgr->SelectPreferNextLine(*inireq);
         }
         lmgr->RunRequest(std::move(runner));
      }
   }
};
//--------------------------------------------------------------------------//
/// 單一線路群組(無分組,不理會下單時的LgOut).
/// - 負責處理 TDay 改變事件的後續相關作業.
/// - 透過 LineGroup 整合相關線路:
///   - LineGroup = TwsTradingLineGroup: 包含: [上市、上櫃] 線路管理員;
///   - LineGroup = TwfTradingLineGroup: 包含: [期貨日盤、選擇權日盤、期貨夜盤、選擇權夜盤] 線路管理員.
///   - LineGroup 必須提供:
///        struct LineGroup {
///          typename LineMgr;  // = OmsTradingLineMgrT<>; 同一個交易
///          typename LineMgr* GetLineMgr(OmsRequestRunnerInCore& runner) const;
///          void OnTDayChangedInCore(OmsResource& resource);
///        };
template <class LineGroup>
class OmsTradingLineG1T : public LineGroup {
   fon9_NON_COPY_NON_MOVE(OmsTradingLineG1T);
   OmsCoreMgr& CoreMgr_;
   void OnTDayChanged(OmsCore& core) {
      core.RunCoreTask([this](OmsResource& resource) {
         if (this->CoreMgr_.CurrentCore().get() == &resource.Core_)
            this->OnTDayChangedInCore(resource);
      });
   }
public:
   OmsTradingLineG1T(OmsCoreMgr& coreMgr) : CoreMgr_(coreMgr) {
      coreMgr.TDayChangedEvent_.Subscribe(// &this->SubrTDayChanged_,
                                          std::bind(&OmsTradingLineG1T::OnTDayChanged, this, std::placeholders::_1));
   }
   ~OmsTradingLineG1T() {
      // CoreMgr 擁有 OmsTradingLineG1T, 所以當此時, CoreMgr_ 必定正在死亡!
      // 因此沒必要 this->CoreMgr_.TDayChangedEvent_.Unsubscribe(&this->SubrTDayChanged_);
   }
};
//--------------------------------------------------------------------------//
/// 線路群組管理(下單時根據LgOut選擇群組).
/// - 負責處理 TDay 改變事件的後續相關作業.
template <class LineGroup>
class OmsTradingLgMgrT : public fon9::seed::NamedMaTree {
   fon9_NON_COPY_NON_MOVE(OmsTradingLgMgrT);
   using base = fon9::seed::NamedMaTree;
protected:
   OmsCoreMgr& CoreMgr_;

   class LgItem : public fon9::seed::NamedMaTree, public LineGroup {
      fon9_NON_COPY_NON_MOVE(LgItem);
      using base = fon9::seed::NamedMaTree;
   public:
      using base::base;
   };
   using LgItemSP = fon9::intrusive_ptr<LgItem>;
   LgItemSP LgItems_[static_cast<uint8_t>(LgOut::Count)];

   /// Lg0 = Title, Description
   LgItemSP MakeLgItem(fon9::StrView tag, fon9::StrView cfgln) {
      constexpr char fon9_kCSTR_LgTAG_LEAD[] = "Lg";
      constexpr auto fon9_kSIZE_LgTAG_LEAD = (sizeof(fon9_kCSTR_LgTAG_LEAD) - 1);
      if (tag.size() != fon9_kSIZE_LgTAG_LEAD + 1)
         return nullptr;
      const char* ptag = tag.begin();
      // Unknown tag.
      if (memcmp(ptag, fon9_kCSTR_LgTAG_LEAD, fon9_kSIZE_LgTAG_LEAD) != 0)
         return nullptr;
      const LgOut lgId = static_cast<LgOut>(ptag[fon9_kSIZE_LgTAG_LEAD]);
      // Unknown LgId.
      if (!IsValidateLgOut(lgId))
         return nullptr;
      const uint8_t lgIdx = LgOutToIndex(lgId);
      // Dup Lg.
      if (this->LgItems_[lgIdx].get() != nullptr)
         return nullptr;
      fon9::StrView  title = fon9::SbrFetchNoTrim(cfgln, ',');
      LgItemSP retval{new LgItem{fon9::Named(
         tag.ToString(),
         fon9::StrView_ToNormalizeStr(fon9::StrTrimRemoveQuotes(title)),
         fon9::StrView_ToNormalizeStr(fon9::StrTrimRemoveQuotes(cfgln))
      )}};
      this->LgItems_[lgIdx] = retval;
      this->Sapling_->Add(retval);
      return retval;
   }

   void OnTDayChanged(OmsCore& core) {
      core.RunCoreTask([this](OmsResource& resource) {
         if (this->CoreMgr_.CurrentCore().get() != &resource.Core_)
            return;
         for (LgItemSP& lgItem : this->LgItems_) {
            if (lgItem)
               lgItem->OnTDayChangedInCore(resource);
         }
      });
   }

public:
   OmsTradingLgMgrT(OmsCoreMgr& coreMgr, std::string name)
      : base(std::move(name))
      , CoreMgr_(coreMgr) {
      coreMgr.TDayChangedEvent_.Subscribe(//&this->SubrTDayChanged_,
                                          std::bind(&OmsTradingLgMgrT::OnTDayChanged, this, std::placeholders::_1));
   }
   ~OmsTradingLgMgrT() {
      // CoreMgr 擁有 OmsTradingLgMgrT, 所以當 OmsTradingLgMgrT 解構時, CoreMgr_ 必定正在死亡!
      // 因此沒必要 this->CoreMgr_.TDayChangedEvent_.Unsubscribe(&this->SubrTDayChanged_);
   }

   template <class SelfT, class ConfigFileBinder>
   static fon9::intrusive_ptr<SelfT> Make(OmsCoreMgr&                coreMgr,
                                          fon9::seed::PluginsHolder& holder,
                                          fon9::IoManagerArgs&       iocfgs,
                                          fon9::StrView              lgTypeName,
                                          ConfigFileBinder&          cfgb) {
      std::string errmsg = cfgb.OpenRead(nullptr, iocfgs.CfgFileName_, false/*isAutoBackup*/);
      if (!errmsg.empty()) {
         holder.SetPluginsSt(fon9::LogLevel::Error, lgTypeName, '.', iocfgs.Name_, '|', errmsg);
         return nullptr;
      }
      fon9::intrusive_ptr<SelfT> retval{new SelfT(coreMgr, iocfgs.Name_)};
      if (!coreMgr.Add(retval, lgTypeName)) {
         holder.SetPluginsSt(fon9::LogLevel::Error, lgTypeName, '.', iocfgs.Name_, "|err=AddTo CoreMgr");
         return nullptr;
      }
      retval->SetTitle(cfgb.GetFileName());
      return retval;
   }

   /// 根據 Request.LgOut_ 選用適當的線路群組.
   const LgItem* GetLgItem(LgOut lg) const {
      auto idx = LgOutToIndex(lg);
      if (const LgItem* retval = this->LgItems_[idx].get())
         return retval;
      return this->LgItems_[0].get();
   }
   /// 若返回 nullptr, 則返回前, 會先執行 runner.Reject();
   typename LgItem::LineMgr* GetLineMgr(OmsRequestRunnerInCore& runner) const {
      if (auto* lgItem = this->GetLgItem(runner.OrderRaw_.GetLgOut())) {
         return lgItem->GetLineMgr(runner);
      }
      runner.Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_Bad_LgOut, nullptr);
      return nullptr;
   }
};

} // namespaces
#endif//__f9omstw_OmsTradingLineReqRunStep_hpp__
