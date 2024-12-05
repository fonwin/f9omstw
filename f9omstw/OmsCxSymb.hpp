// \file f9omstw/OmsCxSymb.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCxSymb_hpp__
#define __f9omstw_OmsCxSymb_hpp__
#include "f9omstw/OmsCxUnit.hpp"

namespace f9omstw {

/// 放在 Symb 裡面, 提供 CxUnit 註冊, 當 Symb 有異動時提供通知;
class OmsCxSymb {
   fon9_NON_COPY_NON_MOVE(OmsCxSymb);
public:
   using CondReq = OmsCxReqToSymb;

   OmsCxSymb() = default;
   ~OmsCxSymb();

   OmsRequestRunStep* DelCondReq(const OmsSymb::MdLocker& mdLocker, const OmsCxUnit& cxUnit) {
      (void)mdLocker; assert(mdLocker.owns_lock());
      return this->CondReqList_.Del(cxUnit);
   }

protected:
   void CondReqList_Add(const OmsSymb::MdLocker& mdLocker, const CondReq& src) {
      (void)mdLocker; assert(mdLocker.owns_lock());
      this->CondReqList_.Add(src);
   }

   void LockReqList() { this->CondReqList_.lock(); }
   void UnlockReqList() { this->CondReqList_.unlock(); }

   struct CondReqListImpl : public std::vector<CondReq> {
      fon9_NON_COPY_NON_MOVE(CondReqListImpl);
      CondReqListImpl() = default;
      OmsCxSrcEvMask CheckCondReq(OmsCxMdEvArgs& args, OnOmsCxMdEvFnT onOmsCxEvFn);
   };
   using CondReqListBase = fon9::MustLock<CondReqListImpl>;
   CondReqListBase::Locker LockCondReqList() { return this->CondReqList_.Lock(); }

   #ifdef _DEBUG
      CondReqListImpl& GetCondReqList_ForDebug() { return this->CondReqList_.GetCondReqList_ForDebug(); }
   #endif
private:
   struct CondReqComp {
      bool operator()(const CondReq& lhs, uint64_t rhs) const {
         return lhs.Priority_ >= rhs;
      }
      bool operator()(uint64_t lhs, const CondReq& rhs) const {
         return lhs >= rhs.Priority_;
      }
   };
   struct CondReqList : public CondReqListBase {
      fon9_NON_COPY_NON_MOVE(CondReqList);
      CondReqList() = default;
      void lock() { this->Mutex_.lock(); }
      void unlock() { this->Mutex_.unlock(); }
      void Add(const CondReq& src) {
         CondReqListImpl::iterator ipos = std::upper_bound(this->Base_.begin(), this->Base_.end(), src.Priority_, CondReqComp{});
         this->Base_.insert(ipos, src);
      }
      OmsRequestRunStep* Del(const OmsCxUnit& cxUnit);
      #ifdef _DEBUG
         CondReqListImpl& GetCondReqList_ForDebug() { return this->Base_; }
      #endif
   };
   CondReqList CondReqList_;
};

} // namespaces
#endif//__f9omstw_OmsCxSymb_hpp__
