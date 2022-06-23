// \file f9omstw/OmsSymb.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsSymb_hpp__
#define __f9omstw_OmsSymb_hpp__
#include "fon9/fmkt/SymbTwa.hpp"
#include "fon9/fmkt/SymbTws.hpp"
#include "fon9/fmkt/SymbTwf.hpp"

namespace f9omstw {

class OmsSymb : public fon9::fmkt::SymbTwa {
   fon9_NON_COPY_NON_MOVE(OmsSymb);
   using base = fon9::fmkt::SymbTwa;
public:
   using base::base;
   ~OmsSymb();
};
using OmsSymbSP = fon9::intrusive_ptr<OmsSymb>;

// TODO: 是否要針對不同市場, 使用不同的 Symb?
// using OmsTwsSymb = fon9::fmkt::SymbTws;
// using OmsTwsSymbSP = fon9::intrusive_ptr<OmsTwsSymb>;
// using OmsTwfSymb = fon9::fmkt::SymbTwf;
// using OmsTwfSymbSP = fon9::intrusive_ptr<OmsTwfSymb>;

} // namespaces
#endif//__f9omstw_OmsSymb_hpp__
