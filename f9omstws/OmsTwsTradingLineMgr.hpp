// \file f9omstws/OmsTwsTradingLineMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsTradingLineMgr_hpp__
#define __f9omstws_OmsTwsTradingLineMgr_hpp__
#include "f9tws/ExgTradingLineMgr.hpp"
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {
namespace f9fmkt = fon9::fmkt;

fon9_WARN_DISABLE_PADDING;
class TwsTradingLineMgr : public f9tws::ExgTradingLineMgr {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineMgr);
   using base = f9tws::ExgTradingLineMgr;
   using base::SendRequest;

   OmsOrdTeamGroupId       OrdTeamGroupId_{};
   fon9::CharVector        OrdTeamConfig_;
   OmsRequestRunnerInCore* CurrRunner_{};
   OmsResource*            CurrOmsResource_{};
   const fon9::CharVector  StrQueuingIn_;

   void SetOrdTeamGroupId(OmsResource& coreResource, const Locker&);

protected:
   OmsCoreSP   OmsCore_;

   f9fmkt::SendRequestResult NoReadyLineReject(f9fmkt::TradingRequest& req, fon9::StrView cause) override;

   void OnNewTradingLineReady(f9fmkt::TradingLine* src, Locker&& tsvr) override;
   void ClearReqQueue(Locker&& tsvr, fon9::StrView cause) override;
   void ClearReqQueue(Locker&& tsvr, fon9::StrView cause, OmsCoreSP core);

   void OnBeforeDestroy() override;

public:
   TwsTradingLineMgr(const fon9::IoManagerArgs& ioargs, f9fmkt_TradingMarket mkt);

   void OnOmsCoreChanged(OmsResource& coreResource);
   void OnOrdTeamConfigChanged(fon9::CharVector ordTeamConfig);

   void RunRequest(OmsRequestRunnerInCore&&);

   OmsRequestRunnerInCore* MakeRunner(fon9::DyObj<OmsRequestRunnerInCore>& tmpRunner,
                                      const OmsRequestBase& req,
                                      unsigned exLogForUpdArg) {
      if (fon9_LIKELY(this->CurrRunner_)) {
         // 從上一個步驟(例:風控檢查)而來的, 會來到這, 使用相同的 runner 繼續處理.
         assert(&this->CurrRunner_->OrderRaw_.Request() == &req);
         return this->CurrRunner_;
      }
      // 來到這裡, 建立新的 runner:
      // 從 OnNewTradingLineReady() 進入 OmsCore 之後, 從 Queue 取出送單.
      assert(tmpRunner.get() == nullptr && this->CurrOmsResource_ != nullptr);
      return tmpRunner.emplace(*this->CurrOmsResource_,
                               *req.LastUpdated()->Order().BeginUpdate(req),
                               exLogForUpdArg);
   }
   /// \retval true  已填妥 OrdNo.
   /// \retval false 已填妥了拒絕狀態.
   bool AllocOrdNo(OmsRequestRunnerInCore& runner) {
      return runner.AllocOrdNo_IniOrTgid(this->OrdTeamGroupId_);
   }
};
fon9_WARN_POP;
using TwsTradingLineMgrSP = fon9::intrusive_ptr<TwsTradingLineMgr>;

/// 會在 owner 加入 2 個 seed:
/// - (ioargs.Name_ + "_io"): TwsTradingLineMgr
///   - 使用 cfgpath + ioargs.Name_ + "_io" + ".f9gv" 綁定設定檔.
/// - (ioargs.Name_ + "_cfg"): TwsTradingLineMgrCfgSeed
///   - 使用 cfgpath + ioargs.Name_ + "_cfg" + ".f9gv" 綁定設定檔.
/// - 如果名稱重複, 則傳回 nullptr;
TwsTradingLineMgrSP CreateTradingLineMgr(fon9::seed::MaTree&  owner,
                                         std::string          cfgpath,
                                         fon9::IoManagerArgs& ioargs,
                                         f9fmkt_TradingMarket mkt);

} // namespaces
#endif//__f9omstws_OmsTwsTradingLineMgr_hpp__
