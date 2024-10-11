// \file f9omstw/OmsSymb.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsSymb.hpp"

namespace f9omstw {

OmsSymb::~OmsSymb() {
}
void OmsSymb::LockMd() {
}
void OmsSymb::UnlockMd() {
}
void OmsSymb::OnTwfSessionChanged(OmsResource&) {
}
void OmsSymb::ResetTwfCombSessionId(OmsResource&) {
}
void OmsSymb::OnTradingSessionClosed(OmsResource&) {
}
void OmsSymb::OnMdNoData(OmsResource&) {
}
TwfExgSymbBasic* OmsSymb::GetTwfExgSymbBasic() {
   return nullptr;
}
bool OmsSymb::GetPriLmt(fon9::fmkt::Pri* upLmt, fon9::fmkt::Pri* dnLmt) const {
   (void)upLmt; (void)dnLmt;
   return false;
}
OmsMdLastPriceEv* OmsSymb::GetMdLastPriceEv() {
   return nullptr;
}
OmsMdLastPriceEv* OmsSymb::GetMdLastPriceEv_OddLot() {
   return nullptr;
}
OmsMdBSEv* OmsSymb::GetMdBSEv() {
   return nullptr;
}
OmsMdBSEv* OmsSymb::GetMdBSEv_OddLot() {
   return nullptr;
}

} // namespaces
