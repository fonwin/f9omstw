// \file f9utw/UtwOmsCoreUsrDef.hpp
// \author fonwinz@gmail.com
#ifndef __f9utw_UtwOmsCoreUsrDef_hpp__
#define __f9utw_UtwOmsCoreUsrDef_hpp__
#include "f9omstw/OmsCxToCore.hpp"
#include "f9omstw/OmsResource.hpp"

namespace f9omstw {

class UtwOmsCoreUsrDef : public OmsResource::UsrDefObj, public OmsCxToCore {
   fon9_NON_COPY_NON_MOVE(UtwOmsCoreUsrDef);
public:
   UtwOmsCoreUsrDef();
   ~UtwOmsCoreUsrDef();
};

} // namespaces
#endif//__f9utw_UtwOmsCoreUsrDef_hpp__
