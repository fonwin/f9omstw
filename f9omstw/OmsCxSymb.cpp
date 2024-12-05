// \file f9omstw/OmsCxSymb.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCxSymb.hpp"
#include "f9omstw/OmsCxReqBase.hpp"

namespace f9omstw {

OmsCxSymb::~OmsCxSymb() {
}
OmsRequestRunStep* OmsCxSymb::CondReqList::Del(const OmsCxUnit& cxUnit) {
   CondReqListImpl::iterator const iend = this->Base_.end();
   for (CondReqListImpl::iterator ipos = this->Base_.begin(); ipos != iend; ++ipos) {
      if (ipos->CxUnit_ == &cxUnit) {
         OmsRequestRunStep* retval = ipos->NextStep_;
         this->Base_.erase(ipos);
         return retval;
      }
   }
   return nullptr;
}
OmsCxSrcEvMask OmsCxSymb::CondReqListImpl::CheckCondReq(OmsCxMdEvArgs& args, OnOmsCxMdEvFnT onOmsCxEvFn) {
   const char*    pbufTail = args.LogRBuf_.GetCurrent();
   OmsCxSrcEvMask afEvMask{};
   for (size_t idx = this->size(); idx > 0;) {
      const CondReq& creq = (*this)[--idx];
      switch (creq.CxRequest_->OnOmsCxMdEv(creq, onOmsCxEvFn, args)) {
      case OmsCxCheckResult::ContinueWaiting:
         // [idx]的條件單仍然有效,且此次異動判斷未成立,所以:
         // 保留[idx],下次有異動時繼續判斷;
         afEvMask |= creq.CxUnit_->RegEvMask();
         continue;
      case OmsCxCheckResult::FireNow:
         args.LogRBuf_.SetPrefixUsed(const_cast<char*>(pbufTail));
         break;
      default:
      case OmsCxCheckResult::TrueLocked:
      case OmsCxCheckResult::AlreadyFired:
         break;
      }
      // 從 condList 移除: (1)Order或條件已經失效; (2)條件成立已交給OmsCore處理;
      this->erase(this->begin() + fon9::signed_cast(idx));
   }
   return afEvMask;
}

} // namespaces
