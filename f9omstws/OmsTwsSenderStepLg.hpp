// \file f9omstws/OmsTwsSenderStepLg.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsSenderStepLg_hpp__
#define __f9omstws_OmsTwsSenderStepLg_hpp__
#include "f9omstw/OmsRequestRunner.hpp"
#include "f9omstws/OmsTwsTradingLineMgr.hpp"

namespace f9omstw {

class TwsTradingLineMgrLg;
using TwsTradingLineMgrLgSP = fon9::intrusive_ptr<TwsTradingLineMgrLg>;

/// 最多提供 '0'..'9', 'A'..'Z' 共 36 個線路群組.
/// - '0' 為預設線路群組, 當找不到適當的群組時使用.
/// - 從設定檔載入提供那些群組及名稱.
/// - Sapling:
///   - "Lg0", Title="通用線路"
///      - "Lg0_OTC_io"
///      - "Lg0_OTC_cfg"
///      - "Lg0_TSE_io"
///      - "Lg0_TSE_cfg"
///   - "LgF", Title="節費線路"
///      - "LgF_OTC_io"
///      - "LgF_OTC_cfg"
///      - "LgF_TSE_io"
///      - "LgF_TSE_cfg"
///   - "LgV", Title="VIP線路"
///      - "LgV_OTC_io"
///      - "LgV_OTC_cfg"
///      - "LgV_TSE_io"
///      - "LgV_TSE_cfg"
class TwsTradingLineMgrLg : public fon9::seed::NamedMaTree {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineMgrLg);
   using base = fon9::seed::NamedMaTree;
   OmsCoreMgr&    CoreMgr_;
   fon9::SubConn  SubrTDayChanged_{};
   void OnTDayChanged(OmsCore& core);
public:
   class LgMgr : public fon9::seed::NamedMaTree {
      fon9_NON_COPY_NON_MOVE(LgMgr);
      using base = fon9::seed::NamedMaTree;
   public:
      using base::base;
      TwsTradingLineMgrSP  TseTradingLineMgr_;
      TwsTradingLineMgrSP  OtcTradingLineMgr_;
   };

   TwsTradingLineMgrLg(OmsCoreMgr& coreMgr, std::string name);
   ~TwsTradingLineMgrLg();

   const LgMgr* GetLgMgr(LgOut lg) const {
      auto idx = LgOutToIndex(lg);
      if (const LgMgr* retval = this->LgMgrs_[idx].get())
         return retval;
      return this->LgMgrs_[0].get();
   }

   /// - 使用 iocfgs.CfgFileName_ 載入設定檔
   ///   - 如果設定檔不存在, holder.SetPluginsSt(fon9::LogLevel::Error...), 返回 nullptr;
   ///   - IoTse = {上市交易線路 IoService 參數} 可使用 IoTse=/MaIo 表示與 /MaIo 共用 IoService
   ///   - IoOtc = {上櫃交易線路 IoService 參數} 可使用 IoOtc=/MaIo 或 IoTse 表示共用 IoService
   ///   - 預設的 IoServiceArgs: 請參考: "fon9/io/IoServiceArgs.hpp"
   ///   - Lg0 = Title, Description
   ///   - 依照設定檔出現的順序建立 LgMgr;
   /// - 線路設定檔的路徑: cfgpath = FilePath::ExtractPathName(&iocfgs.CfgFileName_);
   ///   - cfgpath + "Lg0_OTC_io.f9gv"
   ///   - cfgpath + "Lg0_OTC_cfg.f9gv"
   ///   - cfgpath + "Lg0_TSE_io.f9gv"
   ///   - cfgpath + "Lg0_TSE_cfg.f9gv"
   /// - retval 使用 iocfgs.Name_ 加入 coreMgr.
   static TwsTradingLineMgrLgSP Plant(OmsCoreMgr&                coreMgr,
                                      fon9::seed::PluginsHolder& holder,
                                      fon9::IoManagerArgs&       iocfgs);

private:
   using LgMgrSP = fon9::intrusive_ptr<LgMgr>;
   LgMgrSP LgMgrs_[static_cast<uint8_t>(LgOut::Count)];
};

/// 根據 Request.LgOut_ 選用適當的線路群組.
class OmsTwsSenderStepLg : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(OmsTwsSenderStepLg);
   using base = OmsRequestRunStep;
   using base::base;

public:
   TwsTradingLineMgrLg&  LineMgr_;
   OmsTwsSenderStepLg(TwsTradingLineMgrLg&);
   void RunRequest(OmsRequestRunnerInCore&& runner) override;
   void RerunRequest(OmsReportRunnerInCore&& runner) override;
};

} // namespaces
#endif//__f9omstws_OmsTwsSenderStepLg_hpp__
