// \file f9omstw/OmsRequestTrade.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestTrade_hpp__
#define __f9omstw_OmsRequestTrade_hpp__
#include "f9omstw/OmsRequestBase.hpp"
#include "f9omstw/OmsRequestPolicy.hpp"
#include "f9omstw/IvacNo.hpp"

namespace f9omstw {

struct OmsRequestFrom {
   /// 收單處理程序名稱, 例如: Rc, FixIn...
   fon9::CharAry<8>  SesName_;
   fon9::CharAry<12> UserId_;
   fon9::CharAry<16> FromIp_;
   /// 來源別.
   fon9::CharAry<4>  Src_;

   OmsRequestFrom() {
      memset(this, 0, sizeof(*this));
   }
};

struct OmsRequestCliDef {
   fon9::CharAry<16> UsrDef_;
   fon9::CharAry<16> ClOrdId_;

   OmsRequestCliDef() {
      memset(this, 0, sizeof(*this));
   }
};

/// 下單要求(排除成交): 新、刪、改、查; 共同的基底 OmsRequestTrade;
class OmsRequestTrade : public OmsRequestBase,
                        public OmsRequestFrom,
                        public OmsRequestCliDef {
   fon9_NON_COPY_NON_MOVE(OmsRequestTrade);
   using base = OmsRequestBase;

   OmsRequestPolicySP   Policy_;

   static void MakeFieldsImpl(fon9::seed::Fields& flds);
protected:
   template <class Derived>
   static void MakeFields(fon9::seed::Fields& flds) {
      static_assert(fon9_OffsetOf(Derived, SesName_) == fon9_OffsetOf(OmsRequestTrade, SesName_),
                    "'OmsRequestTrade' must be the first base class in derived.");
      MakeFieldsImpl(flds);
   }

public:
   OmsRequestTrade(OmsRequestFactory& creator, f9fmkt_RequestKind reqKind = f9fmkt_RequestKind_Unknown)
      : base{creator, reqKind} {
   }
   OmsRequestTrade(f9fmkt_RequestKind reqKind = f9fmkt_RequestKind_Unknown)
      : base{reqKind} {
   }

   void SetPolicy(OmsRequestPolicySP policy) {
      assert(this->Policy_.get() == nullptr);
      this->Policy_ = std::move(policy);
   }
   const OmsRequestPolicy* Policy() const {
      return this->Policy_.get();
   }

   /// 請參閱「下單流程」文件.
   /// 在建立好 req 之後, 預先檢查程序, 此時還在 user thread, 尚未進入 OmsCore.
   /// return this->Policy_ ? this->Policy_->PreCheckInUser(reqRunner) : true;
   virtual bool PreCheckInUser(OmsRequestRunner& reqRunner);
};

//--------------------------------------------------------------------------//

struct OmsRequestIniDat {
   IvacNo            IvacNo_;
   fon9::CharAry<10> SubacNo_;
   fon9::CharAry<10> SalesNo_;

   OmsRequestIniDat() {
      memset(this, 0, sizeof(*this));
   }
};

/// OmsRequestIni: 一般而言是新單要求, 但也有可能是遺失單的補單「查詢、刪單」之類的操作。
class OmsRequestIni : public OmsRequestTrade, public OmsRequestIniDat {
   fon9_NON_COPY_NON_MOVE(OmsRequestIni);
   using base = OmsRequestTrade;

   friend class OmsOrder; // OmsOrder 建構時需要呼叫 SetInitiatorFlag() 的權限.
   void SetInitiatorFlag() {
      this->RequestFlags_ |= OmsRequestFlag_Initiator;
   }
   static void MakeFieldsImpl(fon9::seed::Fields& flds);
protected:
   template <class Derived>
   static void MakeFields(fon9::seed::Fields& flds) {
      static_assert(fon9_OffsetOf(Derived, IvacNo_) == fon9_OffsetOf(OmsRequestIni, IvacNo_),
                    "'OmsRequestIni' must be the first base class in derived.");
      MakeFieldsImpl(flds);
   }

public:
   OmsRequestIni(OmsRequestFactory& creator, f9fmkt_RequestKind reqKind = f9fmkt_RequestKind_New)
      : base{creator, reqKind} {
   }
   OmsRequestIni(f9fmkt_RequestKind reqKind = f9fmkt_RequestKind_New)
      : base{reqKind} {
   }

   /// 檢查: 若 OrdNo_.begin() != '\0'; 則必須有 IsAllowAnyOrdNo_ 權限.
   bool PreCheckInUser(OmsRequestRunner& reqRunner) override;
};

class OmsRequestUpd : public OmsRequestTrade {
   fon9_NON_COPY_NON_MOVE(OmsRequestUpd);
   using base = OmsRequestTrade;

   OmsRxSNO IniSNO_{0};

   static void MakeFieldsImpl(fon9::seed::Fields& flds);
protected:
   template <class Derived>
   static void MakeFields(fon9::seed::Fields& flds) {
      static_assert(fon9_OffsetOf(Derived, IniSNO_) == fon9_OffsetOf(OmsRequestUpd, IniSNO_),
                    "'OmsRequestUpd' must be the first base class in derived.");
      MakeFieldsImpl(flds);
   }

public:
   using base::base;
   OmsRequestUpd() = default;

   OmsRxSNO IniSNO() const {
      return this->IniSNO_;
   }

   const OmsRequestBase* PreCheckIniRequest(OmsResource& res) {
      return base::PreCheckIniRequest(&this->IniSNO_, res);
   }
};

} // namespaces
#endif//__f9omstw_OmsRequestTrade_hpp__
