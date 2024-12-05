// \file f9omstw/OmsCxToCore.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCxToCore_hpp__
#define __f9omstw_OmsCxToCore_hpp__
#include "f9omstw/OmsRequestBase.hpp"

namespace f9omstw {

class OmsCxToCore {
   fon9_NON_COPY_NON_MOVE(OmsCxToCore);
   struct PendingReq {
      const OmsRequestTrade*  TxRequest_;
      OmsRequestRunStep*      NextStep_;
      fon9::CharVector        Log_;
      PendingReq(const OmsRequestTrade& txReq, OmsRequestRunStep& nextStep, fon9::StrView logstr)
         : TxRequest_{&txReq}, NextStep_{&nextStep}, Log_{logstr} {
      }
   };
   using PendingReqListImpl = std::vector<PendingReq>;
   using PendingReqList = fon9::MustLock<PendingReqListImpl>;
   PendingReqList       PendingReqList_;
   PendingReqListImpl   ReqListInCore_;

   struct ToCoreReq : public OmsRequestBase {
      fon9_NON_COPY_NON_MOVE(ToCoreReq);
      using base = OmsRequestBase;

   protected:
      void InitializeForReportIn() {
         base::InitializeForReportIn();
         this->SetReportSt(f9fmkt_TradingRequestSt_Sending);
      }
   public:
      ToCoreReq() : base{f9fmkt_RxKind_Unknown} {
         this->InitializeForReportIn();
         // 跟隨 UtwOmsCoreUsrDef 一起死亡, 所以 use_count +1 永遠不會因 intrusive_ptr 而死亡;
         intrusive_ptr_add_ref(this);
      }
      void RunReportInCore(OmsReportChecker&& checker) override;
   };
   ToCoreReq   ToCoreReq_;

public:
   /// reserve:預留空間, 讓條件觸發時, 不用再分配 PendingReqList_ 記憶體, 降低一些延遲.
   OmsCxToCore(unsigned reserve);
   ~OmsCxToCore();

   void AddCondFiredReq(OmsCore& omsCore, const OmsRequestTrade& txReq, OmsRequestRunStep& nextStep, fon9::StrView logstr);
};

} // namespaces
#endif//__f9omstw_OmsCxToCore_hpp__
