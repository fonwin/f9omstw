// \file f9omstw/OmsRequestFilled.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestFilled_hpp__
#define __f9omstw_OmsRequestFilled_hpp__
#include "f9omstw/OmsRequestBase.hpp"

namespace f9omstw {

/// 成交回報.
/// 在 OmsOrder 提供:
/// \code
///    OmsRequestFilled* OmsOrder::FilledHead_;
///    OmsRequestFilled* OmsOrder::FilledLast_;
/// \endcode
/// 新的成交, 如果是在 FilledLast_->MatchKey_ 之後, 就直接 append; 否則就從頭搜尋.
/// 由於成交回報「大部分」是附加到尾端, 所以這樣的處理負擔應是最小.
class OmsRequestFilled : public OmsRequestBase {
   fon9_NON_COPY_NON_MOVE(OmsRequestFilled);
   using base = OmsRequestBase;

   const OmsRequestFilled mutable* Next_{nullptr};

   /// 將 curr 插入 this 與 this->Next_ 之間;
   void InsertAfter(const OmsRequestFilled* curr) const {
      assert(curr->Next_ == nullptr && this->MatchKey_ < curr->MatchKey_);
      assert(this->Next_ == nullptr || curr->MatchKey_ < this->Next_->MatchKey_);
      curr->Next_ = this->Next_;
      this->Next_ = curr;
   }

   OmsRxSNO IniSNO_;
   uint64_t MatchKey_{0};

   static void MakeFieldsImpl(fon9::seed::Fields& flds);
protected:
   template <class Derived>
   static void MakeFields(fon9::seed::Fields& flds) {
      static_assert(fon9_OffsetOf(Derived, IniSNO_) == fon9_OffsetOf(OmsRequestFilled, IniSNO_),
                    "'OmsRequestFilled' must be the first base class in derived.");
      MakeFieldsImpl(flds);
   }

public:
   using MatchKey = uint64_t;

   OmsRequestFilled(OmsRequestFactory& creator)
      : base{creator, f9fmkt_RxKind_Filled} {
   }
   OmsRequestFilled()
      : base{f9fmkt_RxKind_Filled} {
   }

   /// 將 curr 依照 MatchKey_ 的順序(小到大), 加入到「成交串列」.
   /// \retval nullptr  成功將 curr 加入成交串列.
   /// \retval !nullptr 與 curr->MatchKey_ 相同的那個 request.
   static const OmsRequestFilled* Insert(const OmsRequestFilled** ppHead, const OmsRequestFilled** ppLast, const OmsRequestFilled* curr);

   const OmsRequestFilled* Next() {
      return this->Next_;
   }

   const OmsRequestBase* PreCheck_GetRequestInitiator(OmsRequestRunner& runner, OmsResource& res) {
      return base::PreCheck_GetRequestInitiator(runner, &this->IniSNO_, res);
   }
};

} // namespaces
#endif//__f9omstw_OmsRequestFilled_hpp__
