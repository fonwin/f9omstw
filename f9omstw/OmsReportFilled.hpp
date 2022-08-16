// \file f9omstw/OmsReportFilled.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsReportFilled_hpp__
#define __f9omstw_OmsReportFilled_hpp__
#include "f9omstw/OmsRequestBase.hpp"

namespace f9omstw {

/// 成交回報.
/// 在 OmsOrder 提供:
/// \code
///    OmsReportFilled* OmsOrder::FilledHead_;
///    OmsReportFilled* OmsOrder::FilledLast_;
/// \endcode
/// 新的成交, 如果是在 FilledLast_->MatchKey_ 之後, 就直接 append; 否則就從頭搜尋.
/// 由於成交回報「大部分」是附加到尾端, 所以這樣的處理負擔應是最小.
class OmsReportFilled : public OmsRequestBase {
   fon9_NON_COPY_NON_MOVE(OmsReportFilled);
   using base = OmsRequestBase;

   const OmsReportFilled mutable* Next_{nullptr};

   /// 將 curr 插入 this 與 this->Next_(可能為nullptr) 之間;
   /// 不支援: this 是新的 head, 因 curr 是舊的成交串列.
   void InsertAfter(const OmsReportFilled* curr) const {
      assert(curr->Next_ == nullptr && this->MatchKey_ < curr->MatchKey_);
      assert(this->Next_ == nullptr || curr->MatchKey_ < this->Next_->MatchKey_);
      curr->Next_ = this->Next_;
      this->Next_ = curr;
   }

   static void MakeFieldsImpl(fon9::seed::Fields& flds);

protected:
   template <class Derived>
   static void MakeFields(fon9::seed::Fields& flds) {
      fon9_GCC_WARN_DISABLE("-Winvalid-offsetof");
      static_assert(offsetof(Derived, IniSNO_) == offsetof(OmsReportFilled, IniSNO_),
                    "'OmsReportFilled' must be the first base class in derived.");
      fon9_GCC_WARN_POP;

      MakeFieldsImpl(flds);
   }
   void InitializeForReportIn() {
      base::InitializeForReportIn();
      this->SetReportSt(f9fmkt_TradingRequestSt_Filled);
   }

   /// 在 RunReportInCore_MakeReqUID() 時, 轉呼叫此處.
   /// - Market[1] + Session[1] + [RevFilledReqUID()] + MatchKey_;
   /// - 通常在此填入:
   ///   - *--pout = this->Side_;
   ///   - memcpy(pout -= 2, pBrkEnd - 2, 2);
   virtual char* RevFilledReqUID(char* pout) = 0;
   void RunReportInCore_MakeReqUID() override;

