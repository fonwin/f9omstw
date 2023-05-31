// \file f9omstws/OmsTwsRequest.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstws_OmsTwsRequest_hpp__
#define __f9omstws_OmsTwsRequest_hpp__
#include "f9omstw/OmsRequestTrade.hpp"
#include "f9omstw/OmsReportFilled.hpp"
#include "f9omstws/OmsTwsTypes.hpp"

namespace f9omstw {

struct OmsTwsRequestIniDat {
   OmsTwsSymbol         Symbol_;
   char                 padding_____[5];

   /// 投資人下單類別: ' '=一般, 'A'=自動化設備, 'D'=DMA, 'I'=Internet, 'V'=Voice, 'P'=API;
   fon9::CharAry<1>     IvacNoFlag_;
   f9fmkt_Side          Side_;
   OmsTwsOType          OType_;
   f9fmkt_TimeInForce   TimeInForce_;
   f9fmkt_PriType       PriType_;
   OmsTwsPri            Pri_;
   OmsTwsQty            Qty_;

   OmsTwsRequestIniDat() {
      fon9::ForceZeroNonTrivial(this);
      this->Pri_.AssignNull();
   }
};

class OmsTwsRequestIni : public OmsRequestIni, public OmsTwsRequestIniDat {
   fon9_NON_COPY_NON_MOVE(OmsTwsRequestIni);
   using base = OmsRequestIni;
public:
   using base::base;
   OmsTwsRequestIni() = default;

   static void MakeFields(fon9::seed::Fields& flds);

   const char* IsIniFieldEqual(const OmsRequestBase& req) const override;

   /// - 若沒填 Market 或 SessionId, 則會設定 scRes.Symb_;
   /// - 如果沒填 SessionId, 則:
   ///   - f9fmkt_PriType_Limit && Pri.IsNull() 使用:  f9fmkt_TradingSessionId_FixedPrice;
   ///   - Qty < scRes.Symb_->ShUnit_ 使用:            f9fmkt_TradingSessionId_OddLot;
   ///   - 其他:                                       f9fmkt_TradingSessionId_Normal;
   const OmsRequestIni* BeforeReq_CheckOrdKey(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) override;
};

class OmsTwsRequestChg : public OmsRequestUpd {
   fon9_NON_COPY_NON_MOVE(OmsTwsRequestChg);
   using base = OmsRequestUpd;
public:
   /// 數量(股數): =0刪單, >0期望改後數量(含成交), <0想要減少的數量.
   using QtyType = fon9::make_signed_t<OmsTwsQty>;
   QtyType        Qty_{0};
   OmsTwsPri      Pri_{OmsTwsPri::Null()};
   f9fmkt_PriType PriType_{};
   char           padding_____[7];

   using base::base;
   OmsTwsRequestChg() = default;

   static void MakeFields(fon9::seed::Fields& flds);

   /// 如果沒填 RequestKind, 則根據 Qty 設定 RequestKind.
   /// 然後返回 base::ValidateInUser();
   bool ValidateInUser(OmsRequestRunner& reqRunner) override;

   void ProcessPendingReport(const OmsRequestRunnerInCore& prevRunner) const override;
   bool PutAskToRemoteMessage(OmsRequestRunnerInCore& runner, fon9::RevBuffer& rbuf) const override;

   OpQueuingRequestResult OpQueuingRequest(fon9::fmkt::TradingLineManager& from,
                                           TradingRequest& queuingRequest) override;
};

/// 二段強迫下單要求
class OmsTwsRequestForce : public f9omstw::OmsRequestUpd {
   fon9_NON_COPY_NON_MOVE(OmsTwsRequestForce);
   using base = f9omstw::OmsRequestUpd;
   f9omstw::OmsOrderRaw* BeforeReqInCore(f9omstw::OmsRequestRunner& runner, f9omstw::OmsResource& res) override;
public:
   f9oms_ScForceFlag ScForceFlag_{};

   using base::base;
   OmsTwsRequestForce() = default;

   static void MakeFields(fon9::seed::Fields& flds);

   /// 將 iniReq 的 Policy 強制取代為強迫要求建立者的 Policy.
   /// 衍生者可透過此決定該委託的權限.
   /// 須注意重啟後的 iniReq 為 nullptr.
   virtual void BeforeReq_ResetIniPolicy(const OmsRequestTrade* iniReq, const OmsRequestPolicy* reqPol);
};

} // namespaces
#endif//__f9omstws_OmsTwsRequest_hpp__
