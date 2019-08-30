// \file f9omstw/OmsCoreMgrSeed.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCoreMgrSeed.hpp"
#include "f9omstw/OmsCoreByThread.hpp"
#include "fon9/seed/SysEnv.hpp"
#include "fon9/TimedFileName.hpp"

namespace f9omstw {

OmsCoreMgrSeed::OmsCoreMgrSeed(std::string name, fon9::seed::MaTreeSP owner)
   : base(new OmsCoreMgr{"ConfigsTab"}, std::move(name))
   , Root_{std::move(owner)} {
}

bool OmsCoreMgrSeed::SetIoManager(fon9::seed::PluginsHolder& holder, fon9::IoManagerArgs& out) {
   if (*out.IoServiceCfgstr_.c_str() == '/') {
      fon9::StrView name{&out.IoServiceCfgstr_};
      name.SetBegin(name.begin() + 1);
      out.IoServiceSrc_ = holder.Root_->GetSapling<fon9::IoManager>(name);
      if (!out.IoServiceSrc_) {
         holder.SetPluginsSt(fon9::LogLevel::Error,
                             "OmsCoreMgrSeed.SetIoManager|err=Unknown IoManager: ",
                             out.IoServiceCfgstr_);
         return false;
      }
   }
   return true;
}

bool OmsCoreMgrSeed::AddCore(fon9::TimeStamp tday) {
   fon9::RevBufferFixedSize<128> rbuf;
   fon9::RevPrint(rbuf, this->Name_, '_', fon9::GetYYYYMMDD(tday));
   std::string coreName = rbuf.ToStrT<std::string>(); //      = omstws_yyyymmdd
   fon9::RevPrint(rbuf, this->Name_, '/'); // seedName = omstws/omstws_yyyymmdd

   auto fnCoreCreator = GetOmsCoreByThreadCreator(this->HowWait_);
   OmsCoreByThreadBaseSP core = fnCoreCreator(
      std::bind(&OmsCoreMgrSeed::InitCoreTables, this, std::placeholders::_1),
      &this->GetOmsCoreMgr(),
      rbuf.ToStrT<std::string>(),
      std::move(coreName)
   );

   fon9::TimedFileName logfn(fon9::seed::SysEnv_GetLogFileFmtPath(*this->Root_), fon9::TimedFileName::TimeScale::Day);
   // oms log 檔名與 TDay 相關, 與 TimeZone 無關, 所以要扣除 logfn.GetTimeChecker().GetTimeZoneOffset();
   logfn.RebuildFileName(tday - logfn.GetTimeChecker().GetTimeZoneOffset());
   return core->StartToCoreMgr(tday, logfn.GetFileName() + this->Name_ + ".log", this->CpuId_);
}

} // namespaces
