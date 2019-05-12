// \file f9omstw/OmsIvBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsIvBase_hpp__
#define __f9omstw_OmsIvBase_hpp__
#include "f9omstw/OmsBase.hpp"

namespace f9omstw {

enum OmsIvKind {
   Subac,
   Ivac,
   Brk,
};

/// OmsIvBase 衍生出:「子帳」、「投資人帳號」、「券商」.
class OmsIvBase : public fon9::intrusive_ref_counter<OmsIvBase> {
   fon9_NON_COPY_NON_MOVE(OmsIvBase);
public:
   const OmsIvKind   IvKind_;

   /// - OmsSubac 的 Parent = OmsIvac;
   /// - OmsIvac 的 Parent = OmsBrk;
   /// - OmsBrk 的 Parent = nullptr;
   const OmsIvBaseSP Parent_;

   OmsIvBase(OmsIvKind kind, OmsIvBaseSP parent)
      : IvKind_{kind}
      , Parent_{std::move(parent)} {
   }
   virtual ~OmsIvBase();
};

} // namespaces
#endif//__f9omstw_OmsIvBase_hpp__
