// \file f9omstw/OmsScBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsScBase_hpp__
#define __f9omstw_OmsScBase_hpp__
#include "f9omstw/OmsRequestRunner.hpp"
#include "fon9/fmkt/FmktTools.hpp"
#include "fon9/ConfigUtils.hpp"

namespace f9omstw {

extern fon9::EnabledYN  gIsScLogAll;

//--------------------------------------------------------------------------//
#define f9oms_SC_LOG(runner, ...)   do {                          \
      if (fon9_UNLIKELY(gIsScLogAll == fon9::EnabledYN::Yes))     \
         fon9::RevPrint(runner.ExLogForReq_, __VA_ARGS__, '\n');  \
   } while(0)

//--------------------------------------------------------------------------//
/// 找不到帳號(或帳號未啟用).
#define OmsErrCode_Sc_IvrNotFound            static_cast<OmsErrCode>(OmsErrCode_FromRisk + 0)
/// OType 不正確或不支援.
#define OmsErrCode_Sc_BadOType               static_cast<OmsErrCode>(OmsErrCode_FromRisk + 1)
/// 帳號停用,原因={:Reason:}
#define OmsErrCode_Sc_IvrStop                static_cast<OmsErrCode>(OmsErrCode_FromRisk + 2)
/// 停用子帳,原因={:Reason:}
#define OmsErrCode_Sc_SubacStop              static_cast<OmsErrCode>(OmsErrCode_FromRisk + 3)
/// 選擇權複式單, 不可使用ROD
#define OmsErrCode_Sc_TimeInForce_Opt        static_cast<OmsErrCode>(OmsErrCode_FromRisk + 4)
/// 巿價委託及一定範圍巿價, 不可使用ROD
#define OmsErrCode_Sc_TimeInForce_Mkt        static_cast<OmsErrCode>(OmsErrCode_FromRisk + 5)
/// 價格類別有誤.
#define OmsErrCode_Sc_Bad_PriType            static_cast<OmsErrCode>(OmsErrCode_FromRisk + 6)
/// 選擇權組合式委託(複式單)不可使用一定範圍巿價委託.
#define OmsErrCode_Sc_OptCombo_CannotMWP     static_cast<OmsErrCode>(OmsErrCode_FromRisk + 7)
/// 新單 SessionId 與商品的 SessionId 不符.
#define OmsErrCode_Sc_SessionId_NotMatchSymb static_cast<OmsErrCode>(OmsErrCode_FromRisk + 8)
/// 夜盤不支援當沖.
#define OmsErrCode_Sc_AfterHourNoDayTrade    static_cast<OmsErrCode>(OmsErrCode_FromRisk + 9)
/// 投資人代沖銷成交前,禁止使用市價代沖銷.
#define OmsErrCode_Sc_FoClrCanMkt            static_cast<OmsErrCode>(OmsErrCode_FromRisk + 10)
/// 代沖銷禁止使用 ROD 漲停價.
#define OmsErrCode_Sc_FoClrUpLmtROD          static_cast<OmsErrCode>(OmsErrCode_FromRisk + 11)
/// 代沖銷禁止使用 ROD 跌停價.
#define OmsErrCode_Sc_FoClrDnLmtROD          static_cast<OmsErrCode>(OmsErrCode_FromRisk + 12)
/// 不認識的 PosEff.
#define OmsErrCode_Sc_Bad_PosEff             static_cast<OmsErrCode>(OmsErrCode_FromRisk + 13)
/// 只有「限價 ROD」可改價.
#define OmsErrCode_Sc_OnlyLmtROD_Can_ChgPri  static_cast<OmsErrCode>(OmsErrCode_FromRisk + 14)
/// 已有成交,不可改為FOK.
#define OmsErrCode_Sc_Filled_Cannot_FOK      static_cast<OmsErrCode>(OmsErrCode_FromRisk + 15)
/// 改為「限價」只能使用「ROD」
#define OmsErrCode_Sc_ChgToPriLmt_OnlyROD    static_cast<OmsErrCode>(OmsErrCode_FromRisk + 16)
/// 帳號限制只能平倉.
#define OmsErrCode_Sc_PosEff_OnlyClose       static_cast<OmsErrCode>(OmsErrCode_FromRisk + 17)
/// 使用者沒有代沖銷權限.
#define OmsErrCode_Sc_Usr_Deny_ForceClr      static_cast<OmsErrCode>(OmsErrCode_FromRisk + 18)
/// 帳號禁止代沖銷.
#define OmsErrCode_Sc_Ivr_Deny_ForceClr      static_cast<OmsErrCode>(OmsErrCode_FromRisk + 19)
/// 現貨市場違約，禁止新倉交易
#define OmsErrCode_Sc_StkVio_Deny_Open       static_cast<OmsErrCode>(OmsErrCode_FromRisk + 20)
/// 期貨市場違約，禁止新倉交易
#define OmsErrCode_Sc_FutVio_Deny_Open       static_cast<OmsErrCode>(OmsErrCode_FromRisk + 21)

