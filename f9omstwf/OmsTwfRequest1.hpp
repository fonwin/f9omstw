// \file f9omstwf/OmsTwfRequest1.hpp
//
// TaiFex 一般期權下單要求(R01/R31).
//
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfRequest1_hpp__
#define __f9omstwf_OmsTwfRequest1_hpp__
#include "f9omstwf/OmsTwfRequest0.hpp"

namespace f9omstw {

struct OmsTwfRequestIniDat1 {
   OmsTwfPri            Pri_;
   f9fmkt_PriType       PriType_;
   f9fmkt_TimeInForce   TimeInForce_;
   f9fmkt_Side          Side_;
   OmsTwfPosEff         PosEff_;
   OmsTwfQty            Qty_;
   /// 投資人身份碼.
   f9twf::TmpIvacFlag   IvacNoFlag_;
   char                 padding__[1];

   OmsTwfRequestIniDat1() {
      memset(this, 0, sizeof(*this));
      this->Pri_.AssignNull();
   }
};

class OmsTwfRequestIni1 : public OmsTwfRequestIni0, public OmsTwfRequestIniDat1 {
   fon9_NON_COPY_NON_MOVE(OmsTwfRequestIni1);
   using base = OmsTwfRequestIni0;

public:
   template <class... ArgsT>
   OmsTwfRequestIni1(ArgsT&&... args)
      : base(RequestType::Normal, std::forward<ArgsT>(args)...) {
   }

   static void MakeFields(fon9::seed::Fields& flds);

   const char* IsIniFieldEqual(const OmsRequestBase& req) const override;

   bool IsHalfWm() const {
      return(this->PosEff_ == OmsTwfPosEff::DayTrade && this->Market() == f9fmkt_TradingMarket_TwFUT);
   }
   OmsTwfMrgn AdjWm(OmsTwfMrgn wmAmt) const {
      return this->IsHalfWm() ? (wmAmt / 2) : wmAmt;
   }
};

class OmsTwfRequestChg1 : public OmsRequestUpd {
   fon9_NON_COPY_NON_MOVE(OmsTwfRequestChg1);
   using base = OmsRequestUpd;
public:
   /// 數量(股數): =0刪單, >0期望改後數量(含成交), <0想要減少的數量.
   using QtyType = fon9::make_signed_t<OmsTwfQty>;
   OmsTwfPri            Pri_{OmsTwfPri::Null()};
   f9fmkt_PriType       PriType_{};
   f9fmkt_TimeInForce   TimeInForce_{};
   QtyType              Qty_{0};
   char                 padding_____[4];

   using base::base;
   OmsTwfRequestChg1() = default;

   static void MakeFields(fon9::seed::Fields& flds);

   /// 如果沒填 RequestKind, 則根據 Qty 設定 RequestKind.
   /// 然後返回 base::ValidateInUser();
   bool ValidateInUser(OmsRequestRunner& reqRunner) override;

   /// 匯入尚未處理的刪改回報. (因為之前可能有回報亂序);
   void ProcessPendingReport(OmsResource& res) const;

   OpQueuingRequestResult OpQueuingRequest(fon9::fmkt::TradingLineManager& from,
                                           TradingRequest& queuingRequest);
};

} // namespaces
#endif//__f9omstwf_OmsTwfRequest1_hpp__
