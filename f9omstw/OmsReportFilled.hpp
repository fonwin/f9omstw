// \file f9omstw/OmsReportFilled.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsReportFilled_hpp__
#define __f9omstw_OmsReportFilled_hpp__
#include "f9omstw/OmsRequestBase.hpp"

namespace f9omstw {

/// 成交回報.
/// 在 OmsOrder 提供:
/// \code
///    OmsReportFilled* OmsOrder::FilledHead_;
///    OmsReportFilled* OmsOrder::FilledLast_;
/// \endcode
/// 新的成交, 如果是在 FilledLast_->MatchKey_ 之後, 就直接 append; 否則就從頭搜尋.
/// 由於成交回報「大部分」是附加到尾端, 所以這樣的處理負擔應是最小.
class OmsReportFilled : public OmsRequestBase {
   fon9_NON_COPY_NON_MOVE(OmsReportFilled);
   using base = OmsRequestBase;

   const OmsReportFilled mutable* Next_{nullptr};

   /// 將 curr 插入 this 與 this->Next_(可能為nullptr) 之間;
   /// 不支援: this 是新的 head, 因 curr 是舊的成交串列.
   void InsertAfter(const OmsReportFilled* curr) const {
      assert(curr->Next_ == nullptr && this->MatchKey_ < curr->MatchKey_);
      assert(this->Next_ == nullptr || curr->MatchKey_ < this->Next_->MatchKey_);
      curr->Next_ = this->Next_;
      this->Next_ = curr;
   }

   static void MakeFieldsImpl(fon9::seed::Fields& flds);

protected:
   template <class Derived>
   static void MakeFields(fon9::seed::Fields& flds) {
      static_assert(fon9_OffsetOf(Derived, IniSNO_) == fon9_OffsetOf(OmsReportFilled, IniSNO_),
                    "'OmsReportFilled' must be the first base class in derived.");
      MakeFieldsImpl(flds);
   }

public:
   using MatchKey = uint64_t;
   OmsRxSNO IniSNO_{};
   MatchKey MatchKey_{};

   OmsReportFilled(OmsRequestFactory& creator)
      : base{creator, f9fmkt_RxKind_Filled} {
   }
   OmsReportFilled()
      : base{f9fmkt_RxKind_Filled} {
   }

   /// 將 curr 依照 MatchKey_ 的順序(小到大), 加入到「成交串列」.
   /// \retval nullptr  成功將 curr 加入成交串列.
   /// \retval !nullptr 與 curr->MatchKey_ 相同的那個 request.
   static const OmsReportFilled* Insert(const OmsReportFilled** ppHead,
                                        const OmsReportFilled** ppLast,
                                        const OmsReportFilled* curr);

   const OmsReportFilled* Next() {
      return this->Next_;
   }
};

} // namespaces
#endif//__f9omstw_OmsReportFilled_hpp__
