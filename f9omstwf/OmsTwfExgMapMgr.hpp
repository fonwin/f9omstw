// \file f9omstwf/OmsTwfExgMapMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfExgMapMgr_hpp__
#define __f9omstwf_OmsTwfExgMapMgr_hpp__
#include "f9twf/ExgMapMgr.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstwf/OmsTwfExgSymbBasic.hpp"

namespace f9omstw {

class TwfExgMapMgr : public f9twf::ExgMapMgr {
   fon9_NON_COPY_NON_MOVE(TwfExgMapMgr);
   using base = f9twf::ExgMapMgr;
   fon9::intrusive_ptr<ContractTree>   ContractTree_;
   OmsCore*                            CurrentCore_;

protected:
   void OnP08Updated(const f9twf::P08Recs& p08recs, f9twf::ExgSystemType sysType, MapsConstLocker&& lk) override;
   void OnP09Updated(const f9twf::P09Recs& p09recs, f9twf::ExgSystemType sysType, MapsConstLocker&& lk) override;
   void OnP06Updated(const f9twf::ExgMapBrkFcmId& mapBrkFcmId, MapsLocker&& lk) override;

public:
   OmsCoreMgr& CoreMgr_;

   template <class... ArgsT>
   TwfExgMapMgr(OmsCoreMgr& coreMgr, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , CoreMgr_(coreMgr) {
   }

   void OnAfter_InitCoreTables(OmsResource& res);

   /// 在 SetTDay() 之後, 才會有效.
   ContractTree* GetContractTree() const {
      return this->ContractTree_.get();
   }
   OmsCore* GetCurrentCore() const {
      return this->CurrentCore_ ? this->CurrentCore_ : this->CoreMgr_.CurrentCore().get();
   }
};
using TwfExgMapMgrSP = fon9::intrusive_ptr<TwfExgMapMgr>;

} // namespaces
#endif//__f9omstwf_OmsTwfExgMapMgr_hpp__
