// \file f9utw/UtwOmsCoreUsrDef.hpp
// \author fonwinz@gmail.com
#ifndef __f9utw_UtwOmsCoreUsrDef_hpp__
#define __f9utw_UtwOmsCoreUsrDef_hpp__
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsCxRequest.hpp"

namespace f9omstw {

class UtwOmsCoreUsrDef : public OmsResource::UsrDefObj {
   fon9_NON_COPY_NON_MOVE(UtwOmsCoreUsrDef);
   struct PendingReq {
      const OmsCxBaseIniFn*   CxReq_;
      OmsRequestRunStep*      NextStep_;
      fon9::CharVector        Log_;
      PendingReq(const OmsCxBaseIniFn& cxReq, OmsRequestRunStep& nextStep, fon9::StrView log)
         : CxReq_{&cxReq}, NextStep_{&nextStep}, Log_{log} {
      }
   };
   using PendingReqListImpl = std::vector<PendingReq>;
   using PendingReqList = fon9::MustLock<PendingReqListImpl>;
   PendingReqList PendingReqList_;
public:
   UtwOmsCoreUsrDef();
   ~UtwOmsCoreUsrDef();

   void AddCondFiredReq(OmsCore& omsCore, const OmsCxBaseIniFn& cxReq, OmsRequestRunStep& runStep, fon9::StrView logstr);
};

} // namespaces
#endif//__f9utw_UtwOmsCoreUsrDef_hpp__
