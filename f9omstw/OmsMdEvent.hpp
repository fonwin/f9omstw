// \file f9omstw/OmsMdEvent.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsMdEvent_hpp__
#define __f9omstw_OmsMdEvent_hpp__
#include "f9omstw/OmsBase.hpp"
#include "fon9/Subr.hpp"

namespace f9omstw {

struct OmsMdLastPrice {
   fon9::fmkt::Pri   LastPrice_;
   bool operator==(const OmsMdLastPrice& rhs) const {
      return this->LastPrice_ == rhs.LastPrice_;
   }
   bool operator!=(const OmsMdLastPrice& rhs) const {
      return !this->operator==(rhs);
   }
};
class OmsMdLastPriceEv : public OmsMdLastPrice {
   fon9_NON_COPY_NON_MOVE(OmsMdLastPriceEv);
protected:
   bool IsNeedsOnMdLastPriceEv_{false};
   char Padding_____[7];
   void SetIsNeedsOnMdLastPriceEv(bool value) {
      this->IsNeedsOnMdLastPriceEv_ = value;
   }
public:
   fon9::fmkt::Qty   LastQty_{};
   fon9::fmkt::Qty   TotalQty_{};

   OmsMdLastPriceEv() = default;
   virtual ~OmsMdLastPriceEv();
   virtual void OnMdLastPriceEv(const OmsMdLastPrice& bf, OmsCoreMgr&) = 0;
   /// 是否需要觸發 this->OnMdLastPriceEv()?
   bool IsNeedsOnMdLastPriceEv() const {
      return this->IsNeedsOnMdLastPriceEv_;
   }
};

class OmsMdLastPriceSubject : public OmsMdLastPriceEv {
   fon9_NON_COPY_NON_MOVE(OmsMdLastPriceSubject);
public:
   using Handler = std::function<void(const OmsMdLastPriceSubject& sender, const OmsMdLastPrice& bf, OmsCoreMgr&)>;
   using Subject = fon9::Subject<Handler>;

   OmsMdLastPriceSubject() = default;
   ~OmsMdLastPriceSubject();
   void OnMdLastPriceEv(const OmsMdLastPrice& bf, OmsCoreMgr&) override;

   void MdLastPrice_Subscribe(fon9::SubConn* subConn, Handler&& handler) {
      this->Subject_.Subscribe(subConn, std::move(handler));
      this->IsNeedsOnMdLastPriceEv_ = true;
   }
   void MdLastPrice_Unsubscribe(fon9::SubConn* subConn) {
      if (this->Subject_.Unsubscribe(subConn)) {
         auto subj = this->Subject_.Lock();
         this->IsNeedsOnMdLastPriceEv_ = !subj->empty();
      }
   }

protected:
   Subject  Subject_;
};
//--------------------------------------------------------------------------//
#define f9omstw_kMdBSCount    5
fon9_MSC_WARN_DISABLE(4582); // constructor is not implicitly called
struct OmsMdBS {
   union {
      fon9::fmkt::PriQty   Buy_;
      fon9::fmkt::PriQty   Buys_[f9omstw_kMdBSCount];
   };
   union {
      fon9::fmkt::PriQty   Sell_;
      fon9::fmkt::PriQty   Sells_[f9omstw_kMdBSCount];
   };
   OmsMdBS() {
      fon9::ForceZeroNonTrivial(this);
   }
};
fon9_MSC_WARN_POP;

class OmsMdBSEv : public OmsMdBS {
   fon9_NON_COPY_NON_MOVE(OmsMdBSEv);
protected:
   bool IsNeedsOnMdBSEv_{false};
   char Padding_____[7];
   void SetIsNeedsOnMdBSEv(bool value) {
      this->IsNeedsOnMdBSEv_ = value;
   }
public:
   OmsMdBSEv() = default;
   virtual ~OmsMdBSEv();
   virtual void OnMdBSEv(const OmsMdBS& bf, OmsCoreMgr&) = 0;
   /// 是否需要觸發 this->OnMdBSEv()?
   bool IsNeedsOnMdBSEv() const {
      return this->IsNeedsOnMdBSEv_;
   }
};
class OmsMdBSSubject : public OmsMdBSEv {
   fon9_NON_COPY_NON_MOVE(OmsMdBSSubject);
public:
   using Handler = std::function<void(const OmsMdBSSubject& sender, const OmsMdBS& bf, OmsCoreMgr&)>;
   using Subject = fon9::Subject<Handler>;

   OmsMdBSSubject() = default;
   ~OmsMdBSSubject();
   void OnMdBSEv(const OmsMdBS& bf, OmsCoreMgr&) override;

   void MdBS_Subscribe(fon9::SubConn* subConn, Handler&& handler) {
      this->Subject_.Subscribe(subConn, std::move(handler));
      this->IsNeedsOnMdBSEv_ = true;
   }
   void MdBS_Unsubscribe(fon9::SubConn* subConn) {
      if (this->Subject_.Unsubscribe(subConn)) {
         auto subj = this->Subject_.Lock();
         this->IsNeedsOnMdBSEv_ = !subj->empty();
      }
   }

protected:
   Subject  Subject_;
};

} // namespaces
#endif//__f9omstw_OmsMdEvent_hpp__
