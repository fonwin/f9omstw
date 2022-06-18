// \file f9omstwf/OmsTwfSenderStepLgMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfSenderStepLgMgr_hpp__
#define __f9omstwf_OmsTwfSenderStepLgMgr_hpp__
#include "f9omstwf/OmsTwfTradingLineMgr.hpp"
#include "f9omstw/OmsTradingLineReqRunStep.hpp"

namespace f9omstw {

class TwfTradingLgMgr;
using TwfTradingLgMgrSP = fon9::intrusive_ptr<TwfTradingLgMgr>;

/// 最多提供 '0'..'9', 'A'..'Z' 共 36 個線路群組.
/// - '0' 為預設線路群組, 當找不到適當的群組時使用.
/// - 從設定檔載入提供那些群組及名稱.
/// - Sapling:
///   - "Lg0", Title="通用線路"
///      - "Lg0_FutNormal_io"
///      - "Lg0_FutNormal_cfg"
///      - "Lg0_OptNormal_io"
///      - "Lg0_OptNormal_cfg"
///      - "Lg0_FutAfterHour_io"
///      - "Lg0_FutAfterHour_cfg"
///      - "Lg0_OptAfterHour_io"
///      - "Lg0_OptAfterHour_cfg"
///   - "LgF", Title="節費線路"
///      - "LgF_FutNormal_io"
///      - "LgF_FutNormal_cfg"
///      - "LgF_OptNormal_io"
///      - "LgF_OptNormal_cfg"
///      - "LgF_FutAfterHour_io"
///      - "LgF_FutAfterHour_cfg"
///      - "LgF_OptAfterHour_io"
///      - "LgF_OptAfterHour_cfg"
class TwfTradingLgMgr : public OmsTradingLgMgrT<TwfTradingLineGroup> {
   fon9_NON_COPY_NON_MOVE(TwfTradingLgMgr);
   using base = OmsTradingLgMgrT<TwfTradingLineGroup>;
public:
   TwfTradingLgMgr(OmsCoreMgr& coreMgr, std::string name);
   ~TwfTradingLgMgr();

   /// - 使用 iocfgs.CfgFileName_ 載入設定檔
   ///   - 如果設定檔不存在, holder.SetPluginsSt(fon9::LogLevel::Error...), 返回 nullptr;
   ///   - IoFutN = {期貨  日盤交易線路 IoService 參數} 可使用 IoFutN=/MaIo 表示與 /MaIo 共用 IoService;
   ///   - IoOptN = {選擇權日盤交易線路 IoService 參數} 可使用 IoOptN=/MaIo 或 IoFutN 表示共用 IoService;
   ///   - IoFutA = {期貨  夜盤交易線路 IoService 參數} ...;
   ///   - IoOptA = {選擇權夜盤交易線路 IoService 參數} ...;
   ///   - 預設的 IoServiceArgs: 請參考: "fon9/io/IoServiceArgs.hpp"
   ///   - Lg0 = Title, Description
   ///   - 依照設定檔出現的順序建立 LgMgr;
   /// - 線路設定檔的路徑: cfgpath = FilePath::ExtractPathName(&iocfgs.CfgFileName_);
   ///   - cfgpath + "Lg0_FutNormal_io.f9gv"
   ///   - cfgpath + "Lg0_FutNormal_cfg.f9gv"
   ///   - cfgpath + "Lg0_OptNormal_io.f9gv"
   ///   - cfgpath + "Lg0_OptNormal_cfg.f9gv"
   ///   - cfgpath + "Lg0_FutAfterHour_io.f9gv"
   ///   - cfgpath + "Lg0_FutAfterHour_cfg.f9gv"
   ///   - cfgpath + "Lg0_OptAfterHour_io.f9gv"
   ///   - cfgpath + "Lg0_OptAfterHour_cfg.f9gv"
   /// - retval 使用 iocfgs.Name_ 加入 coreMgr.
   static TwfTradingLgMgrSP Plant(OmsCoreMgr&                coreMgr,
                                  fon9::seed::PluginsHolder& holder,
                                  fon9::IoManagerArgs&       iocfgs,
                                  f9twf::ExgMapMgrSP         exgMapMgr);
};

using OmsTwfSenderStepLgMgr = OmsTradingLineReqRunStepT<TwfTradingLgMgr>;

} // namespaces
#endif//__f9omstwf_OmsTwfSenderStepLgMgr_hpp__
