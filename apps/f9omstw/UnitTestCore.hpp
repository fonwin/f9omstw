// \file f9omstw/apps/f9omstw/UnitTestCore.cpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_apps_UnitTestCore_hpp__
#define __f9omstw_apps_UnitTestCore_hpp__
#define _CRT_SECURE_NO_WARNINGS
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsRequestPolicy.hpp"
#include "f9omstw/OmsOrderTws.hpp"
#include "utws/UtwsBrk.hpp"
#include "utws/UtwsSymb.hpp"

#include "fon9/CmdArgs.hpp"
#include "fon9/Log.hpp"
#include "fon9/TestTools.hpp"
//--------------------------------------------------------------------------//
#include "fon9/ObjSupplier.hpp"
#ifdef _DEBUG
const unsigned    kPoolObjCount = 1000;
#else
const unsigned    kPoolObjCount = 1000 * 100;
#endif

enum class AllocFrom {
   Supplier,
   Memory,
};
static AllocFrom  gAllocFrom{AllocFrom::Supplier};
//--------------------------------------------------------------------------//
class OmsOrderTwsFactory : public f9omstw::OmsOrderFactory {
   fon9_NON_COPY_NON_MOVE(OmsOrderTwsFactory);
   using base = f9omstw::OmsOrderFactory;

   using OmsOrderTwsRaw = f9omstw::OmsOrderTwsRaw;
   using RawSupplier = fon9::ObjSupplier<OmsOrderTwsRaw, kPoolObjCount>;
   RawSupplier::ThisSP RawSupplier_{RawSupplier::Make()};
   OmsOrderTwsRaw* MakeOrderRawImpl() override {
      if (gAllocFrom == AllocFrom::Supplier)
         return this->RawSupplier_->Alloc();
      return new OmsOrderTwsRaw{};
   }

   using OmsOrderTws = f9omstw::OmsOrder;
   using OrderSupplier = fon9::ObjSupplier<OmsOrderTws, kPoolObjCount>;
   OrderSupplier::ThisSP OrderSupplier_{OrderSupplier::Make()};
   OmsOrderTws* MakeOrderImpl() override {
      if (gAllocFrom == AllocFrom::Supplier)
         return this->OrderSupplier_->Alloc();
      return new OmsOrderTws{};
   }
public:
   fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list
   OmsOrderTwsFactory()
      : base(fon9::Named{"TwsOrd"}, f9omstw::MakeFieldsT<OmsOrderTwsRaw>()) {
   }
   fon9_MSC_WARN_POP;

   ~OmsOrderTwsFactory() {
   }
};
//--------------------------------------------------------------------------//
template <class OmsRequestBaseT>
class OmsRequestFactoryT : public f9omstw::OmsRequestFactory {
   fon9_NON_COPY_NON_MOVE(OmsRequestFactoryT);
   using base = f9omstw::OmsRequestFactory;

   struct OmsRequestT : public OmsRequestBaseT {
      fon9_NON_COPY_NON_MOVE(OmsRequestT);
      using base = OmsRequestBaseT;
      using base::base;
      OmsRequestT() = default;
      void NoReadyLineReject(fon9::StrView) override {
      }
   };
   using RequestSupplier = fon9::ObjSupplier<OmsRequestT, kPoolObjCount>;
   typename RequestSupplier::ThisSP RequestTape_{RequestSupplier::Make()};
   f9omstw::OmsRequestSP MakeRequestImpl() override {
      if (gAllocFrom == AllocFrom::Supplier)
         return this->RequestTape_->Alloc();
      return new OmsRequestT{};
   }
public:
   OmsRequestFactoryT(std::string name, f9omstw::OmsRequestRunStepSP runStepList)
      : base(std::move(runStepList),
             fon9::Named(std::move(name)),
             f9omstw::MakeFieldsT<OmsRequestT>()) {
   }

