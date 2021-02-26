// \file f9omstw/OmsImporter.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsImporter_hpp__
#define __f9omstw_OmsImporter_hpp__
#include "f9omstw/OmsCoreMgr.hpp"
#include "fon9/seed/FileImpTree.hpp"

namespace f9omstw {

fon9_WARN_DISABLE_PADDING;
struct OmsFileImpLoader : public fon9::seed::FileImpLoader {
   fon9_NON_COPY_NON_MOVE(OmsFileImpLoader);
   const OmsCoreSP Core_;
   const unsigned  CoreYYYYMMDD_;
   OmsFileImpLoader(OmsCoreSP core)
      : Core_{core}
      , CoreYYYYMMDD_{fon9::GetYYYYMMDD(core->TDay())} {
   }
};
fon9_WARN_POP;

using OmsFileImpMgr = fon9::seed::FileImpMgr;
using OmsFileImpSeed = fon9::seed::FileImpSeed;

class OmsFileImpTree : public fon9::seed::FileImpTree {
   fon9_NON_COPY_NON_MOVE(OmsFileImpTree);
   using base = fon9::seed::FileImpTree;

public:
   OmsCoreMgr& CoreMgr_;
   OmsFileImpTree(fon9::seed::FileImpMgr& cfgMgr, OmsCoreMgr& coreMgr)
      : base{cfgMgr}
      , CoreMgr_(coreMgr) {
   }
   ~OmsFileImpTree();
};

} // namespaces
#endif//__f9omstw_OmsImporter_hpp__
