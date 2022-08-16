// \file f9omstw/OmsParentBase.hpp
//
// OMS 子母單機制.
// https://docs.google.com/document/d/1mVKBRPoR98nFoOqRpgCWCzYp-JSLLmyyl9YwQmVZZ1g/edit?usp=sharing
//
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsParentBase_hpp__
#define __f9omstw_OmsParentBase_hpp__
#include "fon9/fmkt/FmktTypes.hpp"

namespace f9omstw {

using OmsParentQty = uint32_t;
using OmsParentQtyS = fon9::make_signed_t<OmsParentQty>;
using OmsParentPri = fon9::fmkt::Pri;
using OmsParentAmt = fon9::fmkt::Amt;

class OmsParentOrderRaw;
class OmsParentOrder;
class OmsParentRequestIni;
class OmsParentRequestChg;
//--------------------------------------------------------------------------//

#define OmsErrCode_Parent_NoLeaves  static_cast<OmsErrCode>(OmsErrCode_FromRisk + 2000)
#define OmsErrCode_Parent_OnlyDel   static_cast<OmsErrCode>(OmsErrCode_FromRisk + 2001)

} // namespaces
#endif//__f9omstw_OmsParentBase_hpp__
