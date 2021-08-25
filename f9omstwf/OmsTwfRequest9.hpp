// \file f9omstwf/OmsTwfRequest9.hpp
//
// TaiFex 報價(R09/R39).
//
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfRequest9_hpp__
#define __f9omstwf_OmsTwfRequest9_hpp__
#include "f9omstwf/OmsTwfRequest0.hpp"

namespace f9omstw {

struct OmsTwfRequestIniDat9 {
   OmsTwfPri            BidPri_;
   OmsTwfPri            OfferPri_;
   OmsTwfQty            BidQty_;
   OmsTwfQty            OfferQty_;
   f9fmkt_TimeInForce   TimeInForce_;
   /// 投資人身份碼.
   f9twf::TmpIvacFlag   IvacNoFlag_;
   char                 padding__[2];

   OmsTwfRequestIniDat9() {
      memset(this, 0, sizeof(*this));
      this->BidPri_.AssignNull();
      this->OfferPri_.AssignNull();
   }
};

class OmsTwfRequestIni9 : public OmsTwfRequestIni0, public OmsTwfRequestIniDat9 {
   fon9_NON_COPY_NON_MOVE(OmsTwfRequestIni9);
   using base = OmsTwfRequestIni0;

public:
   template <class... ArgsT>
   OmsTwfRequestIni9(ArgsT&&... args)
      : base(RequestType::Quote, std::forward<ArgsT>(args)...) {
   }

   static void MakeFields(fon9::seed::Fields& flds);
};

class OmsTwfRequestChg9 : public OmsRequestUpd {
   fon9_NON_COPY_NON_MOVE(OmsTwfRequestChg9);
   using base = OmsRequestUpd;
public:
   /// 數量(股數): =0刪單, >0期望改後數量(含成交), <0想要減少的數量.
   using QtyType = fon9::make_signed_t<OmsTwfQty>;
   QtyType     BidQty_{0};
   QtyType     OfferQty_{0};
   char        padding_____[4];
   OmsTwfPri   BidPri_;
   OmsTwfPri   OfferPri_;

   using base::base;
   OmsTwfRequestChg9() = default;

   static void MakeFields(fon9::seed::Fields& flds);

   /// 如果沒填 RequestKind, 則根據 Qty 設定 RequestKind.
   /// 然後返回 base::ValidateInUser();
   bool ValidateInUser(OmsRequestRunner& reqRunner) override;

   /// 匯入尚未處理的刪改回報. (因為之前可能有回報亂序);
   void ProcessPendingReport(OmsResource& res) const;

   OpQueuingRequestResult OpQueuingRequest(fon9::fmkt::TradingLineManager& from,
                                           TradingRequest& queuingRequest) override;
};

} // namespaces
#endif//__f9omstwf_OmsTwfRequest9_hpp__
