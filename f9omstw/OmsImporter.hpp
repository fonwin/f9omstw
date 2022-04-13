// \file f9omstw/OmsImporter.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsImporter_hpp__
#define __f9omstw_OmsImporter_hpp__
#include "f9omstw/OmsCoreMgr.hpp"
#include "fon9/seed/FileImpTree.hpp"

namespace f9omstw {

class OmsFileImpTree;

class OmsFileImpCoreRun {
   OmsCoreSP      Core_;
   OmsResource*   CoreRes_;

protected:
   void ResetCoreResource(OmsResource* res) {
      if ((this->CoreRes_ = res) != nullptr)
         this->Core_.reset(&res->Core_);
   }

public:
   OmsFileImpCoreRun() : CoreRes_{} {
   }
   OmsFileImpCoreRun(OmsCoreSP core, OmsResource* coreRes)
      : Core_{std::move(core)}
      , CoreRes_{coreRes} {
   }
   /// 是否直接使用 OmsResource 執行 task?
   /// 若非直接使用 OmsResource, 則需透過 OmsCore 執行.
   bool IsUseOmsResourceDirect() const {
      return this->CoreRes_ != nullptr;
   }
   bool RunCoreTask(OmsCoreTask&& task) {
      if (this->CoreRes_) {
         task(*this->CoreRes_);
         return true;
      }
      return this->Core_->RunCoreTask(std::move(task));
   }
   void CoreLogAppend(fon9::RevBufferList&& rbuf) {
      if (this->CoreRes_)
         this->CoreRes_->LogAppend(std::move(rbuf));
      else
         this->Core_->LogAppend(std::move(rbuf));
   }
};

fon9_WARN_DISABLE_PADDING;
class OmsFileImpLoader : public fon9::seed::FileImpLoader, public OmsFileImpCoreRun {
   fon9_NON_COPY_NON_MOVE(OmsFileImpLoader);
   using base = OmsFileImpCoreRun;

public:
   /// 若 OmsCore 尚未啟動, 則 CoreYYYYMMDD_ 為 0;
   const unsigned  CoreYYYYMMDD_;

   OmsFileImpLoader(const OmsFileImpTree& owner);
   OmsFileImpLoader(OmsCoreSP core, OmsResource* coreRes)
      : base(core, coreRes)
      , CoreYYYYMMDD_{fon9::GetYYYYMMDD(coreRes ? coreRes->TDay() : core->TDay())} {
   }
};
using OmsFileImpLoaderSP = fon9::intrusive_ptr<OmsFileImpLoader>;
fon9_WARN_POP;

class OmsFileImpMgr : public fon9::seed::FileImpMgr {
   fon9_NON_COPY_NON_MOVE(OmsFileImpMgr);
   using base = fon9::seed::FileImpMgr;

public:
   using base::base;

   /// 立即全部載入一次, 並啟動排程檢查.
   virtual void OnAfter_InitCoreTables(f9omstw::OmsResource& res);
};

class OmsFileImpTree : public fon9::seed::FileImpTree {
   fon9_NON_COPY_NON_MOVE(OmsFileImpTree);
   using base = fon9::seed::FileImpTree;
   friend class OmsFileImpMgr;
   // 為了避免衍生者沒有呼叫 InitCoreTables_BeforeLoadAll(); InitCoreTables_AfterLoadAll();
   // 所以在 OmsFileImpMgr::OnAfter_InitCoreTables() 設定並清除此值.
   OmsResource*   CoreRes_{};

public:
   OmsCoreMgr&    CoreMgr_;

   template <class... ArgsT>
   OmsFileImpTree(OmsFileImpMgr& cfgMgr, OmsCoreMgr& coreMgr, ArgsT&&... args)
      : base(cfgMgr, std::forward<ArgsT>(args)...)
      , CoreMgr_(coreMgr) {
   }
   ~OmsFileImpTree();

   bool IsReady() const {
      return this->CoreMgr_.CurrentCore().get() != nullptr
         || this->CoreRes_ != nullptr;
   }
   OmsResource* CoreRes() const {
      return this->CoreRes_;
   }

   /// 預設: do nothing.
   /// 在 InitCoreTables 之後, 全部重新載入之前.
   virtual void InitCoreTables_BeforeLoadAll(OmsResource& res);
   /// 預設: do nothing.
   /// 在 InitCoreTables 之後, 全部重新載入之後.
   virtual void InitCoreTables_AfterLoadAll(OmsResource& res);
};

class OmsFileImpSeed : public fon9::seed::FileImpSeed {
   fon9_NON_COPY_NON_MOVE(OmsFileImpSeed);
   using base = fon9::seed::FileImpSeed;

protected:
   using FileImpLoaderSP = fon9::seed::FileImpLoaderSP;
   using FileImpMonitorFlag = fon9::seed::FileImpMonitorFlag;
   FileImpLoaderSP OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, FileImpMonitorFlag monFlag) override;
   virtual OmsFileImpLoaderSP MakeLoader(OmsFileImpTree& owner, fon9::RevBuffer& rbufDesp, uint64_t addSize, FileImpMonitorFlag monFlag) = 0;

public:
   using base::base;

   fon9::TimeInterval Reload(ConfigLocker&& lk, std::string fname, bool isClearAddTailRemain) override;
};

inline OmsFileImpLoader::OmsFileImpLoader(const OmsFileImpTree& owner)
   : OmsFileImpLoader(owner.CoreMgr_.CurrentCore(), owner.CoreRes()) {
}

/// 用 fon9::RevBufferList rbuf; 建立訊息,
/// 然後記錄在 OmsLog 裡面: OmsResource.LogAppend(fon9::RevBufferList{128, fon9::BufferList{retval}});
/// 因為 c++11 的 lambda 尚未支援 std::move(); 所以只好傳遞 BufferNode*;
fon9::BufferNode* MakeOmsImpLog(fon9::seed::FileImpSeed& imp, fon9::seed::FileImpMonitorFlag monFlag, size_t itemsCount);

} // namespaces
#endif//__f9omstw_OmsImporter_hpp__
