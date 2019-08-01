// \file f9omstw/OmsRequestRunner.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestRunner.hpp"
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {

void OmsRequestRunner::RequestAbandon(OmsResource* res, OmsErrCode errCode) {
   this->Request_->Abandon(errCode);
   if (res)
      res->Backend_.Append(*this->Request_, std::move(this->ExLog_));
}
void OmsRequestRunner::RequestAbandon(OmsResource* res, OmsErrCode errCode, std::string reason) {
   this->Request_->Abandon(errCode, std::move(reason));
   if (res)
      res->Backend_.Append(*this->Request_, std::move(this->ExLog_));
}
//--------------------------------------------------------------------------//
bool OmsRequestRunnerInCore::AllocOrdNo(OmsOrdNo reqOrdNo) {
   if (OmsBrk* brk = this->OrderRaw_.Order_->GetBrk(this->Resource_)) {
      if (OmsOrdNoMap* ordNoMap = brk->GetOrdNoMap(*this->OrderRaw_.Request_))
         return ordNoMap->AllocOrdNo(*this, reqOrdNo);
   }
   return OmsOrdNoMap::Reject(*this, OmsErrCode_OrdNoMapNotFound);
}
bool OmsRequestRunnerInCore::AllocOrdNo(OmsOrdTeamGroupId tgId) {
   if (OmsBrk* brk = this->OrderRaw_.Order_->GetBrk(this->Resource_)) {
      if (OmsOrdNoMap* ordNoMap = brk->GetOrdNoMap(*this->OrderRaw_.Request_))
         return ordNoMap->AllocOrdNo(*this, tgId);
   }
   return OmsOrdNoMap::Reject(*this, OmsErrCode_OrdNoMapNotFound);
}
void OmsRequestRunnerInCore::Update(f9fmkt_TradingRequestSt reqst) {
   this->OrderRaw_.RequestSt_ = reqst;
   if (this->OrderRaw_.Request_->RxKind() == f9fmkt_RxKind_RequestNew) {
      if (this->OrderRaw_.OrderSt_ < static_cast<f9fmkt_OrderSt>(reqst)) {
         this->OrderRaw_.OrderSt_ = static_cast<f9fmkt_OrderSt>(reqst);
         if (f9fmkt_TradingRequestSt_IsRejected(reqst))
            this->OrderRaw_.OnOrderReject();
      }
   }
}
//--------------------------------------------------------------------------//
OmsRequestRunStep::~OmsRequestRunStep() {
}
//--------------------------------------------------------------------------//
void OmsReportRunner::ReportAbandon(fon9::StrView reason) {
   this->RunnerSt_ = OmsReportRunnerSt::Abandoned;
   fon9::RevPrint(this->ExLog_, fon9_kCSTR_ROWSPL fon9_kCSTR_CELLSPL,
                  reason, fon9_kCSTR_ROWSPL fon9_kCSTR_CELLSPL);
   this->Report_->RevPrint(this->ExLog_);
   fon9::RevPrint(this->ExLog_, "ReportAbandon:");
   this->Resource_.Backend_.LogAppend(std::move(this->ExLog_));
}
OmsOrdNoMap* OmsReportRunner::GetOrdNoMap() {
   OmsBrk* brk = this->Resource_.Brks_->GetBrkRec(ToStrView(this->Report_->BrkId_));
   if (brk == nullptr) {
      this->ReportAbandon("Bad BrkId");
      return nullptr;
   }
   OmsOrdNoMap* ordnoMap = brk->GetOrdNoMap(*this->Report_);
   if (ordnoMap == nullptr)
      this->ReportAbandon("Bad Market or SessionId");
   return ordnoMap;
}
const OmsRequestBase* OmsReportRunner::SearchOrigRequestId() {
   OmsRequestBase&       rpt = *this->Report_;
   const OmsRxSNO        sno = this->Resource_.ParseRequestId(rpt);
   const OmsRequestBase* origReq = this->Resource_.GetRequest(sno);
   if (fon9_UNLIKELY(origReq == nullptr)) // 無法用 ReqUID 的 RxSNO 取得 request.
      return nullptr;
   // ReqUID 除了 RxSNO, 還包含其他資訊(例:HostId), 所以仍需檢查 ReqUID 是否一致.
   if (fon9_UNLIKELY(origReq->ReqUID_ != rpt.ReqUID_))
      return nullptr;
   // 即使用 ReqUID 取得了 origReq.
   // 但為了以防萬一, 還是需要檢查基本的 key 是否一致.
   // 如果 key 不一致, 則系統 ReqUID 有問題!
   if (fon9_UNLIKELY(origReq->BrkId_ != rpt.BrkId_
                     || origReq->Market() != rpt.Market()
                     || origReq->SessionId() != rpt.SessionId()))
      return nullptr;

   if (const OmsOrderRaw* origLast = origReq->LastUpdated()) {
      const OmsOrdNo  origOrdNo = origLast->Order_->Tail()->OrdNo_;
      if (fon9_LIKELY(origOrdNo == rpt.OrdNo_)) {
         // 最普通的情況(ReqUID、OrdKey都相同):
         // - 一般送單後的回報, 或其他主機編的 ReqUID, 剛好與本機的 RxSNO 對應.
         // - 返回後, 由呼叫端繼續處理.
         return origReq;
      }
      if (!origOrdNo.empty1st()) {
         if (!rpt.OrdNo_.empty1st()) {
            // ReqUID 找到的 orig 與 rpt 都有編委託書號, 但卻不同.
            // => 此處的 orig 不屬於此筆 rpt.
            // => 應是 ReqUID 編者有問題!!
            // -----
            // => 使用 rpt 的委託書號重新尋找 orig?
            return nullptr;
            // => 或拒絕此筆 rpt?
            // this->ReportAbandon("ReqUID has different OrdNo");
            // return nullptr;
         }
         // orig 已編委託書號, 但 rpt 未編委託書號.
         // => rpt 為此筆委託「未編委託書號前」的回報?
         // => 因 rpt 未編委託書號, rpt 必定不是「交易所回報」.
         // => 所以只要簡單的判斷 RequestSt 即可過濾重複回報.
         if (rpt.ReportSt() > origLast->RequestSt_)
            return origReq;
         this->ReportAbandon("Duplicate or Obsolete report(No OrdNo)");
         return nullptr;
      }
      // orig 未編委託書號, 則 rpt 的 OrdKey 應該還沒有對應的 Order.
      OmsOrdNoMap* ordnoMap = this->GetOrdNoMap();
      if (ordnoMap == nullptr)
         return nullptr;
      // orig 尚未編委託書號, rpt 有委託書號, 用 rpt.OrdNo 建立對照:
      // - 如果失敗: 表示 rpt.OrdKey 對應的委託已存在, 但不是 orig, 所以 rpt 為異常回報.
      // - 如果成功: orig.OrdNo = rpt 的「新編」委託書號.
      //            但此時尚未建立新的 OrderRaw, 返回後再處理.
      if (fon9_LIKELY(ordnoMap->EmplaceOrder(rpt.OrdNo_, origLast->Order_))) {
         this->RunnerSt_ = OmsReportRunnerSt::NotReceived;
         return origReq;
      }
      this->ReportAbandon("Bad OrdNo(Order exists)");
      return nullptr;
   }
   // origLast == nullptr
   if (rpt.OrdNo_ != origReq->OrdNo_)
      return nullptr;
   // key(ReqUID、OrdKey) 一致, 但委託沒有異動過?
   // => rpt 是其他「Req、Ord」分開回報的情境(例: 其他 f9oms 來的).
   // => 這種回報情境, 如果 Req 「沒有委託異動」, 則必定是 Abandoned Request.
   //    => 因為「Req 會與第一次 Ord 異動」合併成一筆 rpt.
   //    => 只有 Abandoned Request 才會讓 Req 單獨存在, 而沒有委託異動.
   // => 所以此筆 rpt 必定為重複的 Abandoned Request 回報.
   assert(origReq->IsAbandoned());
   assert(origReq->AbandonErrCode() == rpt.AbandonErrCode());
   this->ReportAbandon("Duplicate abandoned request");
   return nullptr;
}

} // namespaces
