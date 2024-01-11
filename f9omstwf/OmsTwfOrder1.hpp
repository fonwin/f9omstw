// \file f9omstwf/OmsTwfOrder1.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfOrder1_hpp__
#define __f9omstwf_OmsTwfOrder1_hpp__
#include "f9omstwf/OmsTwfOrder0.hpp"
#include "f9omstwf/OmsTwfTypes.hpp"

namespace f9omstw {

using OmsTwfOrder1 = OmsTwfOrder0;

/// 期權一般委託.
struct OmsTwfOrderRawDat1 : public OmsTwfOrderQtys {
   /// 最後價格異動的時間:
   /// - 改價成功
   /// - 刪改查回報若有提供價格, 且價格與 LastPri 不同,
   ///   且「刪改查回報」的「交易所時間」 > LastPriTime_,
   ///   則會異動 LastPri*
   fon9::TimeStamp      LastPriTime_;

   /// Order.LastPri* 及 LastTimeInForce_ 記錄最後成功的價格.
   /// - 新增委託時, 使用 OmsTwfRequestIni;
   /// - 改價成功後, 修改此欄.
   OmsTwfPri            LastPri_;
   f9fmkt_TimeInForce   LastTimeInForce_;
   f9fmkt_PriType       LastPriType_;

   /// 實際送交易所的 PosEff, 應在新單的風控流程填妥.
   OmsTwfPosEff      PosEff_;
   char              padding_____[1];

   /// 複式單第1腳、第2腳的累計成交值、成交量.
   /// 單式單, 這裡不使用, 固定為 0.
   OmsTwfQty         Leg1CumQty_;
   OmsTwfQty         Leg2CumQty_;
   OmsTwfAmt         Leg1CumAmt_;
   OmsTwfAmt         Leg2CumAmt_;

   /// 最後一次收到的成交, 的交易所時間.
   /// 若成交回報有亂序, 則這裡不一定是最後成交時間.
   fon9::TimeStamp   LastFilledTime_;

   /// 全部內容清為 '\0' 或 Null()
   void ClearRawDat() {
      fon9::ForceZeroNonTrivial(this);
      this->LastFilledTime_.AssignNull();
      this->LastPriTime_.AssignNull();
      this->LastPri_.AssignNull();
   }
   /// 先從 prev 複製全部, 然後修改:
   /// - this->BeforeQty_ = this->AfterQty_ = this.LeavesQty_;
   void ContinuePrevUpdate(const OmsTwfOrderRawDat1& prev) {
      memcpy(this, &prev, sizeof(*this));
      this->ContinuePrevQty(prev);
   }
};

class OmsTwfOrderRaw1 : public OmsTwfOrderRaw0, public OmsTwfOrderRawDat1 {
   fon9_NON_COPY_NON_MOVE(OmsTwfOrderRaw1);
   using base = OmsTwfOrderRaw0;
public:
   using base::base;
   OmsTwfOrderRaw1() {
      this->ClearRawDat();
   }

   static void MakeFields(fon9::seed::Fields& flds);

   /// - base::ContinuePrevUpdate(prev);
   /// - OmsTwfOrderRawDat1::ContinuePrevUpdate(*static_cast<const OmsTwfOrderRaw1*>(&prev));
   void ContinuePrevUpdate(const OmsOrderRaw& prev) override;

   void OnOrderReject() override;
   bool IsWorking() const override;
   OmsFilledFlag  HasFilled() const override;
   bool OnBeforeRerun(const OmsReportRunnerInCore& runner) override;
};

} // namespaces
#endif//__f9omstwf_OmsTwfOrder1_hpp__
