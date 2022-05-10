// \file f9omstwf/OmsTwfTypes.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfTypes_hpp__
#define __f9omstwf_OmsTwfTypes_hpp__
#include "f9omstw/OmsTypes.hpp"
#include "f9omstw/OmsBase.hpp"
#include "f9twf/ExgTmpTypes.hpp"

namespace f9omstw {

/// 配合 P08 的小數位最大值: 小數最多 9 位.
/// 但也要配合 fmkt 的 Pri = Decimal<int64_t, 8>; 這樣 symb->PriceOrigDiv_ 才能共用;
using OmsTwfPri = fon9::fmkt::Pri;
using OmsTwfAmt = fon9::fmkt::Amt;
using OmsTwfQty = f9twf::TmpQty_t;
using OmsTwfSymbol = f9twf::SymbolId;
using OmsTwfPosEff = f9twf::ExgPosEff;

union OmsTwfQtyBS {
   struct {
      OmsTwfQty   Buy_;
      OmsTwfQty   Sell_;
   }              Get_;
   OmsTwfQty      BS_[2];

   OmsTwfQtyBS() {
      memset(this, 0, sizeof(*this));
   }
};

fon9_MSC_WARN_DISABLE(4582); // constructor is not implicitly called
union OmsTwfAmtBS {
   struct {
      OmsTwfAmt   Buy_;
      OmsTwfAmt   Sell_;
   }              Get_;
   OmsTwfAmt      BS_[2];

   OmsTwfAmtBS() {
      memset(this, 0, sizeof(*this));
   }
};
fon9_MSC_WARN_POP;

/// 可用於累計成交價量, 總成價金, 成交均價 = Amt_ / Qty_;
struct OmsTwfOrderQtys {
   OmsTwfQty      BeforeQty_;
   OmsTwfQty      AfterQty_;
   OmsTwfQty      LeavesQty_;
   OmsTwfQty      CumQty_;
   OmsTwfAmt      CumAmt_;

   void ContinuePrevQty(const OmsTwfOrderQtys& prev) {
      this->BeforeQty_ = this->AfterQty_ = prev.LeavesQty_;
   }
   void OnOrderReject() {
      this->AfterQty_ = this->LeavesQty_ = 0;
   }
};
static_assert(sizeof(OmsTwfOrderQtys) == 16, "pack OmsTwfOrderQtys?");

//--------------------------------------------------------------------------//

enum class OmsReportPriStyle : uint8_t {
   /// 回報的價格尚未處理小數位,
   /// 正確價格 = 回報價 * symb->PriceOrigDiv_;
   NoDecimal,
   /// 回報價格 = 正確價格.
   HasDecimal,
};

/// 若無法取得商品資料, 或 PriceOrigDiv_ == 0: 則返回 -1, 並呼叫 checker.ReportAbandon();
int32_t OmsGetSymbolPriMul(OmsReportChecker& checker, fon9::StrView symbid);
int32_t OmsGetOrderPriMul(OmsOrder& order, OmsReportChecker& checker, fon9::StrView symbid);

/// - retval=0:  表示回報價格不用調整.
/// - retval>0:  回報的正確價格 = 價格 * retval;
/// - retval=-1: 無法取得商品資料, 無法調整價格;
template <class ReportT>
inline int32_t OmsGetReportPriMul(OmsOrder& order, OmsReportChecker& checker, ReportT& rpt) {
   switch (rpt.PriStyle_) {
   case OmsReportPriStyle::HasDecimal:
      return 0;
   case OmsReportPriStyle::NoDecimal:
      break;
   }
   return OmsGetOrderPriMul(order, checker, ToStrView(rpt.Symbol_));
}
template <class ReportT>
inline int32_t OmsGetReportPriMul(OmsReportChecker& checker, ReportT& rpt) {
   switch (rpt.PriStyle_) {
   case OmsReportPriStyle::HasDecimal:
      return 0;
   case OmsReportPriStyle::NoDecimal:
      break;
   }
   return OmsGetSymbolPriMul(checker, ToStrView(rpt.Symbol_));
}

//--------------------------------------------------------------------------//

using OmsTwfContractId = f9twf::ContractId;

/// 期交所定義的交易帳號種類, 用於: 交易人契約部位限制檔(P13/PB3);
enum TwfIvacKind : uint8_t {
   /// 0:自然人/Individual.
   TwfIvacKind_Natural = 0,
   /// 1:法人/Institution/Juristic.
   TwfIvacKind_Legal = 1,
   /// 2:期貨自營商.
   TwfIvacKind_Proprietary = 2,
   /// 3:造市者.
   TwfIvacKind_MarketMaker = 3,
   /// 數量, 用於定義陣列.
   TwfIvacKind_Count = 4,
};

} // namespaces
#endif//__f9omstwf_OmsTwfTypes_hpp__
