﻿// \file f9omstw/OmsRequestFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestFactory_hpp__
#define __f9omstw_OmsRequestFactory_hpp__
#include "f9omstw/OmsRequestRunner.hpp"
#include "fon9/seed/Tab.hpp"
#include "fon9/TimeStamp.hpp"
#include "fon9/ObjSupplier.hpp"

namespace f9omstw {

/// Name_   = 下單要求名稱, 例如: TwsNew;
/// Fields_ = 下單要求欄位;
class OmsRequestFactory : public fon9::seed::Tab {
   fon9_NON_COPY_NON_MOVE(OmsRequestFactory);
   using base = fon9::seed::Tab;

   virtual OmsRequestSP MakeRequestImpl();
   virtual OmsRequestSP MakeReportInImpl(f9fmkt_RxKind reqKind);

   /// 備援主機透過 Rpt(例:BergRpt...) 主動重新執行母單.
   OmsRequestRunStepSP  RptRerunStep_;

protected:
   /// 在 OmsReportFactoryT<> 建構時可以指定 RptRerunStep;
   void SetRptRerunStep(OmsRequestRunStepSP&& rptRerunStep) {
      assert(this->RptRerunStep_.get() == nullptr);
      this->RptRerunStep_ = std::move(rptRerunStep);
   }

public:
   /// 如果 this 建立的 request 屬於 OmsRequestIni 或 回報(包含成交回報),
   /// 則必須提供此類 request 對應的 OrderFactory;
   /// - (this->OrderFactory_ && this->RunStep_):  this = 可建立 Initiator 的 factory;
   /// - (this->OrderFactory_ && !this->RunStep_): this = Report 用的 factory;
   const OmsOrderFactorySP    OrderFactory_;
   const OmsRequestRunStepSP  RunStep_;

   using base::base;

   template <class... TabArgsT>
   OmsRequestFactory(OmsRequestRunStepSP runStepList, TabArgsT&&... tabArgs)
      : base{std::forward<TabArgsT>(tabArgs)...}
      , RunStep_{std::move(runStepList)} {
   }
   template <class... TabArgsT>
   OmsRequestFactory(OmsOrderFactorySP ordFactory, OmsRequestRunStepSP runStepList, TabArgsT&&... tabArgs)
      : base{std::forward<TabArgsT>(tabArgs)...}
      , OrderFactory_{std::move(ordFactory)}
      , RunStep_{std::move(runStepList)} {
   }

   virtual ~OmsRequestFactory();

   /// 在收到回報後, 可能需要的後續處理, 由回報處理者 [主動呼叫執行].
   /// 例: BergOrder 在收到子單異動回報時: OnAfterChildOrderUpdated, 使用此處提供的 RunStep 執行後續的步驟.
   OmsRequestRunStep* RptRerunStep() const {
      return this->RptRerunStep_.get();
   }
   OmsRequestRunStep* GetRunOrRptRerunStep() const {
      return this->RunStep_ ? this->RunStep_.get() : this->RptRerunStep_.get();
   }

   /// 預設傳回 nullptr, 表示此 factory 不支援下單.
   OmsRequestSP MakeRequest(fon9::TimeStamp now = fon9::UtcNow()) {
      OmsRequestSP retval = this->MakeRequestImpl();
      if (retval)
         retval->Initialize(*this, now);
      return retval;
   }

   /// 預設傳回 nullptr, 表示此 factory 不支援回報.
   OmsRequestSP MakeReportIn(f9fmkt_RxKind reqKind, fon9::TimeStamp now) {
      OmsRequestSP retval = this->MakeReportInImpl(reqKind);
      if (retval)
         retval->Initialize(*this, now);
      return retval;
   }
};
static inline OmsRequestSP OmsMakeRequest(OmsRequestFactory* fac) {
   if (fon9_LIKELY(fac))
      return fac->MakeRequest();
   return nullptr;
}

template <class RequestBaseT>
class OmsRequestFactoryBaseT : public OmsRequestFactory {
   fon9_NON_COPY_NON_MOVE(OmsRequestFactoryBaseT);
   using base = OmsRequestFactory;

protected:
   OmsRequestSP MakeRequestImpl() override {
      return new RequestBaseT;
   }

public:
   OmsRequestFactoryBaseT(std::string name, OmsRequestRunStepSP runStepList)
      : base(std::move(runStepList), fon9::Named(std::move(name)), MakeFieldsT<RequestBaseT>()) {
   }
   OmsRequestFactoryBaseT(std::string name, OmsOrderFactorySP ordFactory, OmsRequestRunStepSP runStepList)
      : base(std::move(ordFactory), std::move(runStepList), fon9::Named(std::move(name)), MakeFieldsT<RequestBaseT>()) {
   }
};

template <class RequestBaseT, unsigned kPoolObjCount>
class OmsRequestFactoryT : public OmsRequestFactoryBaseT<RequestBaseT> {
   fon9_NON_COPY_NON_MOVE(OmsRequestFactoryT);
   using base = OmsRequestFactoryBaseT<RequestBaseT>;

protected:
   using RequestSupplier = fon9::ObjSupplier<RequestBaseT, kPoolObjCount>;
   typename RequestSupplier::ThisSP RequestTape_{RequestSupplier::Make()};
   OmsRequestSP MakeRequestImpl() override {
      return this->RequestTape_->Alloc();
   }

public:
   using base::base;
};

template <class RequestBaseT>
class OmsRequestFactoryT<RequestBaseT, 0> : public OmsRequestFactoryBaseT<RequestBaseT> {
   fon9_NON_COPY_NON_MOVE(OmsRequestFactoryT);
   using base = OmsRequestFactoryBaseT<RequestBaseT>;

public:
   using base::base;
};

} // namespaces
#endif//__f9omstw_OmsRequestFactory_hpp__
