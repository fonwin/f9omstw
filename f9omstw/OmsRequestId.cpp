﻿// \file f9omstw/OmsRequestId.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestId.hpp"
#include "f9omstw/OmsRequestBase.hpp"
#include "fon9/HostId.hpp"
#include "fon9/ToStr.hpp"

namespace f9omstw {

OmsReqUID_Builder::OmsReqUID_Builder() {
   memset(this->Buffer_, 0, sizeof(this->Buffer_));
   char* pout = fon9::UIntToStrRev(this->RevStart(), fon9::LocalHostId_) - 1;
   *pout = '.';
   memcpy(this->RevStart(), pout, static_cast<size_t>(this->RevStart() - pout));
}
void OmsReqUID_Builder::MakeReqUID(OmsRequestId& reqid, OmsRxSNO sno) {
   assert(sno != 0);
   if (OmsIsReqUIDEmpty(reqid)) {
      // [...ReqSNO.LocalHostId--------]
      //     \___ sizeof(ReqUID) ___/ copyto(req.ReqUID_)
      // memcpy(,,sizeof(req->ReqUID_)) 固定大小, 可以讓 compiler 最佳化.
      memcpy(reqid.ReqUID_.data(), fon9::UIntToStrRev(this->RevStart(), sno), sizeof(reqid.ReqUID_));
   }
}
OmsRxSNO OmsReqUID_Builder::ParseRequestId(const OmsRequestId& reqId) {
   return fon9::StrTo(fon9::StrView{reqId.ReqUID_.begin(), reqId.ReqUID_.end()}, OmsRxSNO{});
}

} // namespaces