//--------------------------------------------------------------------------//
/// 找不到商品資料: 市價單無法風控.
#define OmsErrCode_Sc_SymbNotFound           static_cast<OmsErrCode>(OmsErrCode_FromRisk + 101)
/// 有商品資料, 但參考價不存在, 不可交易: [漲跌停價不正確,市價單無法風控] or [限價單,沒有Symb.PriRef,視為不可交易商品]
#define OmsErrCode_Sc_SymbPriNotFound        static_cast<OmsErrCode>(OmsErrCode_FromRisk + 102)
/// 限價單沒填價格.
#define OmsErrCode_Sc_BadPri                 static_cast<OmsErrCode>(OmsErrCode_FromRisk + 103)
/// 超過漲停.
#define OmsErrCode_Sc_OverPriUpLmt           static_cast<OmsErrCode>(OmsErrCode_FromRisk + 104)
/// 超過跌停.
#define OmsErrCode_Sc_OverPriDnLmt           static_cast<OmsErrCode>(OmsErrCode_FromRisk + 105)
/// 定價交易商品不存在(T33): 有商品資料,但定價交易收盤價不正確.
#define OmsErrCode_Sc_SymbPriFixedNotFound   static_cast<OmsErrCode>(OmsErrCode_FromRisk + 106)
/// 價格 Tick Size 有誤.
#define OmsErrCode_Sc_BadPriTickSize         static_cast<OmsErrCode>(OmsErrCode_FromRisk + 107)
/// 商品禁止下單, 使用「DenyReason=」設定原因.
#define OmsErrCode_Sc_SymbDeny               static_cast<OmsErrCode>(OmsErrCode_FromRisk + 108)
/// 原為市價單, 不可改價.
#define OmsErrCode_Sc_MarketCannotChgPri     static_cast<OmsErrCode>(OmsErrCode_FromRisk + 109)
/// 不可改為市價.
#define OmsErrCode_Sc_CannotChgToMarket      static_cast<OmsErrCode>(OmsErrCode_FromRisk + 110)
/// 契約禁止下單, 使用「DenyReason=」設定原因.
#define OmsErrCode_Sc_ContractDeny           static_cast<OmsErrCode>(OmsErrCode_FromRisk + 111)
/// 過期商品, 使用「EndDate=」設定到期日.
#define OmsErrCode_Sc_SymbExpired            static_cast<OmsErrCode>(OmsErrCode_FromRisk + 112)
/// 契約禁止新倉{:Reason:ES:{9=N09 limit}:}
#define OmsErrCode_Sc_ContractOpenDeny       static_cast<OmsErrCode>(OmsErrCode_FromRisk + 113)
/// 期交所N06通知{:Id:}已達多方上限{:Limit:}
#define OmsErrCode_Sc_N06LimitL              static_cast<OmsErrCode>(OmsErrCode_FromRisk + 114)
/// 期交所N06通知{:Id:}已達空方上限{:Limit:}
#define OmsErrCode_Sc_N06LimitS              static_cast<OmsErrCode>(OmsErrCode_FromRisk + 115)
/// 部位限制多方超過{:Over:}口,上限{:Limit:}口
#define OmsErrCode_Sc_OverPsLimitL           static_cast<OmsErrCode>(OmsErrCode_FromRisk + 116)
/// 部位限制空方超過{:Over:}口,上限{:Limit:}口
#define OmsErrCode_Sc_OverPsLimitS           static_cast<OmsErrCode>(OmsErrCode_FromRisk + 117)
/// 下單帳號保證金超過{:Over:},可用{:Limit:},此筆新增{:Add:}
#define OmsErrCode_Sc_OverMrgnSelf           static_cast<OmsErrCode>(OmsErrCode_FromRisk + 120)
/// 母帳保證金超過{:Over:},可用{:Limit:},此筆新增{:Add:}
#define OmsErrCode_Sc_OverMrgnParent         static_cast<OmsErrCode>(OmsErrCode_FromRisk + 121)

