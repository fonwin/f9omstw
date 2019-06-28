// \file f9omstw/OmsOrderTws.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsOrderTws_hpp__
#define __f9omstw_OmsOrderTws_hpp__
#include "f9omstw/OmsOrder.hpp"
#include "f9omstw/OmsRequestTws.hpp"

namespace f9omstw {

struct OmsOrderTwsRawDat {
   /// 最後交易所異動時間(新刪改查,不含成交).
   fon9::DayTime  LastExgTime_;
   /// 交易所異動前剩餘量, 不含 CumQty;
   OmsTwsQty      BeforeQty_;
   /// 交易所異動後剩餘量, 不含 CumQty;
   OmsTwsQty      AfterQty_;
   /// 本機剩餘量, 不含 CumQty;
   OmsTwsQty      LeavesQty_;

   /// 總成交量.
   OmsTwsQty      CumQty_;
   /// 總成價金, 成交均價 = CumAmt_ / CumQty_;
   OmsTwsAmt      CumAmt_;
   /// 最後交易所成交時間.
   fon9::DayTime  LastFilledTime_;

   /// 實際送交易所的 OType.
   TwsOType       OType_;
   char           padding_____[7];

   /// 全部內容清為 '\0' 或 Null()
   void ClearRawDat() {
      memset(this, 0, sizeof(*this));
      this->LastExgTime_.AssignNull();
      this->LastFilledTime_.AssignNull();
   }
   /// 除了 this->BeforeQty_ = prev.AfterQty_; 之外,
   /// 其餘全部欄位都從 prev 複製而來.
   void ContinuePrevUpdate(const OmsOrderTwsRawDat& prev) {
      memcpy(this, &prev, sizeof(*this));
      this->BeforeQty_ = this->AfterQty_;
   }
};

class OmsOrderTwsRaw : public OmsOrderRaw, public OmsOrderTwsRawDat {
   fon9_NON_COPY_NON_MOVE(OmsOrderTwsRaw);
   using base = OmsOrderRaw;
public:
   using base::base;
   OmsOrderTwsRaw() {
      this->ClearRawDat();
   }

   static void MakeFields(fon9::seed::Fields& flds);

   /// - base::ContinuePrevUpdate();
   /// - OmsOrderTwsRawDat::ContinuePrevUpdate(*static_cast<const OmsOrderTwsRaw*>(this->Prev_));
   void ContinuePrevUpdate() override;
};

} // namespaces
#endif//__f9omstw_OmsOrderTws_hpp__
