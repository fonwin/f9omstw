// \file f9omstw/OmsImporter.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsImporter.hpp"

namespace f9omstw {

void OmsFileImpMgr::OnAfter_InitCoreTables(f9omstw::OmsResource& res) {
   this->StopAndWait_SchTask();
   assert(dynamic_cast<OmsFileImpTree*>(&this->GetFileImpSapling()) != nullptr);
   OmsFileImpTree* imp = static_cast<OmsFileImpTree*>(&this->GetFileImpSapling());
   imp->CoreRes_ = &res;
   imp->InitCoreTables_BeforeLoadAll(res);
   this->LoadAll("OnAfter_InitCoreTables", fon9::UtcNow());
   imp->InitCoreTables_AfterLoadAll(res);
   imp->CoreRes_ = nullptr;
   this->Restart_SchTask(fon9::TimeInterval{});
}

OmsFileImpTree::~OmsFileImpTree() {
}
void OmsFileImpTree::InitCoreTables_BeforeLoadAll(OmsResource& res) {
   (void)res;
}
void OmsFileImpTree::InitCoreTables_AfterLoadAll(OmsResource& res) {
   (void)res;
}

fon9::TimeInterval OmsFileImpSeed::Reload(ConfigLocker&& lk, std::string fname, bool isClearAddTailRemain) {
   assert(dynamic_cast<OmsFileImpTree*>(&this->OwnerTree_) != nullptr);
   if (static_cast<OmsFileImpTree*>(&this->OwnerTree_)->IsReady())
      return base::Reload(std::move(lk), std::move(fname), isClearAddTailRemain);
   return fon9::TimeInterval_Second(1);
}
fon9::seed::FileImpLoaderSP OmsFileImpSeed::OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, FileImpMonitorFlag monFlag) {
   assert(dynamic_cast<OmsFileImpTree*>(&this->OwnerTree_) != nullptr);
   if (static_cast<OmsFileImpTree*>(&this->OwnerTree_)->IsReady())
      return this->MakeLoader(*static_cast<OmsFileImpTree*>(&this->OwnerTree_), rbufDesp, addSize, monFlag);
   return nullptr;
}

fon9::BufferNode* MakeOmsImpLog(OmsFileImpSeed& imp, fon9::seed::FileImpMonitorFlag monFlag, size_t itemsCount) {
   fon9::RevBufferList rbuf{128};
   fon9::RevPrint(rbuf, fon9::LocalNow(),
                  '|', imp.Name_, "|mon=", monFlag,
                  "|ftime=", imp.LastFileTime(), fon9::kFmtYMD_HH_MM_SS_us_L,
                  "|items=", itemsCount,
                  '\n');
   return rbuf.MoveOut().ReleaseList();
}

} // namespaces