   /// - 先用 this->ReqUID_ 尋找 origReq, 或 OrdKey 尋找 order, 如果有找到:
   ///   - 重編 ReqUID_;
   ///   - 透過 this->RunReportInCore_FilledOrder() 處理成交回報.
   /// - 如果沒找到 order:
   ///   - 先透過 this->RunReportInCore_FilledMakeOrder(); 建立委託.
   ///   - 如果 order 建立成功, 則等候 order 的新單回報.
   void RunReportInCore_Filled(OmsReportChecker&& checker);
   /// 在 RunReportInCore_Filled(), 如果找不到 order, 則透過這裡建立新的 order, 用來等候新單回報.
   /// 預設使用 this->Creator().OrderFactory_->MakeOrder(); 建立 order.
   virtual OmsOrderRaw* RunReportInCore_FilledMakeOrder(OmsReportChecker& checker);
   /// 在 RunReportInCore_Filled() 有找到原委託時, 透過這裡處理成交回報.
   /// - 如果新單已完成(order.LastOrderSt() >= f9fmkt_OrderSt_NewDone),
   ///   則透過 RunReportInCore_FilledUpdateCum() 更新委託內容.
   /// - 如果新單未完成, 則 inCoreRunner.UpdateFilled(f9fmkt_OrderSt_ReportPending); 等候新單完成後再處理.
   virtual void RunReportInCore_FilledOrder(OmsReportChecker&& checker, OmsOrder& order);
   /// 在 RunReportInCore_FilledOrder(); 取得 order.Initiator() 之後, 檢查基本欄位是否一致.
   virtual bool RunReportInCore_FilledIsFieldsMatch(const OmsRequestIni& ini) const = 0;
   /// 在 RunReportInCore_FilledOrder(), 確定新單已完成(order.LastOrderSt() >= f9fmkt_OrderSt_NewDone)
   /// 則透過這裡更新 inCoreRunner.OrderRaw_ 的成交內容.
   virtual void RunReportInCore_FilledUpdateCum(OmsReportRunnerInCore&& inCoreRunner) const = 0;
   /// 在有 initiator 的情況下, 若新單尚未完成, 就收到了成交, 通知衍生者處理:
   /// - 台灣證券: 證交所接受的委託量, 可能會小於新單要求的數量(可能因TIF刪減剩餘量、超過上限...).
   ///   - 證交所作法為: 在新單回報時告知「新單接受數量」, 所以必須等後新單結果.
   ///   - 因此台灣證券的成交, 這裡 do nothing, 因為要等新單結果才能確知「接受數量」.
   /// - 台灣期權: 期交所接受的委託量, 必定與新單要求數量相同.
   ///   - 期交所針對 TIF 刪除剩餘的做法: 在回報完畢後, 最後提供一個「刪除剩餘」的成交回報.
   ///   - 期交所有「新單合併成交」回報.
   ///   - 因此一旦收到成交, 就表示新單已接受.
   ///   - 一般委託: 直接先進入 ExchangeAccepted 的狀態, 再處理後續回報.
   ///   - 報價委託: 須等候雙邊完成, 所以:
   ///     - 若 this.Side==Buy 則須等候 Offer 回報.
   ///     - 若 this.Side==Offer 則進入 ExchangeAccepted 狀態.
   /// - 預設 do nothing.
   virtual void RunReportInCore_FilledBeforeNewDone(OmsResource& resource, OmsOrder& order);
   /// 在 order.LastOrderSt() >= f9fmkt_OrderSt_NewDone 的情況下,
   /// 檢查是否需要暫留成交回報, 等後續回報補齊後再透過 ProcessPendingReport() 處理.
   /// 預設返回 false;
   virtual bool RunReportInCore_FilledIsNeedsReportPending(const OmsOrderRaw& lastOrdUpd) const;

public:
   using MatchKey = uint64_t;
   OmsRxSNO IniSNO_{};
   MatchKey MatchKey_{};

   OmsReportFilled(OmsRequestFactory& creator)
      : base{creator, f9fmkt_RxKind_Filled} {
      this->InitializeForReportIn();
   }
   OmsReportFilled()
      : base{f9fmkt_RxKind_Filled} {
      this->InitializeForReportIn();
   }

   /// 成交回報, 是否為 Internal?
   /// - ReqUID_ == "I:原有規則" 則為[強制Internal];
   ///   在 OmsReportFilled::RunReportInCore_MakeReqUID() 裡面設定,
   ///   在 IsEnumContains(this->RequestFlags(), OmsRequestFlag_ForceInternal) 時;
   /// - 與 ini 相同.
   bool IsForceInternalRpt() const {
      return this->ReqUID_.Chars_[0] == 'I' && this->ReqUID_.Chars_[1] == ':';
   }
                    
   /// 將 curr 依照 MatchKey_ 的順序(小到大), 加入到「成交串列」.
   /// \retval nullptr  成功將 curr 加入成交串列.
   /// \retval !nullptr 與 curr->MatchKey_ 相同的那個 request.
   static const OmsReportFilled* Insert(const OmsReportFilled** ppHead,
                                        const OmsReportFilled** ppLast,
                                        const OmsReportFilled* curr);

   const OmsReportFilled* Next() {
      return this->Next_;
   }

   /// 轉呼叫 RunReportInCore_Filled();
   void RunReportInCore(OmsReportChecker&& checker) override;
   /// 處理 order.LastOrderSt() >= f9fmkt_OrderSt_NewDone 之後的成交回報:
   /// if (this->RunReportInCore_FilledIsFieldsMatch(*order.Initiator()))
   ///    this->RunReportInCore_FilledUpdateCum(std::move(inCoreRunner));
   void ProcessPendingReport(const OmsRequestRunnerInCore& prevRunner) const override;

