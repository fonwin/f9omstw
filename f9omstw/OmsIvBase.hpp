// \file f9omstw/OmsIvBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsIvBase_hpp__
#define __f9omstw_OmsIvBase_hpp__
#include "f9omstw/OmsBase.hpp"

namespace f9omstw {

enum class OmsIvKind : uint8_t {
   Unknown = 0,
   Subac = 1,
   Ivac = 2,
   Brk = 3,
};

/// OmsIvBase 衍生出:「子帳」、「投資人帳號」、「券商」.
/// \code
///    +-----------------+
///    |    OmsIvBase    |
///    +-----------------+
///      ↑      ↑      ↑
/// OmsBrkc  OmsIvac  OmsSubac
/// \endcode
class OmsIvBase : public fon9::intrusive_ref_counter<OmsIvBase> {
   fon9_NON_COPY_NON_MOVE(OmsIvBase);
public:
   const OmsIvKind   IvKind_;
   char              Padding____[3];

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
