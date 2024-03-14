// \file f9omstw/OmsReportTools.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsReportTools_hpp__
#define __f9omstw_OmsReportTools_hpp__
#include "f9omstw/OmsReportRunner.hpp"
#include "f9omstw/OmsResource.hpp"

namespace f9omstw {

enum class OmsReportQtyStyle : uint8_t {
   /// 依照 SessionId() 而定, 例如:
   /// - f9fmkt_TradingSessionId_OddLot 使用 OddLot;
   /// - f9fmkt_TradingSessionId_Normal, f9fmkt_TradingSessionId_FixedPrice
   ///   使用 BoardLot.
   BySessionId,
   /// 回報數量提供的是「張數」;
   BoardLot,
   /// 回報數量提供的是「股數」;
   OddLot,
};
/// - retval=0:  表示回報數量不用調整.
/// - retval!=0: 回報的正確數量 = 數量 * retval;
template <class ReportT>
inline uint32_t OmsGetReportQtyUnit(OmsOrder& order, OmsResource& res, ReportT& rpt) {
   switch (rpt.QtyStyle_) {
   case OmsReportQtyStyle::BySessionId:
      if (rpt.SessionId() == f9fmkt_TradingSessionId_OddLot)
         return 0;
      break;
   case OmsReportQtyStyle::OddLot:
      return 0;
   case OmsReportQtyStyle::BoardLot:
      break;
   }
   return fon9::fmkt::GetSymbTwsShUnit(order.GetSymb(res, rpt.Symbol_));
}

//--------------------------------------------------------------------------//

template <class OrderRawT, class ReportT>
inline auto OmsIsEqualLastTimeInForce(OrderRawT* ordraw, const ReportT* rpt)
-> decltype(ordraw->LastTimeInForce_, rpt->TimeInForce_, bool()) {
   return ordraw->LastTimeInForce_ == rpt->TimeInForce_;
}
inline bool OmsIsEqualLastTimeInForce(...) {
   return true;
}

template <class OrderRawT, class ReportT>
inline auto OmsAssignLastTimeInForceFromReport(OrderRawT* ordraw, const ReportT* rpt)
-> decltype(ordraw->LastTimeInForce_, rpt->TimeInForce_, void()) {
   ordraw->LastTimeInForce_ = rpt->TimeInForce_;
}
inline void OmsAssignLastTimeInForceFromReport(...) {
}

/// 若有 ordraw->LastPriTime_; 且 rpt->ExgTime_ 較新; 且 Pri* 有異動;
/// 則填入 ordraw->LastPri*; 及 LastTimeInForce_;
template <class OrderRawT, class ReportT>
inline auto OmsAssignLastPrisFromReport(OrderRawT* ordraw, const ReportT* rpt)
-> decltype(ordraw->LastPriTime_, void()) {
   if (ordraw->UpdateOrderSt_ == f9fmkt_OrderSt_ReportStale
       || f9fmkt_TradingRequestSt_IsAnyRejected(rpt->ReportSt())
       || (rpt->PriType_ == f9fmkt_PriType{} && rpt->Pri_.IsNull()))
      return;
   if (!ordraw->LastPriTime_.IsNull() && rpt->RxKind() != f9fmkt_RxKind_RequestNew) {
      if (rpt->PriType_ == f9fmkt_PriType_Limit && rpt->Pri_.IsNull())
         return; // rpt 的價格不正確, 不更新 ordraw->LastPri*
      if (ordraw->LastPriTime_ >= rpt->ExgTime_)
         return;
      if (rpt->PriType_ == f9fmkt_PriType_Unknown || ordraw->LastPriType_ == rpt->PriType_) {
         if (ordraw->LastPri_ == rpt->Pri_)
            if (OmsIsEqualLastTimeInForce(&ordraw, &rpt))
               return;
      }
   }
   // rpt 是有填價格的成功回報(新刪改查) => 一律將 Order.LastPri* 更新成交易所的最後價格.
   ordraw->LastPriTime_ = rpt->ExgTime_;
   if (rpt->PriType_ == f9fmkt_PriType_Unknown) {
      // 期交所 R22 回報, 沒有提供 PriType, 但如果是 Mwp 則會提供「實際價格」.
      // 此時不改變 ordraw->LastPriType_;
   }
   else {
      ordraw->LastPriType_ = rpt->PriType_;
   }
   if (!rpt->Pri_.IsNull())
      ordraw->LastPri_ = rpt->Pri_;
   OmsAssignLastTimeInForceFromReport(ordraw, rpt);
}
// 期貨報價單也有支援改價了, 為了避免 compiler 找錯, 所以拿掉此處.
// inline void OmsAssignLastPrisFromReport(...) {
// }

/// - rpt.ExgTime_.IsNull() 則直接離開, 不更新.
/// - 用 rpt.ExgTime_; 填入 ordraw.LastExgTime_;
/// - 如果有 ordraw.LastPriTime_ 則 rpt.PriType_; rpt.Pri_; 更新對應的 ordraw.LastPri* 內容.
/// - 如果有 rpt.TimeInForce_ 及 ordraw.LastTimeInForce_; 則也會更新.
template <class OrderRawT, class ReportT>
void OmsAssignTimeAndPrisFromReport(OrderRawT& ordraw, const ReportT& rpt) {
   // ordraw.LastExgTime_.IsNull(): ordraw 尚未收到交易所回報, 則須從 rpt 填入 ordraw.LastPri*
   // 這樣 [新單 Sending 的回報] 才能正確反映委託最後狀態.
   if (ordraw.LastExgTime_.IsNull() || !rpt.ExgTime_.IsNull()) {
      ordraw.LastExgTime_ = rpt.ExgTime_;
      OmsAssignLastPrisFromReport(&ordraw, &rpt);
   }
}

//--------------------------------------------------------------------------//

/// 若 LeavesQty_ < 0; 則應在 inCoreRunner.ExLogForUpd_ 留下記錄.
void OmsBadLeavesWriteLog(OmsReportRunnerInCore& inCoreRunner, int leavesQty);

/// 刪減回報已填妥 ordQtys.BeforeQty_ 及 ordQtys.AfterQty_ 之後, 重算 ordQtys.LeavesQty_;
/// - 計算方式: ordQtys.LeavesQty_ = ordQtys.LeavesQty_ + ordQtys.AfterQty_ - ordQtys.BeforeQty_;
/// - 若調整後的 ordQtys.LeavesQty_ < 0
///   - 在 inCoreRunner.ExLogForUpd_ 留下記錄,
///   - 設定 ordQtys.LeavesQty_ = 0;
/// - 若最終 ordQtys.LeavesQty_ == 0; 則會呼叫: OmsUpdateOrderSt_WhenRecalcLeaves0(inCoreRunner, ordQtys);
template <class OrdQtysT>
void OmsRecalcLeavesQtyFromReport(OmsReportRunnerInCore& inCoreRunner, OrdQtysT& ordQtys) {
   if (ordQtys.BeforeQty_ == ordQtys.AfterQty_)
      return;
   using QtyT = decltype(ordQtys.LeavesQty_);
   ordQtys.LeavesQty_ = static_cast<QtyT>(ordQtys.LeavesQty_ + ordQtys.AfterQty_ - ordQtys.BeforeQty_);
   if (fon9_UNLIKELY(fon9::signed_cast(ordQtys.LeavesQty_) < 0)) {
      OmsBadLeavesWriteLog(inCoreRunner, fon9::signed_cast(ordQtys.LeavesQty_));
      ordQtys.LeavesQty_ = 0;
   }
   if (ordQtys.LeavesQty_ == 0)
      OmsUpdateOrderSt_WhenRecalcLeaves0(inCoreRunner, ordQtys);
}
template <class OrdQtysT>
inline void OmsUpdateOrderSt_WhenRecalcLeaves0(OmsReportRunnerInCore& inCoreRunner, OrdQtysT& ordQtys) {
   assert(ordQtys.LeavesQty_ == 0);
   // 只允許單邊委託(OrderRaw 指包含一個 LeavesQty),
   // Bid/Offer 應使用: OmsUpdateOrderSt_WhenRecalcLeaves0_Quote();
   assert(&inCoreRunner.OrderRaw() == &ordQtys);
   inCoreRunner.OrderRaw().UpdateOrderSt_ = (ordQtys.AfterQty_ == 0
                                             ? f9fmkt_OrderSt_UserCanceled
                                             : f9fmkt_OrderSt_AsCanceled);
}
template <class OrdQuoteQtysT>
inline void OmsUpdateOrderSt_WhenRecalcLeaves0_Quote(OmsReportRunnerInCore& inCoreRunner, OrdQuoteQtysT& ordQuoteQtys) {
   if (ordQuoteQtys.Bid_.LeavesQty_ == 0 && ordQuoteQtys.Offer_.LeavesQty_ == 0)
      inCoreRunner.OrderRaw().UpdateOrderSt_ = (ordQuoteQtys.Bid_.AfterQty_ == 0 && ordQuoteQtys.Offer_.AfterQty_ == 0
                                                ? f9fmkt_OrderSt_UserCanceled
                                                : f9fmkt_OrderSt_AsCanceled);
}

/// 還原 OmsRecalcLeavesQtyFromReport() 扣掉的 LeavesQty;
template <class OrdQtysT>
void OmsRestoreLeavesQtyFromReport(OrdQtysT& ordQtys) {
   using QtyT = decltype(ordQtys.LeavesQty_);
   ordQtys.LeavesQty_ = static_cast<QtyT>(ordQtys.LeavesQty_ - ordQtys.AfterQty_ + ordQtys.BeforeQty_);
   assert(fon9::signed_cast(ordQtys.LeavesQty_) >= 0);
}

//--------------------------------------------------------------------------//

template <class RequestIniT, class ReportT>
inline auto OmsIsReportSideMatch(const RequestIniT* ini, const ReportT* rpt)
->decltype(ini->Side_, rpt->Side_, bool()) {
   return (rpt->Side_ == f9fmkt_Side_Unknown || rpt->Side_ == ini->Side_);
}
inline bool OmsIsReportSideMatch(...) {
   // 沒有 Side: 報價、詢價;
   return true;
}

/// 檢查 rpt 與 ini 的基本欄位是否一致.
/// - rpt.Side_;
/// - rpt.IvacNo_;
/// - rpt.Symbol_;
template <class RequestIniT, class ReportT>
inline bool OmsIsReportFieldsMatch(const RequestIniT& ini, const ReportT& rpt) {
   if (!OmsIsReportSideMatch(&ini, &rpt))
      return false;
   if (rpt.IvacNo_ != 0 && rpt.IvacNo_ != ini.IvacNo_)
      return false;
   if (!OmsIsSymbolEmpty(rpt.Symbol_) && rpt.Symbol_ != ini.Symbol_)
      return false;
   return true;
}

//--------------------------------------------------------------------------//

/// 判斷 ordPris->LastPriTime_ 是否回過時回報.
/// 如果過時, 則設定 inCoreRunner.OrderRaw().UpdateOrderSt_ = f9fmkt_OrderSt_ReportStale;
template <class OrdPrisT, class ReportT>
inline auto OmsCheckReportChgPriStale(OmsReportRunnerInCore* inCoreRunner, OrdPrisT* ordPris, ReportT* rpt)
->decltype(ordPris->LastPriTime_, bool()) {
   if (!ordPris->LastPriTime_.IsNull() && ordPris->LastPriTime_ >= rpt->ExgTime_)
      inCoreRunner->OrderRaw().UpdateOrderSt_ = f9fmkt_OrderSt_ReportStale;
   return true;
}
inline bool OmsCheckReportChgPriStale(...) {
   return false;
}

/// 根據 rpt.RxKind() 及 rpt.Qty_、rpt.BeforeQty_ 填入 ordQtys;
/// - 若「新單尚未完成」or 回報的改後數量=0 (或刪改失敗), 但有「在途成交」或「遺漏改量」,
///   則要先等補齊後再處理:
///   - 所以此時會設定: f9fmkt_OrderSt_ReportPending 及 ordQtys.BeforeQty_, ordQtys.AfterQty_;
///   - 然後等回報補齊後, 呼叫 ReportT::ProcessPendingReport();
/// - 若有 ordQtys.LastPriTime_ 才會支援 f9fmkt_RxKind_RequestChgPri;
///   - 然後透過 OmsCheckReportChgPriStale() 處理;
/// - retval = false 不正確的 RxKind.
template <class OrdQtysT, class ReportT, class QtyT>
bool OmsAssignQtysFromReportBfAf(OmsReportRunnerInCore& inCoreRunner, OrdQtysT& ordQtys, ReportT& rpt,
                                 QtyT rptBeforeQty, QtyT rptAfterQty) {
   if (fon9_UNLIKELY(rpt.RxKind() == f9fmkt_RxKind_RequestQuery)) {
      // 查詢結果回報, 直接更新(不論新單是否已經成功),
      // 不需要考慮「保留」或 stale(由使用者自行判斷).
      ordQtys.AfterQty_ = ordQtys.BeforeQty_ = rptAfterQty;
      return true;
   }
   // 刪改失敗(NoLeavesQty), 但有「在途成交」或「遺漏改量」(LeavesQty > 0), 應先等補齊後再處理.
   auto  isReportRejected = f9fmkt_TradingRequestSt_IsAnyRejected(rpt.ReportSt());
   if (fon9_UNLIKELY(isReportRejected)) {
      fon9_WARN_DISABLE_SWITCH;
      switch (inCoreRunner.OrderRaw().Order().LastOrderSt()) {
      case f9fmkt_OrderSt_NewQueuing:
      case f9fmkt_OrderSt_NewQueuingAtOther:
         if (rpt.ReportSt() == f9fmkt_TradingRequestSt_QueuingCanceled) {
            // 回報時若收到[新單內部刪單], 應視為[刪單成功]回報.
            isReportRejected = false;
            break;
         }
         /* fall through */
      default:
         assert((rpt.RxKind() == f9fmkt_RxKind_RequestChgCond  // 改條件: 可能直接觸發送出, 然後送出失敗,
                 && rptAfterQty == 0)                          //   此時必定會清除剩餘數量.
                || rptBeforeQty == rptAfterQty); // 失敗的刪改, 數量不會變動, 所以必定 rptBeforeQty == rptAfterQty;
         if (inCoreRunner.OrderRaw().RequestSt_ == f9fmkt_TradingRequestSt_ExchangeNoLeavesQty) {
            if (ordQtys.LeavesQty_ > 0) {
               ordQtys.BeforeQty_ = ordQtys.AfterQty_ = 0;
               inCoreRunner.OrderRaw().UpdateOrderSt_ = f9fmkt_OrderSt_ReportPending;
               return true;
            }
         }
         break;
      }
      fon9_WARN_POP;
   }
   // 回報的改後數量=0, 但有「在途成交」或「遺漏改量」, 應先等補齊後再處理.
   else if (fon9_UNLIKELY(rptAfterQty == 0 && rptBeforeQty < ordQtys.LeavesQty_)) {
__REPORT_PENDING:
      ordQtys.BeforeQty_ = rptBeforeQty;
      ordQtys.AfterQty_ = rptAfterQty;
      inCoreRunner.OrderRaw().UpdateOrderSt_ = f9fmkt_OrderSt_ReportPending;
      return true;
   }
   // 尚未收到新單結果: 使用 Pending 機制, 先將改單結果存於 ordQtys, 等收到新單結果時再處理.
   if (fon9_UNLIKELY(inCoreRunner.OrderRaw().Order().LastOrderSt() < f9fmkt_OrderSt_NewDone)) {
      fon9_WARN_DISABLE_SWITCH;
      switch (inCoreRunner.OrderRaw().Order().LastOrderSt()) {
      case f9fmkt_OrderSt_NewWaitingCond:
      case f9fmkt_OrderSt_NewWaitingCondAtOther:
      case f9fmkt_OrderSt_NewQueuing:
      case f9fmkt_OrderSt_NewQueuingAtOther:
            // 以上這些狀態, 雖然新單尚未完成(例:條件等候中、排隊中), 但可允許透過回報刪改.
         break;
      default:
         goto __REPORT_PENDING;
      }
      fon9_WARN_POP;
   }
   // ----- 刪減回報 ------------------------------------------
   if (rpt.RxKind() == f9fmkt_RxKind_RequestChgQty
       || rpt.RxKind() == f9fmkt_RxKind_RequestDelete) {
      if (fon9_LIKELY(!isReportRejected)) {
         ordQtys.BeforeQty_ = rptBeforeQty;
         ordQtys.AfterQty_ = rptAfterQty;
         OmsRecalcLeavesQtyFromReport(inCoreRunner, ordQtys);
      }
      else {
         // 失敗的刪改(且不是 ExchangeNoLeavesQty: 上面已經判斷過了), 數量不會變動.
         assert(ordQtys.BeforeQty_ == ordQtys.AfterQty_);
      }
      return true;
   }
   // ----- 改價回報 ------------------------------------------
   if (rpt.RxKind() == f9fmkt_RxKind_RequestChgPri) {
      if (fon9_UNLIKELY(isReportRejected))
         return true;
      ordQtys.AfterQty_ = ordQtys.BeforeQty_ = rptAfterQty;
      if (OmsCheckReportChgPriStale(&inCoreRunner, &ordQtys, &rpt))
         return true;
   }
   // [條件單:改條件]回報, 由衍生者自行處理.
   assert(rpt.RxKind() == f9fmkt_RxKind_RequestChgCond && "OmsAssignQtysFromReportBfAf: Unknown report kind.");
   return false;
}

/// 一般「單邊」下單的回報.
/// - rpt.Qty_ = AfterQty;
/// - rpt.BeforeQty_;
template <class OrdQtysT, class ReportT>
bool OmsAssignQtysFromReportDCQ(OmsReportRunnerInCore& inCoreRunner, OrdQtysT& ordQtys, ReportT& rpt) {
   return OmsAssignQtysFromReportBfAf(inCoreRunner, ordQtys, rpt, rpt.BeforeQty_, rpt.Qty_);
}

//--------------------------------------------------------------------------//

template <class OrderRawT, class ReportT>
inline auto OmsAppendReportMessage(const fon9::StrView& msg, OrderRawT* ordraw, ReportT* rpt)
->decltype(rpt->Message_, void()) {
   (void)ordraw;
   rpt->Message_.append(msg);
}
template <class OrderRawT>
inline void OmsAppendReportMessage(const fon9::StrView& msg, OrderRawT* ordraw, ...) {
   ordraw->Message_.append(msg);
}

/// - 只處理「流程結束的回報」
///   - rpt.ReportSt() > f9fmkt_TradingRequestSt_LastRunStep
///   - 才需要「更新 Order 內容」或「暫時保留」.
/// - 刪改查的下單過程(Queuing,Sending: rpt.ReportSt() <= f9fmkt_TradingRequestSt_LastRunStep)的回報:
///   - 因為過程不影響 Order 的內容, 只要更新 RequestSt 即可, 所以這裡 do nothing.
template <class OrderRawT, class ReportT>
void OmsAssignOrderFromReportDCQ(OmsReportRunnerInCore& inCoreRunner, OrderRawT& ordraw, ReportT& rpt) {
   if (fon9_LIKELY(rpt.ReportSt() > f9fmkt_TradingRequestSt_LastRunStep)) {
      if (!OmsAssignQtysFromReportDCQ(inCoreRunner, ordraw, rpt))
         OmsAppendReportMessage("Report: Unknown report kind.", &ordraw, &rpt);
      OmsAssignTimeAndPrisFromReport(ordraw, rpt);
   }
}

//--------------------------------------------------------------------------//

template <size_t arysz, typename CharT, CharT kChFiller>
static inline bool IsPvcIdValid(const fon9::CharAry<arysz, CharT, kChFiller>& v) {
   return !v.empty1st();
}

template <typename T>
static inline typename std::enable_if<std::is_integral<T>::value, bool>::type
IsPvcIdValid(T v) {
   return v != 0;
}

template <class OrderRawT, class ReportT>
inline auto OmsAssignReportOutPvcId(OrderRawT* ordraw, ReportT* rpt)
->decltype(ordraw->OutPvcId_ = rpt->OutPvcId_, void()) {
   if (IsPvcIdValid(rpt->OutPvcId_))
      ordraw->OutPvcId_ = rpt->OutPvcId_;
}
inline void OmsAssignReportOutPvcId(...) {
}

//--------------------------------------------------------------------------//

/// 針對「Request已存在」的回報.
/// - 必須已排除重複回報, 例如: rpt.RunReportInCore_FromOrig_Precheck(checker, origReq);
/// - 您必須提供: OmsAssignOrderFromReportNewOrig(OrderRawT& ordraw, ReportT& rpt);
/// - 執行步驟:
///   - 建立 OmsReportRunnerInCore inCoreRunner{std::move(checker), ordraw};
///   - 根據 rpt.RxKind() 填入 ordraw;
///   - 最終執行 inCoreRunner.UpdateReport(rpt);
template <class OrderRawT, class ReportT>
void OmsRunReportInCore_FromOrig(OmsReportChecker&& checker, OrderRawT& ordraw, ReportT& rpt) {
   OmsReportRunnerInCore inCoreRunner{std::move(checker), ordraw};
   OmsAssignReportOutPvcId(&ordraw, &rpt);
   // ----- 新單回報 ---------------------------------------------
   if (fon9_LIKELY(rpt.RxKind() == f9fmkt_RxKind_RequestNew)) {
      OmsAssignOrderFromReportNewOrig(ordraw, rpt);
      OmsAssignTimeAndPrisFromReport(ordraw, rpt);
   }
   // ----- 刪改查回報 -------------------------------------------
   else {
      OmsAssignOrderFromReportDCQ(inCoreRunner, ordraw, rpt);
   }
   inCoreRunner.UpdateReport(rpt);
}

//--------------------------------------------------------------------------//

/// 委託不存在(或 initiator 不存在)的新單回報.
/// - 設定 ordraw.OrdNo_;
/// - OmsAssignTimeAndPrisFromReport(ordraw, rpt);
/// - inCoreRunner.Resource_.Backend_.FetchSNO(rpt);
/// - inCoreRunner.UpdateReport(rpt);
template <class OrderRawT, class ReportT>
void OmsRunReportInCore_InitiatorNew(OmsReportRunnerInCore&& inCoreRunner, OrderRawT& ordraw, ReportT& rpt) {
   ordraw.OrdNo_ = rpt.OrdNo_;
   OmsAssignTimeAndPrisFromReport(ordraw, rpt);
   OmsAssignReportOutPvcId(&ordraw, &rpt);
   inCoreRunner.Resource_.Backend_.FetchSNO(rpt);
   inCoreRunner.UpdateReport(rpt);
}

/// 已排除重複後的, 刪改查回報.
/// - if (!OmsIsOrdNoEmpty(rpt.OrdNo_)) ordraw.OrdNo_ = rpt.OrdNo_;
/// - OmsAssignOrderFromReportDCQ(inCoreRunner, ordraw, rpt);
/// - inCoreRunner.Resource_.Backend_.FetchSNO(rpt);
/// - inCoreRunner.UpdateReport(rpt);
template <class OrderRawT, class ReportT>
void OmsRunReportInCore_DCQ(OmsReportRunnerInCore&& inCoreRunner, OrderRawT& ordraw, ReportT& rpt) {
   OmsAssignReportOutPvcId(&ordraw, &rpt);
   if (!OmsIsOrdNoEmpty(rpt.OrdNo_))
      ordraw.OrdNo_ = rpt.OrdNo_;
   OmsAssignOrderFromReportDCQ(inCoreRunner, ordraw, rpt);
   inCoreRunner.Resource_.Backend_.FetchSNO(rpt);
   inCoreRunner.UpdateReport(rpt);
}

//--------------------------------------------------------------------------//

/// - assert(order.LastOrderSt() >= f9fmkt_OrderSt_NewDone)
/// - assert(rptraw.UpdateOrderSt_ == f9fmkt_OrderSt_ReportPending)
/// - lastQtys 是否已達到 rptQtys 的等候條件, 依序如下判斷:
///   - rptQtys.AfterQty_ != 0; 只要等新單完成即可, 所以此時必定返回 true;
///   - rptQtys.BeforeQty_ == lastQtys.LeavesQty_; 委託最後異動的剩餘量與 rpt 相符: 已達 rpt 等候條件, 返回 true;
///   - rptQtys.BeforeQty_ != 0; 必定要等候 lastQtys; 所以返回 false;
///   - rptQtys.BeforeQty_ == rptQtys.AfterQty_ == 0: 表示 rpt 為刪改失敗的回報:
///     - 如果 rpt 是 ExchangeNoLeavesQty 的失敗, 返回 false;
///       - 則一定是有遺漏回報
///       - 此時需要等候 lastQtys.LeavesQty==0.
///     - 如果 rpt 不是 ExchangeNoLeavesQty, 返回 true;
///       - 則是一般的刪改失敗(欄位錯誤? TIME IS LATE?...)
///       - 可能永遠等不到 LeavesQty 符合要求! (例如: TIME IS LATE 的失敗)
///       - 此時應立即處理失敗(因為來到這裡時已經 NewDone).
///       - 有可能需要重送(根據 ErrCodeAct 的設定).
template <class OrderQtysT, class OrderRawT>
bool OmsIsReadyForPendingReport(const OrderQtysT& lastQtys, const OrderQtysT& rptQtys, const OrderRawT& rptraw) {
   assert(rptraw.UpdateOrderSt_ == f9fmkt_OrderSt_ReportPending);
   if (rptQtys.AfterQty_ != 0)
      return true;
   if (rptQtys.BeforeQty_ == lastQtys.LeavesQty_)
      return true;
   if (rptQtys.BeforeQty_ != 0)
      return false;
   return (rptraw.RequestSt_ != f9fmkt_TradingRequestSt_ExchangeNoLeavesQty);
}

/// ProcessPendingReport() 的範本(範例), 適用於一般委託(非報價、有改價)
template <class OrderRawT, class RequestIniT, class ReportT>
void OmsProcessPendingReport(const OmsRequestRunnerInCore& prevRunner, const OmsRequestBase& rpt, const ReportT* chkFields) {
   assert(dynamic_cast<const OrderRawT*>(rpt.LastUpdated()) != nullptr);
   const OrderRawT&  rptraw = *static_cast<const OrderRawT*>(rpt.LastUpdated());
   OmsOrder&         order = rpt.LastUpdated()->Order();
   if (order.LastOrderSt() < f9fmkt_OrderSt_NewDone)
      return;
   if (!OmsIsReadyForPendingReport(*static_cast<const OrderRawT*>(order.Tail()), rptraw, rptraw))
      return;
   OmsReportRunnerInCore  inCoreRunner{prevRunner, *order.BeginUpdate(rpt)};
   OrderRawT&             ordraw = inCoreRunner.OrderRawT<OrderRawT>();
   if (chkFields) {
      if (auto ini = dynamic_cast<const RequestIniT*>(order.Initiator())) {
         if (fon9_UNLIKELY(!OmsIsReportFieldsMatch(*ini, *chkFields))) {
            // chk 欄位與新單回報不同.
            ordraw.ErrCode_ = OmsErrCode_FieldNotMatch;
            ordraw.RequestSt_ = f9fmkt_TradingRequestSt_InternalRejected;
            ordraw.Message_.assign("OmsProcessPendingReport: FieldNotMatch.");
            return;
         }
      }
   }
   ordraw.BeforeQty_   = rptraw.BeforeQty_;
   ordraw.AfterQty_    = rptraw.AfterQty_;
   ordraw.LastExgTime_ = rptraw.LastExgTime_;
   if (!rptraw.LastPriTime_.IsNull()) { // rpt 有填價格.
      if (ordraw.LastPriTime_.IsNull()  // ord 目前沒有填價格 || rpt 為新的價格
          || (ordraw.LastPriTime_ < rptraw.LastPriTime_
              && (ordraw.LastPriType_ != rptraw.LastPriType_ || ordraw.LastPri_ != rptraw.LastPri_))) {
         ordraw.LastPri_ = rptraw.LastPri_;
         ordraw.LastPriType_ = rptraw.LastPriType_;
         ordraw.LastPriTime_ = rptraw.LastPriTime_;
      }
   }
   fon9_WARN_DISABLE_SWITCH;
   switch (rpt.RxKind()) {
   case f9fmkt_RxKind_RequestChgPri:
      break;
   case f9fmkt_RxKind_RequestChgQty:
   case f9fmkt_RxKind_RequestDelete:
      OmsRecalcLeavesQtyFromReport(inCoreRunner, ordraw);
      break;
   default: // 「新、查」不會 ReportPending.
      ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_ReportStale;
      ordraw.Message_.assign("OmsProcessPendingReport: Unknown RxKind.");
      break;
   }
   fon9_WARN_POP;
}

} // namespaces
#endif//__f9omstw_OmsReportTools_hpp__
