// \file f9omstw/OmsScBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsScBase_hpp__
#define __f9omstw_OmsScBase_hpp__
#include "f9omstw/OmsRequestRunner.hpp"
#include "fon9/ConfigUtils.hpp"

namespace f9omstw {

extern fon9::EnabledYN  gIsScLogAll;

//--------------------------------------------------------------------------//
#define f9oms_SC_LOG(runner, ...)   do {                          \
      if (fon9_UNLIKELY(gIsScLogAll == fon9::EnabledYN::Yes))     \
         fon9::RevPrint(runner.ExLogForReq_, __VA_ARGS__, '\n');  \
   } while(0)

//--------------------------------------------------------------------------//
/// 找不到帳號.
#define OmsErrCode_Sc_IvrNotFound      static_cast<OmsErrCode>(OmsErrCode_FromRisk + 0)

//--------------------------------------------------------------------------//
/// 找不到商品資料: 市價單無法風控.
#define OmsErrCode_Sc_SymbNotFound     static_cast<OmsErrCode>(OmsErrCode_FromRisk + 101)
/// 有商品資料, 但漲跌停價不正確: 市價單無法風控.
#define OmsErrCode_Sc_SymbPriNotFound  static_cast<OmsErrCode>(OmsErrCode_FromRisk + 102)
/// 限價單沒填價格.
#define OmsErrCode_Sc_BadPri           static_cast<OmsErrCode>(OmsErrCode_FromRisk + 103)
/// 超過漲停.
#define OmsErrCode_Sc_OverPriUpLmt     static_cast<OmsErrCode>(OmsErrCode_FromRisk + 104)
/// 超過跌停.
#define OmsErrCode_Sc_OverPriDnLmt     static_cast<OmsErrCode>(OmsErrCode_FromRisk + 105)

/// 檢查商品委託價.
/// - 若為市價單, 則會根據買賣別將漲跌停價填入 ordraw.LastPri_;
/// - OrderRawT 必須提供 LastPriType_ 及 LastPri_;
/// - SymbT 必須提供 SymbId_, PriUpLmt_ 及 PriDnLmt_;
/// - RequestIniT 必須提供 Side_ 及 Symbol_(商品ID);
template <class OrderRawT, class SymbT, class RequestIniT>
inline bool Sc_Symbol(OmsRequestRunnerInCore& runner, OrderRawT& ordraw, const SymbT* symb, const RequestIniT& inireq) {
   if (fon9_UNLIKELY(ordraw.LastPriType_ == f9fmkt_PriType_Market)) {
      if (fon9_UNLIKELY(symb == nullptr)) {
         runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_SymbNotFound, nullptr);
         return false;
      }
      ordraw.LastPri_ = (inireq.Side_ == f9fmkt_Side_Buy ? symb->PriUpLmt_ : symb->PriDnLmt_);
      if (fon9_UNLIKELY(ordraw.LastPri_.IsNullOrZero())) {
         runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_SymbPriNotFound, nullptr);
         return false;
      }
   }
   else if (fon9_UNLIKELY(ordraw.LastPri_.IsNullOrZero())) {
      runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_BadPri, nullptr);
      return false;
   }
   else if (fon9_LIKELY(symb)) {
      if (fon9_UNLIKELY(!symb->PriDnLmt_.IsNullOrZero() && ordraw.LastPri_ < symb->PriDnLmt_)) {
         runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_OverPriDnLmt, nullptr);
         return false;
      }
      if (fon9_UNLIKELY(!symb->PriUpLmt_.IsNullOrZero() && ordraw.LastPri_ > symb->PriUpLmt_)) {
         runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_OverPriUpLmt, nullptr);
         return false;
      }
   }
   else { // TODO: 商品不存在是否允許下單?
      f9oms_SC_LOG(runner, "Symb=", inireq.Symbol_, "|warn=SymbolNotFound|OrdPri=", ordraw.LastPri_);
   }
   if (fon9_UNLIKELY(gIsScLogAll == fon9::EnabledYN::Yes && symb))
      f9oms_SC_LOG(runner, "Symb=", symb->SymbId_,
                   "|UpLmt=", symb->PriUpLmt_, "|DnLmt=", symb->PriDnLmt_, "|OrdPri=", ordraw.LastPri_);
   return true;
}

