// \file f9omstw/OmsOrderFactory.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsOrderFactory.hpp"
#include "f9omstw/OmsOrder.hpp"

namespace f9omstw {

OmsOrderFactory::~OmsOrderFactory() {
}
OmsOrderRaw* OmsOrderFactory::MakeOrderRaw(OmsOrder& order, const OmsRequestBase& req) {
   OmsOrderRaw* raw = this->MakeOrderRawImpl();
   if (auto last = order.Last())
      raw->Initialize(last, req);
   else
      raw->Initialize(order);
   return raw;
}
} // namespaces
