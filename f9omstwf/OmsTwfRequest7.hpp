// \file f9omstwf/OmsTwfRequest7.hpp
//
// TaiFex 詢價要求(R07/R37).
//
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfRequest7_hpp__
#define __f9omstwf_OmsTwfRequest7_hpp__
#include "f9omstwf/OmsTwfRequest0.hpp"

namespace f9omstw {

class OmsTwfRequestIni7 : public OmsTwfRequestIni0 {
   fon9_NON_COPY_NON_MOVE(OmsTwfRequestIni7);
   using base = OmsTwfRequestIni0;
public:
   template <class... ArgsT>
   OmsTwfRequestIni7(ArgsT&&... args)
      : base(RequestType::QuoteR, std::forward<ArgsT>(args)...) {
   }

   ~OmsTwfRequestIni7();
};

} // namespaces
#endif//__f9omstwf_OmsTwfRequest7_hpp__
