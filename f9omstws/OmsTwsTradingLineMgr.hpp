// \file f9omstws/OmsTwsTradingLineMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsTradingLineMgr_hpp__
#define __f9omstws_OmsTwsTradingLineMgr_hpp__
#include "f9tws/ExgTradingLineMgr.hpp"
#include "f9omstw/OmsTradingLineMgr.hpp"

namespace f9omstw {

using TwsTradingLineMgr = OmsTradingLineMgrT<f9tws::ExgTradingLineMgr>;
using TwsTradingLineMgrSP = fon9::intrusive_ptr<TwsTradingLineMgr>;

class TwsTradingLineGroup {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineGroup);
public:
   using LineMgr = TwsTradingLineMgr;
   TwsTradingLineMgrSP  TseTradingLineMgr_;
   TwsTradingLineMgrSP  OtcTradingLineMgr_;

   TwsTradingLineGroup() = default;
   ~TwsTradingLineGroup();

   void OnTDayChangedInCore(OmsResource& resource) {
      assert(this->TseTradingLineMgr_.get() != nullptr);
      assert(this->OtcTradingLineMgr_.get() != nullptr);
      this->TseTradingLineMgr_->OnOmsCoreChanged(resource);
      this->OtcTradingLineMgr_->OnOmsCoreChanged(resource);
   }
   void SetTradingSessionSt(fon9::TimeStamp tday, f9fmkt_TradingMarket mkt, f9fmkt_TradingSessionId sesId, f9fmkt_TradingSessionSt sesSt) {
      if (mkt == f9fmkt_TradingMarket_TwSEC && this->TseTradingLineMgr_)
         this->TseTradingLineMgr_->SetTradingSessionSt(tday, sesId, sesSt);
      if (mkt == f9fmkt_TradingMarket_TwOTC && this->OtcTradingLineMgr_)
         this->OtcTradingLineMgr_->SetTradingSessionSt(tday, sesId, sesSt);
   }

   /// 根據 runner.OrderRaw_.Market() 取得 TwsTradingLineMgr;
   /// 若返回 nullptr, 則返回前, 會先執行 runner.Reject();
   TwsTradingLineMgr* GetLineMgr(OmsRequestRunnerInCore& runner) const {
      return this->GetLineMgr(runner.OrderRaw(), &runner);
   }
   TwsTradingLineMgr* GetLineMgr(const OmsOrderRaw& ordraw, OmsRequestRunnerInCore* runner) const {
      fon9_WARN_DISABLE_SWITCH;
      switch (ordraw.Market()) {
      case f9fmkt_TradingMarket_TwSEC:
         return this->TseTradingLineMgr_.get();
      case f9fmkt_TradingMarket_TwOTC:
         return this->OtcTradingLineMgr_.get();
      default:
         if (runner)
            runner->Reject(f9fmkt_TradingRequestSt_InternalRejected, OmsErrCode_Bad_MarketId, nullptr);
         return nullptr;
      }
      fon9_WARN_POP;
   }
};

/// 會在 owner 加入 2 個 seed:
/// - (ioargs.Name_ + "_io"): TwsTradingLineMgr
///   - 使用 cfgpath + ioargs.Name_ + "_io" + ".f9gv" 綁定設定檔.
/// - (ioargs.Name_ + "_cfg"): TwsTradingLineMgrCfgSeed
///   - 使用 cfgpath + ioargs.Name_ + "_cfg" + ".f9gv" 綁定設定檔.
/// - 如果名稱重複, 則傳回 nullptr;
TwsTradingLineMgrSP CreateTwsTradingLineMgr(fon9::seed::MaTree&  owner,
                                            std::string          cfgpath,
                                            fon9::IoManagerArgs& ioargs,
                                            f9fmkt_TradingMarket mkt);

} // namespaces
#endif//__f9omstws_OmsTwsTradingLineMgr_hpp__