/// 檢查商品委託價.
/// - 若為市價單, 則會根據買賣別將漲跌停價填入 ordraw.LastPri_;
/// - OrderRawT 必須提供 LastPriType_ 及 LastPri_;
/// - SymbT 必須提供 SymbId_;
/// - PriRefsT 必須提供 PriUpLmt_ 及 PriDnLmt_;
/// - RequestIniT 必須提供 Side_ 及 Symbol_(商品ID);
template <class OrderRawT, class SymbT, class RequestIniT, class PriRefsT>
inline bool Sc_Symbol_PriLmt(OmsRequestRunnerInCore& runner, OrderRawT& ordraw, const SymbT* symb, const RequestIniT& inireq, const PriRefsT* pris) {
   if (fon9_LIKELY(ordraw.LastPriType_ == f9fmkt_PriType_Market)) {
      if (fon9_UNLIKELY(symb == nullptr)) {
         runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_SymbNotFound, nullptr);
         return false;
      }
      ordraw.LastPri_ = (inireq.Side_ == f9fmkt_Side_Buy ? pris->PriUpLmt_ : pris->PriDnLmt_);
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
      if (fon9_UNLIKELY(!pris->PriDnLmt_.IsNullOrZero() && ordraw.LastPri_ < pris->PriDnLmt_)) {
         runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_OverPriDnLmt, nullptr);
         runner.OrderRaw().Message_ = fon9::RevPrintTo<fon9::CharVector>("DnLmt=", pris->PriDnLmt_);
         return false;
      }
      if (fon9_UNLIKELY(!pris->PriUpLmt_.IsNullOrZero() && ordraw.LastPri_ > pris->PriUpLmt_)) {
         runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_OverPriUpLmt, nullptr);
         runner.OrderRaw().Message_ = fon9::RevPrintTo<fon9::CharVector>("UpLmt=", pris->PriUpLmt_);
         return false;
      }
      if (fon9_UNLIKELY(pris->PriRef_.IsNullOrZero())) {
         runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_SymbPriNotFound, nullptr);
         return false;
      }
      const fon9::fmkt::Pri opri = fon9::fmkt::Pri::Make<ordraw.LastPri_.Scale>(fon9::signed_cast(ordraw.LastPri_.GetOrigValue()));
      const fon9::fmkt::Pri tickSize = fon9::fmkt::FindPriTickSize(symb->LvPriSteps(), opri);
      if (fon9_UNLIKELY(!(opri % tickSize).IsZero())) {
         runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_BadPriTickSize, nullptr);
         return false;
      }
   }
   else { // TODO: 商品不存在是否允許下單?
      f9oms_SC_LOG(runner, "Symb=", inireq.Symbol_, "|warn=SymbolNotFound|OrdPri=", ordraw.LastPri_);
   }
   if (fon9_UNLIKELY(gIsScLogAll == fon9::EnabledYN::Yes && symb))
      f9oms_SC_LOG(runner, "Symb=", symb->SymbId_,
                   "|UpLmt=", pris->PriUpLmt_, "|DnLmt=", pris->PriDnLmt_, "|OrdPri=", ordraw.LastPri_);
   return true;
}

/// 設定「定價交易」委託價.
/// - OrderRawT 必須提供 LastPri_;
/// - SymbT 必須提供 SymbId_ 及 PriFixed_;
template <class OrderRawT, class SymbT>
inline bool Sc_Symbol_PriFixed(OmsRequestRunnerInCore& runner, OrderRawT& ordraw, const SymbT* symb) {
   if (fon9_UNLIKELY(symb == nullptr)) {
      runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_SymbNotFound, nullptr);
      return false;
   }
   ordraw.LastPri_ = symb->PriFixed_;
   if (fon9_UNLIKELY(ordraw.LastPri_.IsNullOrZero())) {
      runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_SymbPriFixedNotFound, nullptr);
      return false;
   }
   if (fon9_UNLIKELY(gIsScLogAll == fon9::EnabledYN::Yes && symb))
      f9oms_SC_LOG(runner, "Symb=", symb->SymbId_, "|PriFixed=", symb->PriFixed_);
   return true;
}

