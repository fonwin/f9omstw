// \file f9omstw/OmsRequestRunner.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestRunner.hpp"
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {

void OmsRequestRunner::RequestAbandon(OmsResource* res, std::string reason) {
   this->Request_->Abandon(std::move(reason));
   if (res)
      res->Backend_.Append(*this->Request_, std::move(this->ExLog_));
}
//--------------------------------------------------------------------------//
OmsRequestRunStep::~OmsRequestRunStep() {
}

} // namespaces
