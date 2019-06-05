// \file f9omstw/OmsRequestMatch.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestMatch_hpp__
#define __f9omstw_OmsRequestMatch_hpp__
#include "f9omstw/OmsRequestBase.hpp"

namespace f9omstw {

/// 成交回報.
/// 在 OmsOrder 提供:
/// \code
///    OmsRequestMatch* OmsOrder::MatchHead_;
///    OmsRequestMatch* OmsOrder::MatchLast_;
/// \endcode
/// 新的成交, 如果是在 MatchLast_->MatchKey_ 之後, 就直接 append; 否則就從頭搜尋.
/// 由於成交回報「大部分」是附加到尾端, 所以這樣的處理負擔應是最小.
class OmsRequestMatch : public OmsRequestBase {
   fon9_NON_COPY_NON_MOVE(OmsRequestMatch);
   using base = OmsRequestBase;

   const OmsRequestMatch mutable* Next_{nullptr};

   /// 成交回報要求與下單線路無關, 所以這裡 do nothing.
   void NoReadyLineReject(fon9::StrView) override;

   /// 將 curr 插入 this 與 this->Next_ 之間;
   void InsertAfter(const OmsRequestMatch* curr) const {
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
      static_assert(fon9_OffsetOf(Derived, IniSNO_) == fon9_OffsetOf(OmsRequestMatch, IniSNO_),
                    "'OmsRequestMatch' must be the first base class in derived.");
      MakeFieldsImpl(flds);
   }

public:
   using MatchKey = uint64_t;

   OmsRequestMatch(OmsRequestFactory& creator)
      : base{creator, f9fmkt_RxKind_RequestMatch} {
   }
   OmsRequestMatch()
      : base{f9fmkt_RxKind_RequestMatch} {
   }

   /// 將 curr 依照 MatchKey_ 的順序(小到大), 加入到「成交串列」.
   /// \retval nullptr  成功將 curr 加入成交串列.
   /// \retval !nullptr 與 curr->MatchKey_ 相同的那個 request.
   static const OmsRequestMatch* Insert(const OmsRequestMatch** ppHead, const OmsRequestMatch** ppLast, const OmsRequestMatch* curr);

   const OmsRequestMatch* Next() {
      return this->Next_;
   }

   const OmsRequestBase* PreCheck_GetRequestInitiator(OmsRequestRunner& runner, OmsResource& res) {
      return base::PreCheck_GetRequestInitiator(runner, &this->IniSNO_, res);
   }
};

} // namespaces
#endif//__f9omstw_OmsRequestMatch_hpp__
