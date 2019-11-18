// \file f9omstwf/OmsTwfExgMapMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfExgMapMgr_hpp__
#define __f9omstwf_OmsTwfExgMapMgr_hpp__
#include "f9twf/ExgMapMgr.hpp"
#include "f9omstw/OmsBase.hpp"

namespace f9omstw {

class TwfExgMapMgr : public f9twf::ExgMapMgr {
   fon9_NON_COPY_NON_MOVE(TwfExgMapMgr);
   using base = f9twf::ExgMapMgr;
protected:
   void OnP08Updated(const f9twf::P08Recs& p08recs, f9twf::ExgSystemType sysType, MapsConstLocker&& lk) override;

public:
   OmsCoreMgr& CoreMgr_;

   template <class... ArgsT>
   TwfExgMapMgr(OmsCoreMgr& coreMgr, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , CoreMgr_(coreMgr) {
   }
};

} // namespaces
#endif//__f9omstwf_OmsTwfExgMapMgr_hpp__