//--------------------------------------------------------------------------//
/// 數量有誤(不可<=0).
#define OmsErrCode_Sc_BadQty                 static_cast<OmsErrCode>(OmsErrCode_FromRisk + 150)
/// 整股、定價: 下單數量必須是整張股數 ShUnit=1000 的整數倍.
#define OmsErrCode_Sc_QtyUnit                static_cast<OmsErrCode>(OmsErrCode_FromRisk + 151)
/// 下單數量超過上限: ShUnit=1000|MaxQty=499000
#define OmsErrCode_Sc_QtyOver                static_cast<OmsErrCode>(OmsErrCode_FromRisk + 152)
/// 市價下單數量超過上限 {:MaxQty:}
#define OmsErrCode_Sc_QtyMarketOver          static_cast<OmsErrCode>(OmsErrCode_FromRisk + 153)
/// 限價下單數量超過上限 {:MaxQty:}
#define OmsErrCode_Sc_QtyLimitOver           static_cast<OmsErrCode>(OmsErrCode_FromRisk + 154)

inline void Sc_Symbol_QtyOver(OmsRequestRunnerInCore& runner, const uint32_t shqty, const uint32_t maxqty) {
   runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_QtyOver, nullptr);
   runner.OrderRaw().Message_ = fon9::RevPrintTo<fon9::CharVector>("ShUnit=", shqty, "|MaxQty=", maxqty);
}
inline void Sc_Symbol_LogShUnit(OmsRequestRunnerInCore& runner, const uint32_t shqty) {
   f9oms_SC_LOG(runner, "ShUnit=", shqty);
}

template <class RequestIniT>
inline bool Sc_Symbol_QtyOK(OmsRequestRunnerInCore& runner, RequestIniT& inireq) {
   if (fon9_UNLIKELY(inireq.Qty_ <= 0)) {
      runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_BadQty, nullptr);
      return false;
   }
   return true;
}

template <class RequestIniT>
inline bool Sc_Symbol_QtyUnit(OmsRequestRunnerInCore& runner, RequestIniT& inireq, const uint32_t shqty) {
   assert(shqty != 0);
   if (fon9_UNLIKELY((inireq.Qty_ % shqty) != 0)) {
      runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, OmsErrCode_Sc_QtyUnit, nullptr);
      runner.OrderRaw().Message_ = fon9::RevPrintTo<fon9::CharVector>("ShUnit=", shqty);
      return false;
   }
   const auto maxqty = shqty * 499;
   if (fon9_UNLIKELY(inireq.Qty_ > maxqty)) {
      Sc_Symbol_QtyOver(runner, shqty, maxqty);
      return false;
   }
   Sc_Symbol_LogShUnit(runner, shqty);
   return true;
}
template <class RequestIniT>
inline bool Sc_Symbol_QtyOddLot(OmsRequestRunnerInCore& runner, RequestIniT& inireq, const uint32_t shqty) {
   assert(shqty != 0);
   if (fon9_UNLIKELY(inireq.Qty_ >= shqty)) {
      Sc_Symbol_QtyOver(runner, shqty, shqty-1);
      return false;
   }
   Sc_Symbol_LogShUnit(runner, shqty);
   return true;
}

//--------------------------------------------------------------------------//
/// 現股庫存不足: Bal=庫存量|Leaves=賣未成交|Filled=賣已成交|Add=此筆新增|Over=超過數量|RFlags=A,R,S
#define OmsErrCode_Sc_BalQtyGn      static_cast<OmsErrCode>(OmsErrCode_FromRisk + 200)
/// 帳號禁止現股當沖: IFlags=R,S
#define OmsErrCode_Sc_DenyIvrGnRvB  static_cast<OmsErrCode>(OmsErrCode_FromRisk + 201)
/// 商品禁止現股當沖.
#define OmsErrCode_Sc_DenySymbGnRvB static_cast<OmsErrCode>(OmsErrCode_FromRisk + 202)
/// 帳號[現股]只能庫存了結.
#define OmsErrCode_Sc_OnlyClearBalGn  static_cast<OmsErrCode>(OmsErrCode_FromRisk + 203)
/// 帳號[信用]只能庫存了結.
#define OmsErrCode_Sc_OnlyClearBalCD  static_cast<OmsErrCode>(OmsErrCode_FromRisk + 204)

