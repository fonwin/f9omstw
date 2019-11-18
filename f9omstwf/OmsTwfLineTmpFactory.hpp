// \file f9omstwf/OmsTwfLineTmpFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfLineTmpFactory_hpp__
#define __f9omstwf_OmsTwfLineTmpFactory_hpp__
#include "f9omstwf/OmsTwfRptLineTmp.hpp"
#include "f9twf/ExgLineTmpFactory.hpp"

namespace f9omstw {

class TwfLineTmpFactory : public f9twf::ExgLineTmpFactory
                        , public TwfLineTmpWorker {
   fon9_NON_COPY_NON_MOVE(TwfLineTmpFactory);
   using base = f9twf::ExgLineTmpFactory;

protected:
   fon9::TimeStamp GetTDay() override;
   fon9::io::SessionSP CreateLineTmp(f9twf::ExgTradingLineMgr&    lineMgr,
                                     const f9twf::ExgLineTmpArgs& lineArgs,
                                     f9twf::ExgLineTmpLog&&       log,
                                     std::string&                 errReason) override;
public:
   TwfLineTmpFactory(std::string           logPathFmt,
                     Named&&               name,
                     OmsCoreMgr&           coreMgr,
                     OmsTwfReport2Factory& rpt1Factory,
                     OmsTwfReport8Factory& rpt8Factory,
                     OmsTwfReport9Factory& rpt9Factory,
                     OmsTwfFilled1Factory& fil1Factory,
                     OmsTwfFilled2Factory& fil2Factory)
      : base(std::move(logPathFmt), std::move(name))
      , TwfLineTmpWorker{coreMgr, rpt1Factory, rpt8Factory, rpt9Factory, fil1Factory, fil2Factory} {
   }
};

} // namespaces
#endif//__f9omstwf_OmsTwfLineTmpFactory_hpp__
