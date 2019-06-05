// \file f9omstw/OmsRequestFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestFactory_hpp__
#define __f9omstw_OmsRequestFactory_hpp__
#include "f9omstw/OmsRequestRunner.hpp"
#include "fon9/seed/Tab.hpp"
#include "fon9/TimeStamp.hpp"

namespace f9omstw {

/// Name_   = 下單要求名稱, 例如: TwsNew;
/// Fields_ = 下單要求欄位;
class OmsRequestFactory : public fon9::seed::Tab {
   fon9_NON_COPY_NON_MOVE(OmsRequestFactory);
   using base = fon9::seed::Tab;

   virtual OmsRequestSP MakeRequestImpl() = 0;

public:
   const OmsRequestRunStepSP  RunStep_;

   using base::base;

   template <class... TabArgsT>
   OmsRequestFactory(OmsRequestRunStepSP runStepList, TabArgsT&&... tabArgs)
      : base{std::forward<TabArgsT>(tabArgs)...}
      , RunStep_{std::move(runStepList)} {
   }

   virtual ~OmsRequestFactory();

   OmsRequestSP MakeRequest(fon9::TimeStamp now = fon9::UtcNow()) {
      OmsRequestSP retval = this->MakeRequestImpl();
      retval->Initialize(*this, now);
      return retval;
   }
};

} // namespaces
#endif//__f9omstw_OmsRequestFactory_hpp__
