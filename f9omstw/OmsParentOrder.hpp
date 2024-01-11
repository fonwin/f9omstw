﻿// \file f9omstw/OmsParentOrder.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsParentOrder_hpp__
#define __f9omstw_OmsParentOrder_hpp__
#include "f9omstw/OmsParentBase.hpp"
#include "f9omstw/OmsOrder.hpp"

namespace f9omstw {

class OmsParentOrderRaw;
struct OmsParentOrderRawDat;

class OmsParentOrder : public OmsOrder {
   fon9_NON_COPY_NON_MOVE(OmsParentOrder);
   using base = OmsOrder;

   using ChildRequests = std::vector<const OmsRequestIni*>;
   ChildRequests  WorkingChildRequests_;

   /// 重新執行母單.
   /// newParent = RecalcAllChild() 的返回值, 可能為 nullptr, 表示最後狀態沒變, 但需要 Rerun.
   /// 若有提供 newParent, 則不論返回值, 返回前都有需要主動執行: OmsRequestRunnerInCore;
   virtual bool OnRerunParent(OmsResource& resource, OmsParentOrderRaw* newParent) = 0;
   /// 返回新的母單最後狀態, 若沒有變動, 則返回 nullptr;
   /// 若返回有異動, 則需: OmsRequestRunnerInCore runner{resource, *retval...};
   OmsParentOrderRaw* RecalcAllChild();
   /// recalc 為單純的 OmsParentOrderRawDat, 不可 cast 成別種型別.
   /// 重新將 child 累加到 recalc 裡面, 包含:
   /// - recalc.ChildLeavesQty_ += childTail.LeavesQty_;
   /// - recalc.CumQty_         += childTail.CumQty_;
   /// - recalc.CumAmt_         += childTail.CumAmt_;
   /// - recalc.Leg1CumQty_     += childTail.Leg1CumQty_;
   /// - recalc.Leg2CumQty_     += childTail.Leg2CumQty_;
   /// - recalc.Leg1CumAmt_     += childTail.Leg1CumAmt_;
   /// - recalc.Leg2CumAmt_     += childTail.Leg2CumAmt_;
   virtual void RecalcParent(OmsParentOrderRawDat& recalc, const OmsOrderRaw& childTail) const = 0;

protected:
   /// 預設 do nothing.
   virtual void OnAfterChildOrderUpdated(const OmsRequestBase& parentIni, OmsRequestRunnerInCore&& parentRunner, const OmsOrderRaw& childOrdraw);
   /// 預設 do nothing.
   virtual void OnChildFilled(OmsParentOrderRaw& parentOrdraw, const OmsRequestBase& childFilled) = 0;

   /// 設定: "Parent rerun@LocalHostId"
   static void AssignRerunMessage(fon9::CharVector& message);
   bool ParentRerunStep(OmsResource& resource, OmsParentOrderRaw* newParent, OmsRequestRunStep& rerunStep);

public:
   OmsParentOrder() {
      this->SetThisIsParentOrder();
   }
   ~OmsParentOrder();

   size_t WorkingChildRequestCount() const {
      return this->WorkingChildRequests_.size();
   }
   /// 將 childReq 加入 Parent.
   /// 當 Parent 的刪改需要觸及已送出的 Child 時, 從這裡查找工作中的 Child.
   void AddChild(const OmsRequestIni& childReq) {
      assert(childReq.RxKind() == f9fmkt_RxKind_RequestNew);
      assert(std::find(this->WorkingChildRequests_.begin(), this->WorkingChildRequests_.end(), &childReq) == this->WorkingChildRequests_.end());
      this->WorkingChildRequests_.push_back(&childReq);
   }
   /// 移除已完成的 childReq: Abandoned or !IsWorkingOrder(Rejected or 全部成交 or 刪單);
   bool EraseChild(const OmsRequestIni* childReq);
   void OnOrderUpdated(const OmsRequestRunnerInCore& runner) override;
   void OnAfterBackendReload(OmsResource& res, void* backendLocker);

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
   /// - 必須是本地工作中的母單才會更新, 否則由遠端更新.
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

   /// - 先判斷是否需要重新執行?
   ///   - 是否正在執行中?
   ///   - this->Tail()->LeavesQty_ > 0?
   /// - 如果不需重新執行, 則返回 false;
   /// - 如果需要重新執行:
   ///   - 重新計算母單狀態;
   ///   - 則返回 this->OnRerunParent(); 由衍生者提供;
   bool RerunParent(OmsResource& resource) override;
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
   /// 原始回報來源的異動時間, 用於協助判斷是否為重複回報.
   /// 來自回報的 OrdRaw.UpdateTime 欄位;
   fon9::TimeStamp   SrcUTime_;

   /// 全部內容清為 '\0' 或 Null()
   void ClearRawDat() {
      fon9::ForceZeroNonTrivial(this);
      this->SrcUTime_.AssignNull();
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
   OmsFilledFlag HasFilled() const override;
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
