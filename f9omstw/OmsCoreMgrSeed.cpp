// \file f9omstw/OmsCoreMgrSeed.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCoreMgrSeed.hpp"
#include "f9omstw/OmsCoreByThread.hpp"
#include "fon9/seed/SysEnv.hpp"
#include "fon9/TimedFileName.hpp"

namespace f9omstw {

OmsCoreMgrSeed::OmsCoreMgrSeed(std::string name, fon9::seed::MaTreeSP owner, OmsCoreMgrSP coreMgr)
   : base(std::move(coreMgr), std::move(name))
   , Root_{std::move(owner)} {
}
OmsCoreMgrSeed::OmsCoreMgrSeed(std::string name, fon9::seed::MaTreeSP owner, FnSetRequestLgOut fnSetRequestLgOut)
   : OmsCoreMgrSeed(std::move(name), std::move(owner), new OmsCoreMgr{fnSetRequestLgOut}) {
}

bool OmsCoreMgrSeed::AddCore(fon9::TimeStamp tday) {
   fon9::RevBufferFixedSize<128> rbuf;
   fon9::RevPrint(rbuf, this->Name_, '_', fon9::GetYYYYMMDD(tday));
   std::string coreName = rbuf.ToStrT<std::string>(); //      = omstws_yyyymmdd
   fon9::RevPrint(rbuf, this->Name_, '/'); // seedName = omstws/omstws_yyyymmdd

   auto fnCoreCreator = GetOmsCoreByThreadCreator(this->HowWait_);
   OmsCoreByThreadBaseSP core = fnCoreCreator(
      std::bind(&OmsCoreMgrSeed::InitCoreTables, this, std::placeholders::_1),
      tday, 0,
      &this->GetOmsCoreMgr(),
      rbuf.ToStrT<std::string>(),
      std::move(coreName)
   );

   fon9::TimedFileName logfn(fon9::seed::SysEnv_GetLogFileFmtPath(*this->Root_), fon9::TimedFileName::TimeScale::Day);
   // oms log 檔名與 TDay 相關, 與 TimeZone 無關, 所以要排除 TimeZone;
   logfn.RebuildFileNameExcludeTimeZone(tday);
   return core->StartToCoreMgr(logfn.GetFileName() + this->Name_ + ".log",
                               this->CoreCpuId_, this->BackendFlushInterval_, this->BackendCpuId_);
}

} // namespaces
