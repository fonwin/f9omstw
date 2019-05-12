// \file f9omstw/OmsRequestMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestMgr_hpp__
#define __f9omstw_OmsRequestMgr_hpp__
#include "f9omstw/OmsRequest.hpp"

namespace f9omstw {

class OmsRequestMgr {
   fon9_NON_COPYABLE(OmsRequestMgr);

   using Requests = std::deque<OmsRequestSP>;
   Requests       Requests_;
   OmsRequestSNO  LastSNO_{0};

   struct ReqUID_Builder {
      char        Buffer_[64 + sizeof(OmsRequestId)];
      ReqUID_Builder();
      char* RevStart() {
         return this->Buffer_ + sizeof(this->Buffer_) - sizeof(OmsRequestId);
      }
   };
   ReqUID_Builder UIDBuilder_;

public:
   enum : size_t {
   #ifdef NDEBUG
      /// 建構時, 容器預留的大小.
      kRequestsReserveIni = (1024 * 1024 * 10),
      /// 當新增 Request 容量不足時, 擴充的大小.
      kRequestsReserveExp = (1024),
   #else
      kRequestsReserveIni = (16),
      kRequestsReserveExp = (4),
   #endif
   };

   OmsRequestMgr(size_t reserveSize = kRequestsReserveIni) {
      this->Requests_.resize(reserveSize);
   }
   OmsRequestMgr(OmsRequestMgr&& rhs)
      : Requests_{std::move(rhs.Requests_)}
      , LastSNO_{rhs.LastSNO_} {
      rhs.LastSNO_ = 0;
   }
   ~OmsRequestMgr();

   const OmsRequestBase* Get(OmsRequestSNO sno) const {
      return(sno > this->LastSNO_ ? nullptr : this->Requests_[sno].get());
   }

   /// 此時若 *req->ReqUID_.begin() == '\0'; 則會編製 req->ReqUID_;
   void Append(OmsRequestSP req);
};

} // namespaces
#endif//__f9omstw_OmsRequestMgr_hpp__
