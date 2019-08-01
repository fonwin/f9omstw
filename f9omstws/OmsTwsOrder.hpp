// \file f9omstws/OmsTwsOrder.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsOrder_hpp__
#define __f9omstws_OmsTwsOrder_hpp__
#include "f9omstw/OmsOrder.hpp"
#include "f9omstws/OmsTwsRequest.hpp"

namespace f9omstw {

/// - 這裡面的 BeforeQty_, AfterQty_
///   - 不含 CumQty_
///   - 與交易所相同: 直接填入交易所的刪改結果.
///   - 此欄僅提供刪改結果參考.
///     - 可能會因為漏回報, 造成 LeavesQty_ 不等於 AfterQty_;
///       - 但是如果刪改結果是 (BeforeQty_ != 0  &&  AfterQty_ == 0) 或 (刪改失敗:已無剩餘量)
///         則會等回報補齊後才處理.
///     - 可能會因為回報亂序, 造成後來的 AfterQty_ 大於先前的 AfterQty_;
///   - 若為成交回報異動, 則:
///     - CumQty_    = 前一筆的 CumQty_ + 單筆成交量;
///     - BeforeQty_ = 前一筆的 LeavesQty_;
///     - AfterQty_  = LeavesQty_ = BeforeQty_ - 單筆成交量;
struct OmsTwsOrderRawDat {
   /// 最後交易所異動時間(新刪改查,不含成交).
   /// 如果回報有亂序, 則為「最後回報」的交易所時間.
   fon9::DayTime  LastExgTime_;
   OmsTwsQty      BeforeQty_;
   OmsTwsQty      AfterQty_;
   OmsTwsQty      LeavesQty_;

   /// 總成交量.
   OmsTwsQty      CumQty_;
   /// 總成價金, 成交均價 = CumAmt_ / CumQty_;
   OmsTwsAmt      CumAmt_;
   /// 最後交易所成交時間.
   fon9::DayTime  LastFilledTime_;

   /// 實際送交易所的 OType, 應在新單的風控流程填妥.
   f9tws::TwsOType   OType_;
   char              padding_____[2];

   /// 20200323支援改價, 所以 Order 必須記錄最後成功的價格.
   /// - 新增委託時, 使用 OmsTwsRequestIni;
   /// - 改價成功後, 修改此欄.
   /// - 改價不支援修改 TIF, 所以委託欄位不用包含 TIF.
   f9fmkt_PriType LastPriType_;
   OmsTwsPri      LastPri_;
   /// 最後改價成功的時間.
   fon9::DayTime  LastPriTime_;

   /// 全部內容清為 '\0' 或 Null()
   void ClearRawDat() {
      memset(this, 0, sizeof(*this));
      this->LastExgTime_.AssignNull();
      this->LastFilledTime_.AssignNull();
      this->LastPriTime_.AssignNull();
   }
   /// 先從 prev 複製全部, 然後修改:
   /// - this->BeforeQty_ = this->AfterQty_ = this.LeavesQty_;
   void ContinuePrevUpdate(const OmsTwsOrderRawDat& prev) {
      memcpy(this, &prev, sizeof(*this));
      this->BeforeQty_ = this->AfterQty_ = this->LeavesQty_;
   }
};

class OmsTwsOrderRaw : public OmsOrderRaw, public OmsTwsOrderRawDat {
   fon9_NON_COPY_NON_MOVE(OmsTwsOrderRaw);
   using base = OmsOrderRaw;
public:
   using base::base;
   OmsTwsOrderRaw() {
      this->ClearRawDat();
   }

   static void MakeFields(fon9::seed::Fields& flds);

   /// - base::ContinuePrevUpdate();
   /// - OmsTwsOrderRawDat::ContinuePrevUpdate(*static_cast<const OmsTwsOrderRaw*>(this->Prev_));
   void ContinuePrevUpdate() override;

   void OnOrderReject() override;
};

} // namespaces
#endif//__f9omstws_OmsTwsOrder_hpp__
