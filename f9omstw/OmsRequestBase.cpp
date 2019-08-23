// \file f9omstw/OmsRequestBase.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestBase.hpp"
#include "f9omstw/OmsReportRunner.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsRequestBase::MakeFieldsImpl(fon9::seed::Fields& flds) {
   flds.Add(fon9_MakeField(OmsRequestBase, RxKind_, "Kind"));
   flds.Add(fon9_MakeField2(OmsRequestBase, Market));
   flds.Add(fon9_MakeField2(OmsRequestBase, SessionId));
   flds.Add(fon9_MakeField2(OmsRequestBase, BrkId));
   flds.Add(fon9_MakeField2(OmsRequestBase, OrdNo));
   flds.Add(fon9_MakeField2(OmsRequestBase, ReqUID));
   flds.Add(fon9_MakeField2(OmsRequestBase, CrTime));
}
void OmsRequestBase::AddFieldsForReport(fon9::seed::Fields& flds) {
   using namespace fon9;
   using namespace fon9::seed;
   flds.Add(FieldSP{new FieldIntHx<underlying_type_t<f9fmkt_TradingRequestSt>>(Named{"ReportSt"}, fon9_OffsetOfRawPointer(OmsRequestBase, ReportSt_))});
   flds.Add(fon9_MakeField(OmsRequestBase, ErrCode_, "ErrCode"));
}
fon9::seed::FieldSP OmsRequestBase::MakeField_RxSNO() {
   return fon9_MakeField2_const(OmsRequestBase, RxSNO);
}
//--------------------------------------------------------------------------//
OmsRequestBase::~OmsRequestBase() {
   if (this->RxItemFlags_ & OmsRequestFlag_Abandon)
      delete this->AbandonReason_;
   //-----------------------------
   else if (auto ordraw = this->LastUpdated()) {
      // 何時才是刪除 Order 的適當時機?
      if (ordraw == ordraw->Order().Tail())
         ordraw->Order().FreeThis();
      // 或是在 ~OrderRaw() { if (this->Order_ && this == this->Order_->Tail())
      //                         this->Order_->FreeThis(); }
      // => 一般而言, OrderRaw 會比 Request 多, 例如:
      //    Request => OrderRaw.Sending => OrderRaw.Accepted;
      // => 所以放在 ~OmsRequestBase() 裡面刪除 Order.
      //    可少判斷幾次.
   }
   //-----------------------------
}
const OmsRequestBase* OmsRequestBase::CastToRequest() const {
   return this;
}
//--------------------------------------------------------------------------//
OmsOrder* OmsRequestBase::SearchOrderByOrdKey(OmsResource& res) const {
   if (const OmsBrk* brk = res.Brks_->GetBrkRec(ToStrView(this->BrkId_))) {
      if (const OmsOrdNoMap* ordNoMap = brk->GetOrdNoMap(*this))
         return ordNoMap->GetOrder(this->OrdNo_);
   }
   return nullptr;
}
OmsOrder* OmsRequestBase::SearchOrderByKey(OmsRxSNO srcSNO, OmsResource& res) {
   if (srcSNO == 0)
      return this->SearchOrderByOrdKey(res);

   const OmsRequestBase* srcReq = res.GetRequest(srcSNO);
   if (srcReq == nullptr)
      return nullptr;
   const OmsOrderRaw* lastUpdated = srcReq->LastUpdated();
   if (lastUpdated == nullptr)
      return nullptr;

   if (this->BrkId_.empty())
      this->BrkId_ = srcReq->BrkId_;
   else if (this->BrkId_ != srcReq->BrkId_)
      return nullptr;

   lastUpdated = lastUpdated->Order().Tail();
   if (this->OrdNo_.empty1st())
      this->OrdNo_ = lastUpdated->OrdNo_;
   else if (this->OrdNo_ != lastUpdated->OrdNo_)
      return nullptr;

   if (this->Market_ == f9fmkt_TradingMarket_Unknown)
      this->Market_ = srcReq->Market_;
   else if (this->Market_ != srcReq->Market_)
      return nullptr;

   if (this->SessionId_ == f9fmkt_TradingSessionId_Unknown)
      this->SessionId_ = srcReq->SessionId_;
   else if (this->SessionId_ != srcReq->SessionId_)
      return nullptr;

   return &lastUpdated->Order();
}
const OmsRequestIni* OmsRequestBase::BeforeReq_GetInitiator(OmsRequestRunner& runner, OmsRxSNO* pIniSNO, OmsResource& res) {
   assert(this->LastUpdated_ == nullptr && this->AbandonReason_ == nullptr);
   if (OmsOrder* order = this->SearchOrderByKey(pIniSNO ? *pIniSNO : 0, res)) {
      if (const OmsRequestIni* iniReq = order->Initiator()) {
         if (pIniSNO && *pIniSNO == 0)
            *pIniSNO = iniReq->RxSNO();
         return iniReq;
      }
      runner.RequestAbandon(&res, OmsErrCode_OrderInitiatorNotFound);
      return nullptr;
   }
   runner.RequestAbandon(&res, OmsErrCode_OrderNotFound);
   return nullptr;
}
//--------------------------------------------------------------------------//
void OmsRequestBase::RunReportInCore(OmsReportChecker&& checker) {
   assert(this == checker.Report_.get() && !"Not support RunReportInCore()");
   checker.ReportAbandon("Not support RunReportInCore");
}
void OmsRequestBase::ProcessPendingReport(OmsResource& res) const {
   (void)res;
   assert(!"Derived must override ProcessPendingReport()");
}
void OmsRequestBase::MakeReportReqUID(fon9::DayTime exgTime, uint32_t beforeQty) {
   if (!this->ReqUID_.empty1st())
      return;
   this->ReqUID_.Chars_[0] = this->Market();
   this->ReqUID_.Chars_[1] = this->SessionId();
   size_t      szBrkId = this->BrkId_.size();
   const char* pBrkIdEnd = this->BrkId_.begin() + szBrkId;
   this->ReqUID_.Chars_[2] = szBrkId >= 2 ? *(pBrkIdEnd - 2) : '-';
   this->ReqUID_.Chars_[3] = szBrkId >= 1 ? *(pBrkIdEnd - 1) : '-';
   memcpy(this->ReqUID_.Chars_ + 4, this->OrdNo_.Chars_, sizeof(this->OrdNo_));
   if (this->RxKind() == f9fmkt_RxKind_RequestNew)
      return;
   // Kind[1] + Time[9(HHMMSSuuu)] + Bf[6(499000)]
   // => ReqUID 相同 => 視為重複
   // => (1) 機率極低 (2) 反正也難以分辨, 哪個才是交易所的最後結果.
   // => 同一個瞬間(相同時間)的多次改價.
   // => 同一個瞬間的刪改失敗(Bf=0).
   this->ReqUID_.Chars_[4 + sizeof(this->OrdNo_)] = this->RxKind();
   char   buf[sizeof(OmsRequestId) * 2];
   char*  pout = buf + sizeof(OmsRequestId);
   memset(pout, 0, sizeof(OmsRequestId));
   pout = fon9::ToStrRev(pout, beforeQty);
   if (exgTime.IsNull())
      *--pout = ':';
   else {
      auto tms = fon9::unsigned_cast(exgTime.ShiftUnit<3>());
      auto sec = (tms / 1000);
      tms = (tms % 1000)
         + ((sec % 60)
            + ((sec / 60) % 60) * 100
            + ((sec / 60 / 60) % 24) * 10000) * 1000;
      pout = fon9::Pic9ToStrRev<9>(pout, tms);
   }
   memcpy(this->ReqUID_.Chars_ + 4 + sizeof(this->OrdNo_) + 1, pout,
          sizeof(this->ReqUID_) - (4 + sizeof(this->OrdNo_) + 1));
}

} // namespaces