//--------------------------------------------------------------------------//
/// 庫存不足.
#define OmsErrCode_Sc_BalQty  static_cast<OmsErrCode>(OmsErrCode_FromRisk + 200)

template <class QtyT, class QtyBS>
inline bool Sc_BalQty(OmsRequestRunnerInCore& runner, QtyT bal, QtyT leaves, QtyBS filled, QtyT add) {
   const auto lhs = bal + filled.Get_.Buy_;
   const auto rhs = leaves + filled.Get_.Sell_ + add;
   if (fon9_LIKELY(lhs >= rhs)) {
      f9oms_SC_LOG(runner,
         "Bal=", bal, "|Leaves=", leaves, "|FilledBuy=", filled.Get_.Buy_, "|FilledSell=", filled.Get_.Sell_, "|Add=", add,
         "|Remain=", lhs - rhs);
      return true;
   }
   runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_BalQty, nullptr);
   runner.OrderRaw_.Message_ = fon9::RevPrintTo<fon9::CharVector>(
         "Bal=", bal, "|Leaves=", leaves, "|FilledBuy=", filled.Get_.Buy_, "|FilledSell=", filled.Get_.Sell_, "|Add=", add,
         "|Over=", rhs - lhs);
   return false;
}

//--------------------------------------------------------------------------//
/// 額度不足.
#define OmsErrCode_Sc_LmtAmt   static_cast<OmsErrCode>(OmsErrCode_FromRisk + 300)
#define OmsErrCode_Sc_LmtBuy   static_cast<OmsErrCode>(OmsErrCode_FromRisk + 301)
#define OmsErrCode_Sc_LmtSell  static_cast<OmsErrCode>(OmsErrCode_FromRisk + 302)
#define OmsErrCode_Sc_LmtAddBS static_cast<OmsErrCode>(OmsErrCode_FromRisk + 303)
#define OmsErrCode_Sc_LmtSubBS static_cast<OmsErrCode>(OmsErrCode_FromRisk + 304)

template <class AmtT>
inline bool Sc_LmtAmt(OmsRequestRunnerInCore& runner,
                      fon9::StrView lmtName, OmsErrCode ec,
                      AmtT lmt, AmtT used, AmtT leaves,
                      AmtT filledUse, AmtT filledRtn,
                      AmtT add) {
   if (lmt.IsNull())
      lmt.Assign0();
   const auto lhs = lmt + filledRtn;
   const auto rhs = used + leaves + filledUse + add;
   if (fon9_LIKELY(lmt.GetOrigValue() == 0 || lhs >= rhs)) {
      f9oms_SC_LOG(runner,
         lmtName, '=', lmt, "|Used=", used, "|Leaves=", leaves, "|FilledRtn=", filledRtn, "|FilledUse=", filledUse, "|Add=", add,
         "|Remain=", lhs - rhs);
      return true;
   }
   runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, ec, nullptr);
   runner.OrderRaw_.Message_ = fon9::RevPrintTo<fon9::CharVector>(
         lmtName, '=', lmt, "|Used=", used, "|Leaves=", leaves, "|FilledRtn=", filledRtn, "|FilledUse=", filledUse, "|Add=", add,
         "|Over=", rhs - lhs);
   return false;
}

//--------------------------------------------------------------------------//
/// IsRelatedStk: 利害關係人股票禁止下單.
#define OmsErrCode_Sc_IsRelatedStk  static_cast<OmsErrCode>(OmsErrCode_FromRisk + 800)

} // namespaces
#endif//__f9omstw_OmsScBase_hpp__