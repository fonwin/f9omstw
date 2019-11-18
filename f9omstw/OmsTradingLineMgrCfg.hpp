// \file f9omstw/OmsTradingLineMgrCfg.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsTradingLineMgrCfg_hpp__
#define __f9omstw_OmsTradingLineMgrCfg_hpp__
#include "fon9/seed/MaConfigTree.hpp"

namespace f9omstw {

class OmsTradingLineMgrBase;

/// OmsTradingLineMgr 的相關設定.
/// - 提供 seed 查看、修改.
/// - 啟動時載入、異動時儲存.
/// - 有異動時, 通知 OmsTradingLineMgr.
/// - 建構時主動加入的設定:
///   - "OrdTeams": "可用櫃號", "使用「,」分隔櫃號, 使用「-」設定範圍"
class OmsTradingLineMgrCfgSeed : public fon9::seed::MaConfigMgr {
   fon9_NON_COPY_NON_MOVE(OmsTradingLineMgrCfgSeed);
   using base = fon9::seed::MaConfigMgr;
   struct CfgOrdTeams;

public:
   OmsTradingLineMgrBase&  LineMgr_;

   OmsTradingLineMgrCfgSeed(OmsTradingLineMgrBase& lineMgr, std::string name);

   void BindConfigFile(fon9::StrView cfgpath);
};

} // namespaces
#endif//__f9omstw_OmsTradingLineMgrCfg_hpp__
