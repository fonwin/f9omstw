// \file f9omstw/OmsTradingLineReqRunStep.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsTradingLineReqRunStep_hpp__
#define __f9omstw_OmsTradingLineReqRunStep_hpp__
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "fon9/fmkt/TradingLine.hpp"
#include "fon9/framework/IoManager.hpp"

namespace f9omstw {

/// - 負責處理 TDay 改變事件的後續相關作業: 轉送給 LineGroupMgr_.OnTDayChangedInCore(resource);
/// - 透過 LineGroupMgr 整合相關線路:
///   - LineGroupMgr 必須提供:
///         struct LineGroupMgr {
///            typename LineMgr;  // = OmsTradingLineMgrT<>;
///            typename LineMgr* GetLineMgr(OmsRequestRunnerInCore& runner) const;
///            void OnTDayChangedInCore(OmsResource& resource);
///         };
///   - LineGroupMgr::LineMgr
///      - TwsTradingLineGroup: 單一線路群組: 包含 [上市、上櫃] 線路管理員;
///      - TwfTradingLineGroup: 單一線路群組: 包含 [期貨日盤、選擇權日盤、期貨夜盤、選擇權夜盤] 線路管理員.
///      - OmsTradingLgMgrT<>:  線路群組管理: 可透過 LgOut 選擇線路群組.
template <class LineGroupMgr>
class OmsTradingLineReqRunStepT : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(OmsTradingLineReqRunStepT);
   using base = OmsRequestRunStep;
   using base::base;
   void OnCurrentCoreChangedImpl(OmsCore& core) override {
      core.RunCoreTask([this](OmsResource& resource) {
         if (resource.Core_.IsCurrentCore())
            this->LineGroupMgr_.OnTDayChangedInCore(resource);
      });
   }

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
/// 線路群組管理(下單時根據LgOut選擇群組).
/// - 負責處理 TDay 改變事件的後續相關作業: 轉送給各群組的 OnTDayChangedInCore(resource);
/// - 在 this 之下, 建立對應的 LineGroup seed;
template <class LineGroup>
class OmsTradingLgMgrT : public fon9::seed::NamedMaTree {
   fon9_NON_COPY_NON_MOVE(OmsTradingLgMgrT);
   using base = fon9::seed::NamedMaTree;
protected:
   class LgItem : public fon9::seed::NamedMaTree, public LineGroup {
      fon9_NON_COPY_NON_MOVE(LgItem);
      using base = fon9::seed::NamedMaTree;
   public:
      using base::base;
   };
   using LgItemSP = fon9::intrusive_ptr<LgItem>;
   LgItemSP LgItems_[static_cast<uint8_t>(LgOut::Count)];

   /// tag:   "Lg?"; 例: "Lg0"
   /// cfgln: "Title, Description"; 例: "VIP線路";
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

public:
   OmsTradingLgMgrT(std::string name)
      : base(std::move(name)) {
   }
   ~OmsTradingLgMgrT() {
   }

   void OnTDayChangedInCore(OmsResource& resource) {
     if (resource.Core_.IsCurrentCore())
        for (LgItemSP& lgItem : this->LgItems_) {
           if (lgItem)
              lgItem->OnTDayChangedInCore(resource);
        }
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

   LineGroup* GetIndexLineGroup(unsigned idx) const {
      return this->LgItems_[idx].get();
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
      return this->GetLineMgr(runner.OrderRaw_, &runner);
   }
   typename LgItem::LineMgr* GetLineMgr(const OmsOrderRaw& ordraw, OmsRequestRunnerInCore* runner) const {
      if (auto* lgItem = this->GetLgItem(ordraw.GetLgOut())) {
         return lgItem->GetLineMgr(ordraw, runner);
      }
      if (runner)
         runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_Bad_LgOut, nullptr);
      return nullptr;
   }
};

} // namespaces
#endif//__f9omstw_OmsTradingLineReqRunStep_hpp__