/// 現股賣出(OType='0',可現股當沖), 帳號自有可賣數量不足:
/// Self=剩餘可賣量|Add=此筆新增|Over=不足量|Bal=庫存量|Filled=買進可沖成交量|Used=已賣量
#define OmsErrCode_Sc_GnSellQty     static_cast<OmsErrCode>(OmsErrCode_FromRisk + 210)
/// 現股當沖賣出(OType='d',但不可先賣當沖,原因請參考RFlags), 帳號自有可賣數量不足:
/// Self=剩餘可賣量|Add=此筆新增|Over=不足量|Bal=庫存量|Filled=買進可沖成交量|Used=已賣量|RFlags=A,R,S
///   A: 股票現股當沖旗標(X=可先賣 or Y=必須先買 or 「-」=不可現沖)
///   R: 帳號權限: enum TwsIvScSignFlag;
///   S: 帳號文件簽署情況: enum TwsIvScRightFlag;
#define OmsErrCode_Sc_GnRvQty       static_cast<OmsErrCode>(OmsErrCode_FromRisk + 211)
/// 或有券源不足:
/// SeQty=或有券源量|Used=已用量|Self=自有可賣量|Add=此筆新增|Over=不足量
#define OmsErrCode_Sc_BrkSeQty      static_cast<OmsErrCode>(OmsErrCode_FromRisk + 212)
/// 借券賣出(OType=SBL5,SBL6)時, 庫存不足:
/// Bal=庫存量|Leaves=賣未成交|Filled=賣已成交|Add=此筆新增|Over=超過數量
#define OmsErrCode_Sc_BalQtySBL     static_cast<OmsErrCode>(OmsErrCode_FromRisk + 213)
//--------------------------------------------------------------------------//
template <class QtyT>
inline bool Sc_BalQty(OmsRequestRunnerInCore& runner, OmsErrCode ec, QtyT bal, QtyT leaves, QtyT filled, QtyT add) {
   const auto rhs = leaves + filled + add;
   if (fon9_LIKELY(bal >= rhs)) {
      f9oms_SC_LOG(runner,
                   "Bal=", bal, "|Leaves=", leaves, "|Filled=", filled, "|Add=", add,
                   "|Remain=", bal - rhs);
      return true;
   }
   runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, ec, nullptr);
   runner.OrderRaw().Message_ = fon9::RevPrintTo<fon9::CharVector>(
      "Bal=", bal, "|Leaves=", leaves, "|Filled=", filled, "|Add=", add,
      "|Over=", rhs - bal);
   return false;
}

//--------------------------------------------------------------------------//
// 「已用額度」無法單純從「剩餘量、成交量」計算:
// - 還要考慮「當沖互抵、庫存回補」。
// - 現股當沖為例: 可參考 https://docs.google.com/document/d/1Xpl3GMlECcQ-M2oav8lIfl43_p2uzuHhKbJKyFC9PyA/edit#heading=h.d46f7eb2xzq9
// - 不應單純的區分「剩餘未成交額度」、「已成交額度」...
// - 也不存在單純的「成交返還」。
// - 所以使用「額度上限」、「已用額度」、「此筆新增」檢查就足夠了。
// - 重點是「已用額度」的異動計算。

/// 整戶額度不足.
#define OmsErrCode_Sc_LmtAmt   static_cast<OmsErrCode>(OmsErrCode_FromRisk + 300)
// 融資額度不足: 超過整戶融資額度上限. 超過單股融資上限.
// 融券額度不足: 超過整戶融券額度上限. 超過單股融券上限.
// ...

template <class AmtT>
inline bool Sc_LmtAmt(OmsRequestRunnerInCore& runner, fon9::StrView lmtName, OmsErrCode ec,
                      AmtT lmt, AmtT used, AmtT add) {
   if (lmt.IsNull())
      lmt.Assign0();
   const auto over = (used + add) - lmt;
   if (fon9_LIKELY(lmt.GetOrigValue() == 0 || fon9::signed_cast(over.GetOrigValue()) <= 0)) {
      f9oms_SC_LOG(runner, lmtName, '=', lmt, "|Used=", used, "|Add=", add, "|Remain=",
                   lmt.GetOrigValue() == 0 ? lmt : (AmtT{} - over));
      return true;
   }
   runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, ec, nullptr);
   runner.OrderRaw().Message_ = fon9::RevPrintTo<fon9::CharVector>(
         lmtName, '=', lmt, "|Used=", used, "|Add=", add, "|Over=", over);
   return false;
}

