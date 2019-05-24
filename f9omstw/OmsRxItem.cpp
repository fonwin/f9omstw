// \file f9omstw/OmsRxItem.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRxItem.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

OmsRxItem::~OmsRxItem() {
}
const OmsRequestBase* OmsRxItem::CastToRequest() const {
   return nullptr;
}
const OmsOrderRaw* OmsRxItem::CastToOrderRaw() const {
   return nullptr;
}

fon9::seed::FieldSP OmsRxItem::MakeField_RxSNO() {
   return fon9_MakeField_const(fon9::Named{"RxSNO"}, OmsRxItem, RxSNO_);
}

} // namespaces
