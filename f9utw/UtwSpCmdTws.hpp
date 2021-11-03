// \file f9utw/UtwSpCmdTws.hpp
// \author fonwinz@gmail.com
#ifndef __f9utw_UtwSpCmdTws_hpp__
#define __f9utw_UtwSpCmdTws_hpp__
#include "f9utw/UtwSpCmd.hpp"

namespace f9omstw {

class SpCmdMgrTws : public SpCmdMgr {
   fon9_NON_COPY_NON_MOVE(SpCmdMgrTws);
   using base = SpCmdMgr;
protected:
   bool ParseSpCmdArg(SpCmdArgs& args, fon9::StrView tag, fon9::StrView value) override;
   SpCmdItemSP MakeSpCmdDord_Impl(fon9::StrView& strResult, SpCmdArgs&& args, fon9::Named&& itemName) override;
public:
   using base::base;
};

} // namespaces
#endif//__f9utw_UtwSpCmdTws_hpp__
