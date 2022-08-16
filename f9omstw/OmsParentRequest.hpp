// \file f9omstw/OmsParentRequest.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsParentRequest_hpp__
#define __f9omstw_OmsParentRequest_hpp__
#include "f9omstw/OmsParentBase.hpp"
#include "f9omstw/OmsOrder.hpp"

namespace f9omstw {

/// 母單預設的櫃號. 必須自行在 InitCoreTables() 期間先設定好.
/// 母單雖然不會送給交易所, 但因底下原因, 必須[編製委託書號]:
/// - 在多主機同步機制, 必須使用委託書號來找尋.
/// - 如果交易所下單時有提供 user_define 欄位,
///   可在送交易所時加上[母單委託書號]註記,
///   這樣可以在線路切到備援主機時, 有尋找母單的依據.
extern OmsOrdTeamGroupId  gParentRequestTgId;
//--------------------------------------------------------------------------//
using RunningChildCount = uint32_t;
/// \retval true: (res遞減後) == 0;
static inline bool DecRunningChildCount(RunningChildCount& res) {
   assert(res > 0);
   return(res == 0 ? 0 : --res) == 0;
}
//--------------------------------------------------------------------------//
/// - OmsParentRequestIni 對應的 Order 必須是 OmsParentOrder 及 OmsParentOrderRaw;
/// - 每一筆 Child 新單失敗, 都需要調整 ParentOrdraw.ChildLeavesQty,
///   所以每次 Child 新單失敗, 都會更新一次 ParentOrder;
class OmsParentRequestIni : public OmsRequestIni {
   fon9_NON_COPY_NON_MOVE(OmsParentRequestIni);
   using base = OmsRequestIni;

   void OnChildError(const OmsRequestIni& childReq, OmsResource& res, const OmsRequestRunnerInCore* childRunner) const;

   mutable RunningChildCount  RunningChildCount_{0};
   char  Padding____[4];

protected:
   const OmsRequestBase* ReportRef_{};
   void RunReportInCore(OmsReportChecker&& checker) override;
   /// 預設: do nothing.
   virtual void OnParentReportInCore_Filled(OmsParentOrderRaw& ordraw);
   /// 新增[新單]回報.
   virtual void OnParentReportInCore_NewReport(OmsReportRunnerInCore& runner);
   /// [新單]異動回報.
   virtual void OnParentReportInCore_NewUpdate(OmsReportRunnerInCore& runner);
   /// 新增[改單]回報.
   virtual void OnParentReportInCore_ChgReport(OmsReportRunnerInCore& runner);
   /// [改單]異動回報.
   virtual void OnParentReportInCore_ChgUpdate(OmsReportRunnerInCore& runner);

   virtual OmsParentQty GetChildRequestQty(const OmsRequestIni& childReq) const = 0;

   template <class ChildOrderRaw, class ChildRequestIni>
   static void GetNewChildOrderQtysT(const OmsOrderRaw& childOrdraw, OmsParentQty& childLeavesQty, OmsParentQtyS& childAdjQty) {
      assert(dynamic_cast<const ChildOrderRaw*>(&childOrdraw) != nullptr);
      assert(dynamic_cast<const ChildRequestIni*>(&childOrdraw.Request()) != nullptr);
      const auto& child = *static_cast<const ChildOrderRaw*>(&childOrdraw);
      childLeavesQty = child.LeavesQty_;
      childAdjQty = fon9::signed_cast(child.AfterQty_
                                      - (child.BeforeQty_ ? child.BeforeQty_
                                         : static_cast<const ChildRequestIni*>(&childOrdraw.Request())->Qty_));
   }
   /// 請參考 GetNewChildOrderQtysT<>();
   virtual void GetNewChildOrderQtys(const OmsOrderRaw& childOrdraw, OmsParentQty& childLeavesQty, OmsParentQtyS& childAdjQty) const = 0;

   void OnAddChildRequest(const OmsRequestBase& childReq, OmsRequestRunnerInCore* parentRunner) const override;
   void OnAfterBackendReloadChild(const OmsRequestBase& childReq) const override;
   void OnAfterBackendReload(OmsResource& res, void* backendLocker) const override;

public:
   using base::base;
   OmsParentRequestIni() = default;
   void OnSynReport(const OmsRequestBase* ref, fon9::StrView message) override;
   bool IsParentRequest() const override;
   void OnChildAbandoned(const OmsRequestBase& childReq, OmsResource& res) const override;
   void OnChildOrderUpdated(const OmsRequestRunnerInCore& childRunner) const override;
   void OnMoveChildRunnerToCoreError(OmsRequestRunnerInCore& parentRunner, OmsRequestRunner&& childRunner) const override;
};
//--------------------------------------------------------------------------//
class OmsParentRequestChg : public OmsRequestUpd {
   fon9_NON_COPY_NON_MOVE(OmsParentRequestChg);
   using base = OmsRequestUpd;
   void OnChildError(const OmsRequestBase& childReq, OmsResource& res, const OmsRequestRunnerInCore* childRunner) const;
   void OnAddChildRequest(const OmsRequestBase& childReq, OmsRequestRunnerInCore* parentRunner) const override;
   void OnAfterBackendReloadChild(const OmsRequestBase& childReq) const override;

   mutable RunningChildCount  RunningChildCount_{0};

public:
   /// 數量(股數): =0刪單, >0期望改後數量(含成交), <0想要減少的數量.
   using ChgQtyType = fon9::make_signed_t<OmsParentQty>;
   ChgQtyType  Qty_{0};

   using base::base;
   OmsParentRequestChg() = default;

   RunningChildCount GetRunningChildCount() const {
      return this->RunningChildCount_;
   }

   bool IsParentRequest() const override;
   void OnChildAbandoned(const OmsRequestBase& childReq, OmsResource& res) const override;
   void OnChildOrderUpdated(const OmsRequestRunnerInCore& childRunner) const override;
   void OnMoveChildRunnerToCoreError(OmsRequestRunnerInCore& parentRunner, OmsRequestRunner&& childRunner) const override;
};

} // namespaces
#endif//__f9omstw_OmsParentRequest_hpp__
