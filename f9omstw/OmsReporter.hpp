// \file f9omstw/OmsReporter.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsReporter_hpp__
#define __f9omstw_OmsReporter_hpp__
#include "f9omstw/OmsBase.hpp"

namespace f9omstw {

class OmsReporter {
   fon9_NON_COPY_NON_MOVE(OmsReporter);
public:
   virtual ~OmsReporter();
   virtual void RunReport(OmsRequestRunnerInCore&&) = 0;
};

} // namespaces
#endif//__f9omstw_OmsReporter_hpp__
