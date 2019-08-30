// \file f9omstw/OmsCoreMgrSeed.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCoreMgrSeed_hpp__
#define __f9omstw_OmsCoreMgrSeed_hpp__
#include "f9omstw/OmsCoreMgr.hpp"
#include "fon9/seed/Plugins.hpp"
#include "fon9/framework/IoManager.hpp"

namespace f9omstw {

class OmsCoreMgrSeed : public fon9::seed::NamedSapling {
   fon9_NON_COPY_NON_MOVE(OmsCoreMgrSeed);
   using base = fon9::seed::NamedSapling;
protected:
   int            CpuId_{-1};
   fon9::HowWait  HowWait_{};

   virtual void InitCoreTables(OmsResource& res) = 0;

public:
   const fon9::seed::MaTreeSP Root_;

   OmsCoreMgrSeed(std::string name, fon9::seed::MaTreeSP owner);
   OmsCoreMgrSeed(std::string name, fon9::seed::MaTreeSP owner, OmsCoreMgrSP coreMgr);

   OmsCoreMgr& GetOmsCoreMgr() const {
      return *static_cast<OmsCoreMgr*>(this->Sapling_.get());
   }

   /// 若 out.IoServiceCfgstr_ 第一碼為 '/' 則從 holder.Root_ 尋找 IoManager 填入 out.IoServiceSrc_;
   /// 例如: out.IoServiceCfgstr_ = "/MaIo";
   static bool SetIoManager(fon9::seed::PluginsHolder& holder, fon9::IoManagerArgs& out);

   /// 建立預設的 OmsCore.
   /// - GetOmsCoreByThreadCreator(this->HowWait_);
   /// - 透過 this->InitCoreTables(); 初始化所需要的資料表.
   /// - OmsCore.Name = this->Name_ + "_" + YYYYMMDD;
   /// - OmsCore.SeedName = this->Name_ + "/" + OmsCore.Name;
   bool AddCore(fon9::TimeStamp tday);
};

} // namespaces
#endif//__f9omstw_OmsCoreMgrSeed_hpp__
