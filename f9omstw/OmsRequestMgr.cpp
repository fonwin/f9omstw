// \file f9omstw/OmsRequestMgr.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestMgr.hpp"
#include "fon9/HostId.hpp"

namespace f9omstw {

OmsRequestMgr::ReqUID_Builder::ReqUID_Builder() {
   // [buffer.LocalHostId--------]
   //        \_ sizeof(ReqUID) _/   '-' = '\0'
   memset(this->Buffer_, 0, sizeof(this->Buffer_));
   char* pout = fon9::UIntToStrRev(this->RevStart(), fon9::LocalHostId_) - 1;
   *pout = '.';
   memcpy(this->RevStart(), pout, static_cast<size_t>(this->RevStart() - pout));
}

OmsRequestMgr::~OmsRequestMgr() {
}
void OmsRequestMgr::Append(OmsRequestSP req) {
   assert(req.get() != nullptr && req->ReqSNO_ == 0);
   const auto snoCurr = req->ReqSNO_ = ++this->LastSNO_;
   if (fon9_UNLIKELY(snoCurr >= this->Requests_.size()))
      this->Requests_.resize(snoCurr + kRequestsReserveExp);
   if (*req->ReqUID_.begin() == '\0') {
      // [...ReqSNO.LocalHostId--------]
      //     \___ sizeof(ReqUID) ___/ copyto(req->ReqUID_)
      // memcpy(,,sizeof(req->ReqUID_)) 固定大小, 可以讓 compiler 最佳化.
      memcpy(req->ReqUID_.data(), fon9::UIntToStrRev(UIDBuilder_.RevStart(), snoCurr), sizeof(req->ReqUID_));
   }
   this->Requests_[snoCurr] = std::move(req);
}

} // namespaces
