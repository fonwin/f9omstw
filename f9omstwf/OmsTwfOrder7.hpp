// \file f9omstwf/OmsTwfOrder7.hpp
//
// 詢價.
//
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfOrder7_hpp__
#define __f9omstwf_OmsTwfOrder7_hpp__
#include "f9omstwf/OmsTwfOrder0.hpp"
#include "f9omstwf/OmsTwfTypes.hpp"

namespace f9omstw {

using OmsTwfOrder7 = OmsTwfOrder0;
class OmsTwfOrderRaw7 : public OmsTwfOrderRaw0 {
   fon9_NON_COPY_NON_MOVE(OmsTwfOrderRaw7);
   using base = OmsTwfOrderRaw0;
public:
   using base::base;
   OmsTwfOrderRaw7() = default;
   ~OmsTwfOrderRaw7();
   bool IsWorking() const override;
};

} // namespaces
#endif//__f9omstwf_OmsTwfOrder7_hpp__
