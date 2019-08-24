// \file f9omstws/OmsTwsTradingLineMgrCfg.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsTradingLineMgrCfg_hpp__
#define __f9omstws_OmsTwsTradingLineMgrCfg_hpp__
#include "fon9/seed/MaTreeConfig.hpp"

namespace f9omstw {

class TwsTradingLineMgr;

/// TwsTradingLineMgr 的相關設定.
/// - 提供 seed 查看、修改.
/// - 啟動時載入、異動時儲存.
/// - 有異動時, 通知 TwsTradingLineMgr.
class TwsTradingLineMgrCfgSeed : public fon9::seed::NamedSapling {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineMgrCfgSeed);
   using base = fon9::seed::NamedSapling;
   fon9::seed::MaTreeConfigItem* const  CfgOrdTeams_;
   struct CfgTree;

public:
   TwsTradingLineMgr&   LineMgr_;

   TwsTradingLineMgrCfgSeed(TwsTradingLineMgr& lineMgr,
                            const std::string& cfgpath,
                            std::string name);
};

} // namespaces
#endif//__f9omstws_OmsTwsTradingLineMgrCfg_hpp__
