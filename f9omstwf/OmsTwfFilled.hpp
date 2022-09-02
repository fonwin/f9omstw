// \file f9omstwf/OmsTwfFilled.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfFilled_hpp__
#define __f9omstwf_OmsTwfFilled_hpp__
#include "f9omstw/OmsReportFilled.hpp"
#include "f9omstw/OmsReportFactory.hpp"
#include "f9omstw/OmsReportTools.hpp"
#include "f9omstwf/OmsTwfTypes.hpp"

namespace f9omstw {

class OmsTwfFilled : public OmsReportFilled {
   fon9_NON_COPY_NON_MOVE(OmsTwfFilled);
   using base = OmsReportFilled;

protected:
   char* RevFilledReqUID(char* pout) override;
   bool RunReportInCore_FilledIsFieldsMatch(const OmsRequestIni& ini) const override;
   void RunReportInCore_FilledBeforeNewDone(OmsResource& resource, OmsOrder& order) override;
   bool RunReportInCore_FilledIsNeedsReportPending(const OmsOrderRaw& lastOrdUpd) const override;
   OmsOrderRaw* RunReportInCore_FilledMakeOrder(OmsReportChecker& checker) override;
   /// 檢查是否需要處理: 剩餘刪除(部分成交).
   void CheckPartFilledQtyCanceled(OmsResource& res, const OmsRequestRunnerInCore* prevRunner) const;
   void ProcessQtyCanceled(OmsReportRunnerInCore&& inCoreRunner) const;

   static void MakeFields(fon9::seed::Fields& flds);

public:
   fon9::TimeStamp   Time_;
   /// 單式成交價, 或複式單第1腳成交價.
   OmsTwfPri         Pri_;
   OmsTwfQty         Qty_;
   f9fmkt_Side       Side_{};
   OmsTwfSymbol      Symbol_;
   IvacNo            IvacNo_{0};
   OmsReportPriStyle PriStyle_;
   OmsTwfPosEff      PosEff_;
   /// 期交所刪除數量(最終未成交的數量).
   /// - IOC、FOK 未成交的期交所刪單.
   /// - 超過「動態價格穩定區間」的取消(status_code=47).
   OmsTwfQty         QtyCanceled_{0};
   /// 當委託不存在時, 是否放棄此次回報?
   /// false(預設): 當委託不存在時, 會將此筆回報使用 pending 機制: 等候收到新單時再處理.
   bool              IsAbandonOrderNotFound_{false};
   char              padding___[3];
   /// - 用於新單合併成交時,「範圍市價」的「實際價格」.
   ///   - 一般新單回報、改價回報, 會用 OmsTwfReport 處理.
   /// - 因 R22 沒有 OrdType(PriType), 無法判斷原始委託是否為 MWP,
   ///   所以只能單純提供「期交所接受的委託價」.
   /// - 可以在收到回報時單純地將「實際委託價」填入,
   /// - 只有在 RunReportInCore_FilledBeforeNewDone() 會使用到.
   OmsTwfPri         PriOrd_;

   using base::base;

   void RunReportInCore(OmsReportChecker&& checker) override;
   void ProcessPendingReport(const OmsRequestRunnerInCore& prevRunner) const override;
   void OnSynReport(const OmsRequestBase* ref, fon9::StrView message) override;
};

//--------------------------------------------------------------------------//

class OmsTwfFilled1 : public OmsTwfFilled {
   fon9_NON_COPY_NON_MOVE(OmsTwfFilled1);
   using base = OmsTwfFilled;

protected:
   void RunReportInCore_FilledOrder(OmsReportChecker&& checker, OmsOrder& order) override;
   OmsOrderRaw* RunReportInCore_FilledMakeOrder(OmsReportChecker& checker) override;
   void RunReportInCore_FilledUpdateCum(OmsReportRunnerInCore&& inCoreRunner) const override;

public:
   using base::base;
   using base::MakeFields;
   static OmsRequestSP MakeReportIn(OmsRequestFactory& creator, f9fmkt_RxKind reqKind) {
      (void)reqKind; assert(reqKind == f9fmkt_RxKind_Filled || reqKind == f9fmkt_RxKind_Unknown);
      return new OmsTwfFilled1{creator};
   }
};

class OmsTwfFilled1Factory : public OmsReportFactoryT<OmsTwfFilled1> {
   fon9_NON_COPY_NON_MOVE(OmsTwfFilled1Factory);
   using base = OmsReportFactoryT<OmsTwfFilled1>;
public:
   /// this->OrderFactory_; 提供一般委託(OmsTwfOrder1);
   /// 這裡提供「報價」委託.
   const OmsOrderFactorySP QuoteOrderFactory_;

   OmsTwfFilled1Factory(std::string name, OmsOrderFactorySP ord1Factory, OmsOrderFactorySP quoteOrderFactory)
      : base(std::move(name), std::move(ord1Factory))
      , QuoteOrderFactory_{quoteOrderFactory} {
   }
};
using OmsTwfFilled1FactorySP = fon9::intrusive_ptr<OmsTwfFilled1Factory>;

//--------------------------------------------------------------------------//

class OmsTwfFilled2 : public OmsTwfFilled {
   fon9_NON_COPY_NON_MOVE(OmsTwfFilled2);
   using base = OmsTwfFilled;

protected:
   void RunReportInCore_FilledOrder(OmsReportChecker&& checker, OmsOrder& order) override;
   OmsOrderRaw* RunReportInCore_FilledMakeOrder(OmsReportChecker& checker) override;
   void RunReportInCore_FilledUpdateCum(OmsReportRunnerInCore&& inCoreRunner) const override;

public:
   using base::base;

   // PriLeg1_ = this->Pri_;
   OmsTwfPri   PriLeg2_;

   OmsTwfPri GetFilledPri(f9twf::ExgCombSide side) const {
      switch (side) {
      case f9twf::ExgCombSide::SameSide:   return this->Pri_ + this->PriLeg2_;
      case f9twf::ExgCombSide::SideIsLeg1: return this->Pri_ - this->PriLeg2_;
      case f9twf::ExgCombSide::SideIsLeg2: return this->PriLeg2_ - this->Pri_;
      case f9twf::ExgCombSide::None:       break;
      }
      assert(!"OmsTwfFilled2::GetFilledPri(): Unknown CombSide.");
      return this->Pri_;
   }

   static OmsRequestSP MakeReportIn(OmsRequestFactory& creator, f9fmkt_RxKind reqKind) {
      (void)reqKind; assert(reqKind == f9fmkt_RxKind_Filled || reqKind == f9fmkt_RxKind_Unknown);
      return new OmsTwfFilled2{creator};
   }

   static void MakeFields(fon9::seed::Fields& flds);
};

using OmsTwfFilled2Factory = OmsReportFactoryT<OmsTwfFilled2>;
using OmsTwfFilled2FactorySP = fon9::intrusive_ptr<OmsTwfFilled2Factory>;

} // namespaces
#endif//__f9omstwf_OmsTwfFilled_hpp__
