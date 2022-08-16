// \file f9omstw/OmsParentOrder.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsParentOrder_hpp__
#define __f9omstw_OmsParentOrder_hpp__
#include "f9omstw/OmsParentBase.hpp"
#include "f9omstw/OmsOrder.hpp"

namespace f9omstw {

class OmsParentOrderRaw;

class OmsParentOrder : public OmsOrder {
   fon9_NON_COPY_NON_MOVE(OmsParentOrder);
   using base = OmsOrder;

   using ChildRequests = std::vector<const OmsRequestIni*>;
   ChildRequests  WorkingChildRequests_;

protected:
   /// 預設 do nothing.
   virtual void OnAfterChildOrderUpdated(const OmsRequestBase& parentIni, OmsRequestRunnerInCore&& parentRunner);
   virtual void OnChildFilled(OmsParentOrderRaw& parentOrdraw, const OmsRequestBase& childFilled) = 0;

public:
   OmsParentOrder() {
      this->SetThisIsParentOrder();
   }
   ~OmsParentOrder();

   size_t WorkingChildRequestCount() const {
      return this->WorkingChildRequests_.size();
   }
   void AddChild(const OmsRequestIni& childReq) {
      this->WorkingChildRequests_.push_back(&childReq);
   }
   bool EraseChild(const OmsRequestIni* childReq) {
      // childReq 無法繼續: Abandoned or !IsWorkingOrder(Rejected or 全部成交);
      assert(childReq->IsAbandoned() || !childReq->LastUpdated()->Order().IsWorkingOrder());
      auto ifind = std::find(this->WorkingChildRequests_.begin(), this->WorkingChildRequests_.end(), childReq);
      assert(ifind != this->WorkingChildRequests_.end());
      if (ifind == this->WorkingChildRequests_.end())
         return false;
      this->WorkingChildRequests_.erase(ifind);
      return true;
   }

   struct OnEachWorkingChild {
      enum CheckResult {
         NeedThisChild,
         SkipThisChild,
         BreakEachChild,
      };
      virtual ~OnEachWorkingChild() {}
      /// 預設為 NeedThisChild;
      virtual CheckResult CheckNeedsChildChange(const OmsRequestIni& childReqIni);
      /// childReqIni.RxSNO()==0: childReqIni 尚未進入 RunInCore(); 刪改必須直接調整 childReqIni 的內容.
      virtual void DirectChange(OmsRequestRunnerInCore& parentRunner, OmsRequestIni& childReqIni, size_t workingIndex) = 0;
      /// 預設: reqUpd->SetIniFrom(*childReqIni);
      /// reqUpd->SetPolicy(static_cast<const OmsRequestTrade*>(&parentRunner.OrderRaw().Request())->Policy());
      virtual void SetupChildRequest(OmsRequestRunnerInCore& parentRunner, OmsRequestRunner& childRunner, const OmsRequestIni& childReqIni);
      /// 中斷尚未進入 RunInCore() 之前的子單.
      /// - 此時: assert(childReqIni.RxSNO() == 0 && childReqIni.GetParentRequest() != nullptr);
      /// - 先切斷與母單的聯繫: childReqIni.ClearParentRequest();
      /// - 中斷子單的執行:     childReqIni.Abandon();
      /// - 從子單列表中移除:   this->WorkingChildRequests_.erase(this->WorkingChildRequests_.begin() + workingIndex);
      /// - 調整已執行的數量:   parentRunner.OrderRawT<OmsParentOrderRaw>().DecChildParentLeavesQty(this->GetChildRequestIniQty(childReqIni));
      void OnAbandonBeforeRunChild(OmsRequestRunnerInCore& parentRunner, OmsRequestIni& childReqIni, size_t workingIndex);
   };
   /// 返回送出的子單要求的筆數.
   size_t ChgEachWorkingChild(OmsRequestRunnerInCore&& parentRunner, OmsRequestFactory& chgfac, OnEachWorkingChild& maker);
   size_t DelEachWorkingChild(OmsRequestRunnerInCore&& parentRunner, OmsRequestFactory& chgfac);

   /// 成交回報 or 手動刪改子單回報.
   /// - 會先排除 childRequest.IsSynReport();
   /// - 若 childRunner 為成交, 則會透過 this->OnChildFilled() 處理成交回報.
   /// - 返回前:
   ///   會呼叫 this->OnAfterChildOrderUpdated(OmsRequestRunnerInCore&& parentRunner);
   ///   由衍生者決定後續動作(例: 建立新的子單).
   void OnChildOrderUpdated(const OmsRequestRunnerInCore& childRunner) override;

   using FnOnAfterChildOrderUpdated = void (*)(OmsParentOrder& parentOrder, const OmsRequestBase& parentIni, OmsRequestRunnerInCore&& parentRunner, void* udata);
   void RunChildOrderUpdated(const OmsOrderRaw& childOrdraw, OmsResource& res, const OmsRequestRunnerInCore* childRunner, void* udata,
                             FnOnAfterChildOrderUpdated fnOnAfterChildOrderUpdated);