//--------------------------------------------------------------------------//
/// 資庫存不足: Bal=庫存量|Leaves=賣未成交|Filled=賣已成交|Add=此筆新增|Over=超過數量
#define OmsErrCode_Sc_BalQtyCr      static_cast<OmsErrCode>(OmsErrCode_FromRisk + 400)
/// 券庫存不足: Bal=庫存量|Leaves=買未成交|Filled=買已成交|Add=此筆新增|Over=超過數量
#define OmsErrCode_Sc_BalQtyDb      static_cast<OmsErrCode>(OmsErrCode_FromRisk + 500)

//--------------------------------------------------------------------------//
/// 可平倉部位不足{:Over:}
#define OmsErrCode_Sc_OverPosClose  static_cast<OmsErrCode>(OmsErrCode_FromRisk + 600)

//--------------------------------------------------------------------------//
/// IsRelatedStk: 利害關係人股票禁止下單.
#define OmsErrCode_Sc_IsRelatedStk  static_cast<OmsErrCode>(OmsErrCode_FromRisk + 800)

//--------------------------------------------------------------------------//
/// 11103..11107 需對應 10103..10107 的錯誤碼.
#define OmsErrCode_CondSc_Adj                    1000
#define OmsErrCode_CondSc_BadCondOp              static_cast<OmsErrCode>(OmsErrCode_FromRisk + OmsErrCode_CondSc_Adj + 999)
#define OmsErrCode_CondSc_BadPri                 static_cast<OmsErrCode>(OmsErrCode_CondSc_Adj + OmsErrCode_Sc_BadPri)
#define OmsErrCode_CondSc_OverPriUpLmt           static_cast<OmsErrCode>(OmsErrCode_CondSc_Adj + OmsErrCode_Sc_OverPriUpLmt)
#define OmsErrCode_CondSc_OverPriDnLmt           static_cast<OmsErrCode>(OmsErrCode_CondSc_Adj + OmsErrCode_Sc_OverPriDnLmt)
#define OmsErrCode_CondSc_BadPriTickSize         static_cast<OmsErrCode>(OmsErrCode_CondSc_Adj + OmsErrCode_Sc_BadPriTickSize)

/// Symb 無法取得所需要的行情價: msSymb.GetMdBSEv() 或 mdSymb.GetMdLastPriceEv();
/// 通常為系統設計錯誤.
#define OmsErrCode_CondSc_MdSymb                 static_cast<OmsErrCode>(OmsErrCode_CondSc_Adj + OmsErrCode_Sc_SymbPriNotFound)

//--------------------------------------------------------------------------//
static inline OmsErrCode CheckPriTickSize(fon9::fmkt::Pri curpri, OmsOrder& order) {
   return fon9::fmkt::CheckPriTickSize(order.ScResource().Symb_->LvPriSteps(), curpri)
      ? OmsErrCode_Null : OmsErrCode_Sc_BadPriTickSize;
}

extern OmsErrCode CheckLmtPri(fon9::fmkt::Pri curpri, fon9::fmkt::Pri upLmt, fon9::fmkt::Pri dnLmt, fon9::RevBuffer& rbuf);

/// 根據 execPriSel 及 priTicksAway 取得下單價.
/// 若 execPriSel == f9fmkt_ExecPriSel_Unknown, 則應直接使用 [固定價], 與行情無關, 此時 out 不變.
/// 可能返回 OmsErrCode_CondSc_MdSymb; 通常為設計有誤.
extern OmsErrCode GetExecPri(OmsSymb& mdSymb, f9fmkt_ExecPriSel execPriSel, int8_t priTicksAway, fon9::fmkt::Pri& out);
static inline bool CheckExecPri(OmsRequestRunnerInCore& runner,
                                OmsSymb& mdSymb, f9fmkt_ExecPriSel execPriSel, int8_t priTicksAway, fon9::fmkt::Pri& out) {
   OmsErrCode errCode = GetExecPri(mdSymb, execPriSel, priTicksAway, out);
   if (fon9_LIKELY(errCode == OmsErrCode_Null))
      return true;
   runner.Reject(f9fmkt_TradingRequestSt_CheckingRejected, errCode, nullptr);
   return false;
}

/// 檢查 reqPri 的檔位, 及漲跌停範圍;
extern bool CheckPriTickSizeAndLmt(OmsRequestRunnerInCore& runner, OmsSymb& mdSymb, fon9::fmkt::Pri reqPri);

} // namespaces
#endif//__f9omstw_OmsScBase_hpp__
