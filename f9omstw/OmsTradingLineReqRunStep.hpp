// \file f9omstw/OmsTradingLineReqRunStep.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsTradingLineReqRunStep_hpp__
#define __f9omstw_OmsTradingLineReqRunStep_hpp__
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "fon9/fmkt/TradingLineGroup.hpp"
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
            if (auto* inireq = runner.OrderRaw().Order().Initiator()) {
               auto tsvr = lmgr->Lock();
               lmgr->SelectPreferNextLine(*inireq, tsvr);
               lmgr->RunRequest(std::move(runner), tsvr);
               return;
            }
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
class OmsTradingLgMgrT : public fon9::fmkt::TradingLgMgrBase {
   fon9_NON_COPY_NON_MOVE(OmsTradingLgMgrT);
   using base = fon9::fmkt::TradingLgMgrBase;
protected:
   class LgItem : public LgItemBase, public LineGroup {
      fon9_NON_COPY_NON_MOVE(LgItem);
      using base = LgItemBase;
   public:
      using base::base;
   };
   using LgItemSP = fon9::intrusive_ptr<LgItem>;
   LgItemBaseSP CreateLgItem(Named&& named) override {
      return new LgItem{std::move(named)};
   }
   fon9::fmkt::TradingLineManager* GetLineMgr(LgItemBase& lg, const fon9::fmkt::TradingRequest& req) const override {
      assert(dynamic_cast<LgItem*>(&lg) != nullptr);
      return static_cast<LgItem*>(&lg)->GetLineMgr(req);
   }
   fon9::fmkt::TradingLineManager* GetLineMgr(LgItemBase& lg, const fon9::fmkt::TradingLineManager& ref) const override {
      assert(dynamic_cast<LgItem*>(&lg) != nullptr);
      return static_cast<LgItem*>(&lg)->GetLineMgr(ref);
   }
   fon9::fmkt::TradingLineManager* GetLineMgr(LgItemBase& lg, fon9::fmkt::LmIndex lmIndex) const override {
      assert(dynamic_cast<LgItem*>(&lg) != nullptr);
      return static_cast<LgItem*>(&lg)->GetLineMgr(lmIndex);
   }

   LgItemSP MakeLgItem(fon9::StrView tag, fon9::StrView cfgln) {
      return fon9::dynamic_pointer_cast<LgItem>(base::MakeLgItem(tag, cfgln));
   }

public:
   OmsTradingLgMgrT(std::string name)
      : base(std::move(name)) {
   }
   ~OmsTradingLgMgrT() {
   }

   void OnTDayChangedInCore(OmsResource& resource) {
     if (resource.Core_.IsCurrentCore())
        for (auto& lgItem : this->LgItems_) {
           if (lgItem)
              static_cast<LgItem*>(lgItem.get())->OnTDayChangedInCore(resource);
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
      return static_cast<LgItem*>(this->LgItems_[idx].get());
   }
   /// 根據 Request.LgOut_ 選用適當的線路群組.
   const LgItem* GetLgItem(LgOut lg) const {
      if (auto* retval = static_cast<const LgItem*>(this->LgItems_[LgOutToIndex(lg)].get()))
         return retval;
      return static_cast<const LgItem*>(this->LgItems_[0].get());
   }
   /// 若返回 nullptr, 則返回前, 會先執行 runner.Reject();
   typename LgItem::LineMgr* GetLineMgr(OmsRequestRunnerInCore& runner) const {
      return this->GetLineMgr(runner.OrderRaw(), &runner);
   }
   typename LgItem::LineMgr* GetLineMgr(const OmsOrderRaw& ordraw, OmsRequestRunnerInCore* runner) const {
      if (auto* lgItem = this->GetLgItem(ordraw.GetLgOut())) {
         return lgItem->GetLineMgr(ordraw, runner);
      }
      if (runner)
         runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_Bad_LgOut, nullptr);
      return nullptr;
   }
   fon9::fmkt::LmIndex LgLineMgrCount() const override {
      return LineGroup::LgLineMgrCount();
   }
};

} // namespaces
#endif//__f9omstw_OmsTradingLineReqRunStep_hpp__