   /// childLeavesQty = childOrdraw.LeavesQty_;
   /// childAdjQty = fon9::signed_cast(childOrdraw.AfterQty_ - childOrdraw.BeforeQty_);
   template <class ChildOrderRaw>
   static void GetChildOrderQtysT(const OmsOrderRaw& childOrdraw, OmsParentQty& childLeavesQty, OmsParentQtyS& childAdjQty) {
      assert(dynamic_cast<const ChildOrderRaw*>(&childOrdraw) != nullptr);
      const auto& child = *static_cast<const ChildOrderRaw*>(&childOrdraw);
      childLeavesQty = child.LeavesQty_;
      childAdjQty = fon9::signed_cast(child.AfterQty_ - child.BeforeQty_);
   }
   /// 請參考 GetChildOrderQtysT<>();
   virtual void GetChildOrderQtys(const OmsOrderRaw& childOrdraw, OmsParentQty& childLeavesQty, OmsParentQtyS& childAdjQty) const = 0;
   virtual OmsParentQty GetChildRequestIniQty(const OmsRequestIni& childReqIni) const = 0;
};
//--------------------------------------------------------------------------//
/// - BeforeQty_, AfterQty_ 用來表示 LeavesQty_ 異動前後的數量;
///   - 不包含 CumQty;
///   - 子單刪改成功.
///   - 改量(可增加).
/// - ChildLeavesQty_ = 子單尚未成交的數量:Sum(Child.LeavesQty);
/// - LeavesQty_ = 尚未建立子單的數量 + ChildLeavesQty_;
struct OmsParentOrderRawDat {
   OmsParentQty   RequireQty_;
   OmsParentQty   ChildLeavesQty_;
   OmsParentQty   LeavesQty_;
   OmsParentQty   BeforeQty_;
   OmsParentQty   AfterQty_;
   void DecChildParentLeavesQty(OmsParentQty adjQty) {
      assert(fon9::signed_cast(adjQty) > 0);
      assert(this->LeavesQty_ >= adjQty && this->ChildLeavesQty_ >= adjQty);
      if (fon9::signed_cast(this->ChildLeavesQty_ -= adjQty) < 0)
         this->ChildLeavesQty_ = 0;
      if (fon9::signed_cast(this->LeavesQty_ -= adjQty) < 0)
         this->LeavesQty_ = 0;
      this->AfterQty_ = this->LeavesQty_;
   }

   OmsParentQty   CumQty_;
   OmsParentAmt   CumAmt_;

   /// 複式單第1腳、第2腳的累計成交值、成交量.
   /// 單式單, 這裡不使用, 固定為 0.
   OmsParentQty   Leg1CumQty_;
   OmsParentQty   Leg2CumQty_;
   OmsParentAmt   Leg1CumAmt_;
   OmsParentAmt   Leg2CumAmt_;

   /// 全部內容清為 '\0' 或 Null()
   void ClearRawDat() {
      fon9::ForceZeroNonTrivial(this);
   }
   /// 先從 prev 複製全部, 然後修改:
   /// - this->BeforeQty_ = this->AfterQty_ = this.LeavesQty_;
   void ContinuePrevUpdate(const OmsParentOrderRawDat& prev) {
      memcpy(this, &prev, sizeof(*this));
      this->BeforeQty_ = this->AfterQty_ = this->LeavesQty_;
   }
};

class OmsParentOrderRaw : public OmsOrderRaw, public OmsParentOrderRawDat {
   fon9_NON_COPY_NON_MOVE(OmsParentOrderRaw);
   using base = OmsOrderRaw;
public:
   using base::base;
   OmsParentOrderRaw() {
      this->ClearRawDat();
   }

   static void MakeFields(fon9::seed::Fields& flds);

   /// - base::ContinuePrevUpdate(prev);
   /// - OmsParentOrderRawDat::ContinuePrevUpdate(*static_cast<const OmsParentOrderRawDat*>(&prev));
   void ContinuePrevUpdate(const OmsOrderRaw& prev) override;

   void OnOrderReject() override;
   bool IsWorking() const override;
};

template <class OrderRawDat>
class OmsParentOrderRawT : public OmsParentOrderRaw, public OrderRawDat {
   fon9_NON_COPY_NON_MOVE(OmsParentOrderRawT);
   using base = OmsParentOrderRaw;
public:
   using base::base;
   OmsParentOrderRawT() {
      this->ClearRawDat();
      fon9::ForceZeroNonTrivial(static_cast<OrderRawDat*>(this));
   }

   static void MakeFields(fon9::seed::Fields& flds) {
      base::MakeFields(flds);
      OrderRawDat::CxMakeFields(flds, static_cast<OmsParentOrderRawT*>(nullptr));
   }

   void ContinuePrevUpdate(const OmsOrderRaw& prev) override {
      *static_cast<OrderRawDat*>(this) = *static_cast<const OmsParentOrderRawT*>(&prev);
      base::ContinuePrevUpdate(prev);
   }
};

} // namespaces
#endif//__f9omstw_OmsParentOrder_hpp__
