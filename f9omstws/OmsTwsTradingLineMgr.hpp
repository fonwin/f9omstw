// \file f9omstws/OmsTwsTradingLineMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsTradingLineMgr_hpp__
#define __f9omstws_OmsTwsTradingLineMgr_hpp__
#include "f9tws/ExgTradingLineMgr.hpp"
#include "f9omstw/OmsTradingLineMgr.hpp"

namespace f9omstw {

using TwsTradingLineMgr = OmsTradingLineMgrT<f9tws::ExgTradingLineMgr>;
using TwsTradingLineMgrSP = fon9::intrusive_ptr<TwsTradingLineMgr>;

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
