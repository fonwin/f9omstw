// \file f9omstw/OmsBackendRecorder.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsBackend.hpp"
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/RevPrint.hpp"

namespace f9omstw {

fon9::File::Result OmsBackend::OpenLoad(std::string logFileName) {
   assert(!this->RecorderFd_.IsOpened());
   auto res = this->RecorderFd_.Open(logFileName,
                                     fon9::FileMode::CreatePath | fon9::FileMode::Append
                                     | fon9::FileMode::Read | fon9::FileMode::DenyWrite);
   if (res.IsError())
      return res;
   res = this->RecorderFd_.GetFileSize();
   if (res.IsError())
      return res;
   if (res.GetResult() != 0) {
      if (0);// TODO: Load from logFileName.
      this->RecorderFd_.Append("\n", 1);
   }
   fon9::RevBufferList rbuf{128};
   if (0);// TODO: output layout to RecorderFd_.
   fon9::RevPrint(rbuf, "===== OMS start @ ", fon9::UtcNow(), " | ", logFileName, " =====\n");
   fon9::DcQueueList dcq{rbuf.MoveOut()};
   this->RecorderFd_.Append(dcq);
   return res;
}
void OmsBackend::SaveQuItems(QuItems& quItems) {
   fon9::DcQueueList dcq;
   for (QuItem& qi : quItems) {
      if (auto exsz = fon9::CalcDataSize(qi.ExLog_.cfront()))
         fon9::RevPrint(qi.ExLog_, *fon9_kCSTR_CELLSPL, exsz + 1, *fon9_kCSTR_CELLSPL);
      if (qi.Item_) {
         fon9::RevPrint(qi.ExLog_, *fon9_kCSTR_ROWSPL);
         qi.Item_->RevPrint(qi.ExLog_);
         fon9::RevPrint(qi.ExLog_, qi.Item_->RxSNO_, *fon9_kCSTR_CELLSPL);
      }
      dcq.push_back(qi.ExLog_.MoveOut());
   }
   if (!dcq.empty())
      this->RecorderFd_.Append(dcq);
}

} // namespaces