   ~OmsRequestFactoryT() {
   }
};
using OmsRequestTwsIniFactory = OmsRequestFactoryT<f9omstw::OmsRequestTwsIni>;
using OmsRequestTwsChgFactory = OmsRequestFactoryT<f9omstw::OmsRequestTwsChg>;
using OmsRequestTwsMatchFactory = OmsRequestFactoryT<f9omstw::OmsRequestTwsMatch>;
//--------------------------------------------------------------------------//
f9omstw::OmsRequestRunner MakeOmsRequestRunner(f9omstw::OmsRequestFactoryPark& facPark, fon9::StrView reqstr) {
   f9omstw::OmsRequestRunner retval{reqstr};
   fon9::StrView tag = fon9::StrFetchNoTrim(reqstr, '|');
   if (auto* fac = facPark.GetFactory(tag)) {
      auto req = fac->MakeRequest();
      if (dynamic_cast<f9omstw::OmsRequestTrade*>(req.get()) == nullptr) {
         fon9_LOG_ERROR("MakeOmsRequestRunner|err=It's not a trading request factory|reqfac=", tag);
         return retval;
      }
      fon9::seed::SimpleRawWr wr{*req};
      fon9::StrView value;
      while(fon9::StrFetchTagValue(reqstr, tag, value)) {
         auto* fld = fac->Fields_.Get(tag);
         if (fld == nullptr) {
            fon9_LOG_ERROR("MakeOmsRequestRunner|err=Unknown field|reqfac=", fac->Name_, "|fld=", tag);
            return retval;
         }
         fld->StrToCell(wr, value);
      }
      retval.Request_ = std::move(fon9::static_pointer_cast<f9omstw::OmsRequestTrade>(req));
      return retval;
   }
   fon9_LOG_ERROR("MakeOmsRequestRunner|err=Unknown request factory|reqfac=", tag);
   return retval;
}
void PrintOmsRequest(f9omstw::OmsRequestBase& req) {
   fon9::RevBufferList     rbuf{128};
   fon9::seed::SimpleRawRd rd{req};
   for (size_t fidx = req.Creator_->Fields_.size(); fidx > 0;) {
      auto fld = req.Creator_->Fields_.Get(--fidx);
      fld->CellRevPrint(rd, nullptr, rbuf);
      fon9::RevPrint(rbuf, *fon9_kCSTR_CELLSPL, fld->Name_, '=');
   }
   fon9::RevPrint(rbuf, req.Creator_->Name_);
   puts(fon9::BufferTo<std::string>(rbuf.MoveOut()).c_str());
}
//--------------------------------------------------------------------------//
// 測試用, 不啟動 OmsThread: 把 main thread 當成 OmsThread.
struct TestCore : public f9omstw::OmsCore {
   fon9_NON_COPY_NON_MOVE(TestCore);
   using base = f9omstw::OmsCore;
   f9omstw::OmsThreadTaskHandler CoreHandler_;

   fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list
   TestCore(int argc, char* argv[]) : base{"ut"}, CoreHandler_(*this) {
      gAllocFrom = static_cast<AllocFrom>(fon9::StrTo(fon9::GetCmdArg(argc, argv, "f", "allocfrom"), 0u));
      std::cout << "AllocFrom = " << (gAllocFrom == AllocFrom::Supplier ? "Supplier" : "Memory") << std::endl;

      this->OrderFacPark_.reset(new f9omstw::OmsOrderFactoryPark(
         new OmsOrderTwsFactory
      ));

      using namespace f9omstw;
      this->Symbs_.reset(new OmsSymbTree(*this, UtwsSymb::MakeLayout(OmsSymbTree::DefaultTreeFlag()), &UtwsSymb::SymbMaker));
      this->Brks_.reset(new OmsBrkTree(*this, UtwsBrk::MakeLayout(OmsBrkTree::DefaultTreeFlag()), &OmsBrkTree::TwsBrkIndex1));
      this->Brks_->Initialize(&UtwsBrk::BrkMaker, "8610", 5u, &IncStrAlpha);
      // 建立委託書號表的關聯.
      this->Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwSEC);
      this->Brks_->InitializeTwsOrdNoMap(f9fmkt_TradingMarket_TwOTC);
   }
   fon9_MSC_WARN_POP;

   ~TestCore() {
      this->Backend_.WaitForEndNow();
      this->Brks_->InThr_OnParentSeedClear();
   }

   void OpenReload(int argc, char* argv[], std::string fnDefault) {
      const auto  outfn = fon9::GetCmdArg(argc, argv, "o", "out");
      this->TDay_ = fon9::UtcNow();
      this->Backend_.OpenReload(outfn.empty() ? fnDefault : outfn.ToString(), this->GetResource());
      this->Backend_.StartThread(this->Name_ + "_Backend");
   }

   f9omstw::OmsResource& GetResource() {
      return *static_cast<f9omstw::OmsResource*>(this);
   }
};
#endif//__f9omstw_apps_UnitTestCore_hpp__
