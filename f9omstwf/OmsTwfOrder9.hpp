// \file f9omstwf/OmsTwfOrder9.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfOrder9_hpp__
#define __f9omstwf_OmsTwfOrder9_hpp__
#include "f9omstwf/OmsTwfOrder0.hpp"
#include "f9omstwf/OmsTwfTypes.hpp"

namespace f9omstw {

using OmsTwfOrder9 = OmsTwfOrder0;

/// 期權報價委託.
struct OmsTwfOrderRawDat9 {
   OmsTwfOrderQtys   Bid_;
   OmsTwfOrderQtys   Offer_;

   /// 最後一次收到的成交, 的交易所時間.
   /// 若成交回報有亂序, 則這裡不一定是最後成交時間.
   fon9::TimeStamp   LastFilledTime_;

   /// 全部內容清為 '\0' 或 Null()
   void ClearRawDat() {
      memset(this, 0, sizeof(*this));
      this->LastFilledTime_.AssignNull();
   }
   /// 先從 prev 複製全部, 然後修改:
   /// - this->BeforeQty_ = this->AfterQty_ = this.LeavesQty_;
   void ContinuePrevUpdate(const OmsTwfOrderRawDat9& prev) {
      memcpy(this, &prev, sizeof(*this));
      this->Bid_.ContinuePrevQty(prev.Bid_);
      this->Offer_.ContinuePrevQty(prev.Offer_);
   }
};

class OmsTwfOrderRaw9 : public OmsTwfOrderRaw0, public OmsTwfOrderRawDat9 {
   fon9_NON_COPY_NON_MOVE(OmsTwfOrderRaw9);
   using base = OmsTwfOrderRaw0;
public:
   using base::base;
   OmsTwfOrderRaw9() {
      this->ClearRawDat();
   }

   static void MakeFields(fon9::seed::Fields& flds);

   /// - base::ContinuePrevUpdate(prev);
   /// - OmsTwfOrderRawDat9::ContinuePrevUpdate(*static_cast<const OmsTwfOrderRaw9*>(&prev));
   void ContinuePrevUpdate(const OmsOrderRaw& prev) override;

   void OnOrderReject() override;
};

//--------------------------------------------------------------------------//

/// 如果 rpt = ReportPending 且為部分成功, 且 req 上次的異動也是(ReportPending且部分完成)
/// 則合併上次的異動, 因為 OmsOrder::ProcessPendingReport() 只會處理 req 「最後一次」的 ReportPending.
void OmsTwfMergeQuotePartReport(OmsTwfOrderRaw9& ordraw);

} // namespaces
#endif//__f9omstwf_OmsTwfOrder9_hpp__
