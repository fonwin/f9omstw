// \file f9omstw/OmsParentRequest.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsParentRequest_hpp__
#define __f9omstw_OmsParentRequest_hpp__
#include "f9omstw/OmsParentBase.hpp"
#include "f9omstw/OmsOrder.hpp"

namespace f9omstw {

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

   void OnChildError(const OmsRequestBase& childReq, OmsResource& res, const OmsRequestRunnerInCore* childRunner) const;

   mutable RunningChildCount  RunningChildCount_{0};
   /// 原始來源主機, 在 RcSyn 收到回報時填入.
   /// 在使用 SeedCommand 啟用 ParentOrder 時使用;
   /// 不儲存, 重啟後歸0: 因為系統重啟前, Parent/Child 的狀態都無法正確更新, 所以重啟後不可以啟用 ParentOrder.
   fon9::HostId               OrigHostId_{};
   /// 透過 this->OnSynReport() 設定;
   /// 在處理回報時的原始下單要求(新刪改): 從 RcSyn 收到回報時的 ReferenceSNO 取得.
   /// 協助 this->RunReportInCore() 的處理.
   const OmsRequestBase*      ReportRef_{};

protected:
   void RunReportInCore(OmsReportChecker&& checker) override;
   /// 若不需要處理回報, 則應返回 false; 並在返回前呼叫 checker.ReportAbandon();
   /// 預設: 透過 this->IsNeedsReport() 檢查;
   virtual bool IsNeedsReport_ForNew(OmsReportChecker& checker, const OmsRequestBase& ref);
   virtual bool IsNeedsReport_ForChg(OmsReportChecker& checker, const OmsRequestBase& ref);
   /// return(this->GetSrcUTime() > Tail.SrcUTime_);
   virtual bool IsNeedsReport(const OmsRequestBase& ref) const;
   /// 回報來源的異動時間, 用於協助判斷是否為重複回報, 來自回報的 OrdRaw.UpdateTime 欄位;
   /// 母單回報 Request(例: BergReport), 必須包含 UpdateTime 欄位, 並在此返回.
   /// 預設返回 fon9::TimeStamp::Null();
   virtual fon9::TimeStamp GetSrcUTime() const;

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
   void OnChildRequestUpdated(const OmsRequestRunnerInCore& childRunner) const override;
   void OnMoveChildRunnerToCoreError(OmsRequestRunnerInCore& parentRunner, OmsRequestRunner&& childRunner) const override;

   fon9::HostId OrigHostId() const override;
   void SetOrigHostId(fon9::HostId origHostId) override;
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
   void OnChildRequestUpdated(const OmsRequestRunnerInCore& childRunner) const override;
   void OnMoveChildRunnerToCoreError(OmsRequestRunnerInCore& parentRunner, OmsRequestRunner&& childRunner) const override;
};

} // namespaces
#endif//__f9omstw_OmsParentRequest_hpp__