   /// 預設, 從 ref 複製: OmsOrdKey; Market; SessionId; IniSNO;
   void OnSynReport(const OmsRequestBase* ref, fon9::StrView message) override;
};

//--------------------------------------------------------------------------//

/// 一般用在成交回報, 計算 AfterQty_ < 0; 則應在 inCoreRunner.ExLogForUpd_ 留下記錄.
void OmsBadAfterWriteLog(OmsReportRunnerInCore& inCoreRunner, int afterQty);

/// 計算 ordQtys 的成交累計(CumQty, CumAmt), 並調整 ordQtys.LeavesQty_ = (ordQtys.AfterQty_ -= qtyFilled);
template <class TOrdQtys, class PriT, class QtyT>
inline void OmsFilledUpdateCumFromReport(OmsReportRunnerInCore& inCoreRunner,
                                         TOrdQtys& ordQtys, PriT priFilled, QtyT qtyFilled) {
   using AmtT = decltype(ordQtys.CumAmt_);
   ordQtys.CumAmt_ += AmtT(priFilled) * qtyFilled;

   fon9_GCC_WARN_DISABLE("-Wconversion");
   ordQtys.CumQty_ += qtyFilled;
   ordQtys.AfterQty_ -= qtyFilled;
   fon9_GCC_WARN_POP;

   if (fon9::signed_cast(ordQtys.AfterQty_) < 0) {
      OmsBadAfterWriteLog(inCoreRunner, fon9::signed_cast(ordQtys.AfterQty_));
      ordQtys.AfterQty_ = 0;
   }
   ordQtys.LeavesQty_ = ordQtys.AfterQty_;
}

/// 如果有 rptFilled->PriOrd_ 則視情況更新 ordraw->LastPri*;
/// 通常用於「範圍市價」+「新單合併成交回報」由交易所提供「真實委託價」.
template <class TOrdRaw, class TRptFilled>
inline auto OmsRunReportInCore_FilledUpdateOrdPri(TOrdRaw* ordraw, const TRptFilled* rptFilled)
->decltype(ordraw->LastPri_, rptFilled->PriOrd_, void()) {
   if (!rptFilled->PriOrd_.IsNullOrZero()) {
      if (ordraw->LastPriTime_.IsNull() || ordraw->LastPriTime_ < rptFilled->Time_) {
         ordraw->LastPriTime_ = rptFilled->Time_;
         ordraw->LastPri_ = rptFilled->PriOrd_;
      }
   }
}
inline void OmsRunReportInCore_FilledUpdateOrdPri(...) {
}

/// 計算 ordQtys 的成交累計(CumQty, CumAmt), 並調整 ordraw.LeavesQty_ 及 ordQtys.AfterQty_;
/// - 最後返回前, 呼叫:
///   inCoreRunner.UpdateFilled(ordQtys.LeavesQty_  <= 0
///                             ? f9fmkt_OrderSt_FullFilled : f9fmkt_OrderSt_PartFilled);
template <class OmsReportRunnerInCore, class TOrdQtys, class PriT, class QtyT, class TRptFilled>
inline void OmsRunReportInCore_FilledUpdateCum(OmsReportRunnerInCore&& inCoreRunner,
                                               TOrdQtys& ordQtys, PriT priFilled, QtyT qtyFilled,
                                               const TRptFilled& rptFilled) {
   OmsFilledUpdateCumFromReport(inCoreRunner, ordQtys, priFilled, qtyFilled);
   OmsRunReportInCore_FilledUpdateOrdPri(&ordQtys, &rptFilled);
   inCoreRunner.UpdateFilled(ordQtys.LeavesQty_ <= 0
                             ? f9fmkt_OrderSt_FullFilled : f9fmkt_OrderSt_PartFilled,
                             rptFilled);
}

} // namespaces
#endif//__f9omstw_OmsReportFilled_hpp__
