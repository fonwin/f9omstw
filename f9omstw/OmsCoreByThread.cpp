// \file f9omstw/OmsCoreByThread.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCoreByThread.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "fon9/Tools.hpp"
#include "fon9/TypeName.hpp"

namespace f9omstw {

bool OmsCoreByThreadBase::StartToCoreMgr(fon9::TimeStamp tday, std::string logFileName, int cpuId) {
   this->CpuId_ = cpuId;
   this->SetTitle(logFileName);
   auto res = this->Start(tday, std::move(logFileName));
   if (res.IsError())
      this->SetDescription(fon9::RevPrintTo<std::string>(res));
   else
      this->StartThrRun();
   // 必須在載入完畢後再加入 CoreMgr, 否則可能造成 TDayChanged 事件處理者, 沒有取得完整的資料表.
   return this->Owner_->Add(this);
}
//--------------------------------------------------------------------------//
OmsThreadTaskHandler::OmsThreadTaskHandler(OmsCoreByThreadBase& owner)
   : Owner_(owner) {
}
void OmsThreadTaskHandler::InitializeFromMQ() {
   fon9_LOG_INFO("OmsThread|name=", this->Owner_.Name_,
                 "|Type=", fon9::TypeName{this->Owner_}.get(),
                 "|Cpu=", this->Owner_.CpuId_, ':', fon9::SetCpuAffinity(this->Owner_.CpuId_));
   this->Owner_.SetThisThreadId();
   this->PendingReqsInThr_.reserve(1024 * 10);
}
void OmsThreadTaskHandler::OnThreadEnd(const std::string& threadName) {
   (void)threadName;
   this->Owner_.Backend_.WaitThreadEnd();
}

} // namespaces
