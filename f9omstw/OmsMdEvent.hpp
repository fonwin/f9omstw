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
using OmsMdLastPriceHandler = std::function<void(const OmsMdLastPrice& owner, const OmsMdLastPrice& bf, OmsCoreMgr&)>;
struct OmsMdLastPriceSubject : public fon9::Subject<OmsMdLastPriceHandler> {
   fon9_NON_COPY_NON_MOVE(OmsMdLastPriceSubject);
   OmsMdLastPriceSubject() = default;
   ~OmsMdLastPriceSubject();
};

} // namespaces
#endif//__f9omstw_OmsMdEvent_hpp__
