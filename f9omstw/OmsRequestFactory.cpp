// \file f9omstw/OmsRequestFactory.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsRequestRunner.hpp"
#include "f9omstw/OmsOrderFactory.hpp"

namespace f9omstw {

OmsRequestFactory::~OmsRequestFactory() {
}
OmsRequestSP OmsRequestFactory::MakeRequestImpl() {
   return nullptr;
}
OmsRequestSP OmsRequestFactory::MakeReportInImpl(f9fmkt_RxKind reqKind) {
   (void)reqKind;
   return nullptr;
}

} // namespaces
