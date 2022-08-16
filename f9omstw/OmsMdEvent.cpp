// \file f9omstw/OmsMdEvent.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsMdEvent.hpp"

namespace f9omstw {

OmsMdLastPriceEv::~OmsMdLastPriceEv() {
}
OmsMdLastPriceSubject::~OmsMdLastPriceSubject() {
}
void OmsMdLastPriceSubject::OnMdLastPriceEv(const OmsMdLastPrice& bf, OmsCoreMgr& omsCoreMgr) {
   this->Subject_.Publish(*this, bf, omsCoreMgr);
}
//--------------------------------------------------------------------------//
OmsMdBSEv::~OmsMdBSEv() {
}
OmsMdBSSubject::~OmsMdBSSubject() {
}
void OmsMdBSSubject::OnMdBSEv(const OmsMdBS& bf, OmsCoreMgr& omsCoreMgr) {
   this->Subject_.Publish(*this, bf, omsCoreMgr);
}

} // namespaces
