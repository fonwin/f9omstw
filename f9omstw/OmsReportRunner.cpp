﻿// \file f9omstw/OmsReportRunner.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsReportRunner.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsRequestFactory.hpp"

namespace f9omstw {

void OmsReportChecker::ReportAbandon(fon9::StrView reason) {
   assert(this->Report_->RxSNO() == 0);
   this->CheckerSt_ = OmsReportCheckerSt::Abandoned;
   fon9::RevPrint(this->ExLog_, fon9_kCSTR_ROWSPL ">" fon9_kCSTR_CELLSPL);
   if ((this->Report_->RxItemFlags() & OmsRequestFlag_ForcePublish) == OmsRequestFlag_ForcePublish) {
      this->Report_->Abandon(OmsErrCode_Bad_Report, reason.ToString());
      this->Resource_.Backend_.LogAppend(*this->Report_, std::move(this->ExLog_));
   }
   else {
      fon9::RevPrint(this->ExLog_, fon9_kCSTR_ROWSPL ">" fon9_kCSTR_CELLSPL, reason);
      this->Report_->RevPrint(this->ExLog_);
      fon9::RevPrint(this->ExLog_, "ReportAbandon:");
      this->Resource_.Backend_.LogAppend(std::move(this->ExLog_));
   }
}
OmsOrdNoMap* OmsReportChecker::GetOrdNoMap() {
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
const OmsRequestBase* OmsReportChecker::SearchOrigRequestId() {
   OmsRequestBase& rpt = *this->Report_;
   OmsRxSNO        sno;
   if (fon9_UNLIKELY(memcmp(rpt.ReqUID_.Chars_, f9omstw_kCSTR_ForceReportReqUID, sizeof(f9omstw_kCSTR_ForceReportReqUID)) == 0)) {
      memcpy(&sno, rpt.ReqUID_.Chars_ + sizeof(f9omstw_kCSTR_ForceReportReqUID), sizeof(sno));
      return this->Resource_.GetRequest(sno);
   }
   sno = this->Resource_.ParseRequestId(rpt);
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
      const OmsOrdNo  origOrdNo = origLast->Order().Tail()->OrdNo_;
      if (fon9_LIKELY(origOrdNo == rpt.OrdNo_)) {
         // 最普通的情況(ReqUID、OrdKey都相同):
         // - 一般送單後的回報, 或其他主機編的 ReqUID, 剛好與本機的 RxSNO 對應.
         // - 返回後, 由呼叫端繼續處理.
         return origReq;
      }
      if (!OmsIsOrdNoEmpty(origOrdNo)) {
         if (!OmsIsOrdNoEmpty(rpt.OrdNo_)) {
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
      if (fon9_LIKELY(ordnoMap->EmplaceOrder(rpt.OrdNo_, &origLast->Order()))) {
         this->CheckerSt_ = OmsReportCheckerSt::NotReceived;
         return origReq;
      }
      this->ReportAbandon("Bad OrdNo(Order exists)");
      return nullptr;
   }
   // origLast == nullptr: 此 origReq 尚未有過任何 OrderRaw 異動
   if (rpt.OrdNo_ != origReq->OrdNo_)
      return nullptr;
   // key(ReqUID、OrdKey) 一致, 但委託沒有異動過?
   // => rpt 是其他「Req、Ord」分開回報的情境(例: 其他 f9oms 來的).
   // => 這種回報情境, 如果 Req 「沒有委託異動」, 則必定是 Abandoned Request.
   //    => 因為「Req 會與第一次 Ord 異動」合併成一筆 rpt.
   //    => 只有 Abandoned Request 才會讓 Req 單獨存在, 而沒有委託異動.
   // => 所以此筆 rpt 必定為重複的 Abandoned Request 回報.
   assert(origReq->IsAbandoned());
   assert(origReq->ErrCode() == rpt.ErrCode());
   this->ReportAbandon("Duplicate abandoned request");
   return nullptr;
}
//--------------------------------------------------------------------------//
OmsReportRunnerInCore::~OmsReportRunnerInCore() {
   if (!this->ErrCodeAct_ || this->ErrCodeAct_->RerunTimes_ <= 0)
      return;
   if (this->OrderRaw_.UpdateOrderSt_ == f9fmkt_OrderSt_ReportPending) {
      if (this->IsReportPending_)
         return;
      this->OrderRaw_.UpdateOrderSt_ = this->OrderRaw_.Order().LastOrderSt();
   }
   if (this->OrderRaw_.RequestSt_ == f9fmkt_TradingRequestSt_PartExchangeRejected)
      // 雙邊單須等到完整回報(f9fmkt_TradingRequestSt_ExchangeRejected)時, 才重送.
      return;
   // 在這裡處理 ErrCodeAct: Rerun.
   // - 如果是新單失敗: 在此時 LeavesQty 可能為 0.
   //   在 RerunRequest() 時:
   //   - 恢復 LeavesQty 的數量.
   //   - 必要時設定 OrderSt (NewSending)
   // - 如果在異動結束後才 Rerun, 則 LeavesQty 會從 0 調回 NewQty,
   //   如此會造成委託狀態變化的怪異現象, 所以應在異動結束前 Rerun.
   auto step = this->OrderRaw_.Request().Creator().RunStep_.get();
   if (step == nullptr)
      return;
   if (this->OrderRaw_.Request().RxKind() == f9fmkt_RxKind_RequestNew) {
      // 如果是新單要重送, 但有收到「刪單要求」, 則不應再重送.
      if (this->HasDeleteRequest_)
         return;
   }

   if (0); // 如果是斷線後的回補回報, 是否還有需要 Rerun 呢?
   // - 此時 TradingLine 可能尚未回補完畢, 尚未進入 ApReady.
   // - 斷線後到重新連線回補, 中間可能已做過其他處置.
   // - 有回補功能的連線都會面臨此問題:
   //   - 證券的 FIX Session;
   //   - 期貨的 TMP Session;

   const auto reqst = this->OrderRaw_.RequestSt_;
   step->RerunRequest(std::move(*this));
   if (reqst != this->OrderRaw_.RequestSt_) {
      fon9::RevPrintAppendTo(this->OrderRaw_.Message_,
                              "(Re:", this->RequestRunTimes_, ')');
   }
}
void OmsReportRunnerInCore::CalcRequestRunTimes() {
   this->RequestRunTimes_ = 0;
   const OmsOrderRaw* ordraw = this->OrderRaw_.Order().Head();
   while (ordraw) {
      if (ordraw->RequestSt_ == f9fmkt_TradingRequestSt_Sending
          && (&ordraw->Request() == &this->OrderRaw_.Request())) {
         ++this->RequestRunTimes_;
      }
      if (ordraw->Request().RxKind() == f9fmkt_RxKind_RequestDelete)
         this->HasDeleteRequest_ = true;
      ordraw = ordraw->Next();
   }
}
void OmsReportRunnerInCore::UpdateReportImpl(OmsRequestBase& rpt) {
   if (&this->OrderRaw_.Request() != &rpt) {
      if (IsEnumContains(rpt.RequestFlags(), OmsRequestFlag_ReportNeedsLog)) {
         fon9::RevPrint(this->ExLogForUpd_, fon9_kCSTR_ROWSPL ">" fon9_kCSTR_CELLSPL);
         rpt.RevPrint(this->ExLogForUpd_);
      }
   }
   if (this->OrderRaw_.UpdateOrderSt_ == f9fmkt_OrderSt_ReportStale)
      this->OrderRaw_.Message_.append("(stale)");
   this->Update(this->OrderRaw_.RequestSt_);
}
//--------------------------------------------------------------------------//
static bool CheckSrc(fon9::StrView cfg, const OmsRequestTrade* req) {
   if (cfg.empty())
      return true;
   if (req == nullptr)
      return false;
   fon9::StrView src = ToStrView(req->Src_);
   if (src.empty())
      return false;
   const size_t   srcsz = src.size();
   const char*    pfound = cfg.begin();
   for (;;) {
      const size_t lsz = static_cast<size_t>(cfg.end() - pfound);
      if (lsz < srcsz)
         return false;
      if (memcmp(pfound, src.begin(), srcsz) == 0) {
         if (lsz == srcsz || pfound[srcsz] == ',')
            return true;
      }
      if ((pfound = static_cast<const char*>(memchr(pfound + 1, ',', lsz - 1))) == nullptr)
         break;
      ++pfound;
   }
   return false;
}
static bool IsNewSending(const OmsRequestTrade& rptReq, const OmsRequestBase* rpt) {
   if (auto bf = rptReq.LastUpdated()) {
      if (bf->UpdateOrderSt_ != f9fmkt_OrderSt_NewSending) {
         if (bf->UpdateOrderSt_ != f9fmkt_OrderSt_ReportPending || rpt != nullptr)
            return false;
         // 從 ProcessPendingReport 來的, 應視為 NewSending.
         assert(bf->Order().LastOrderSt() >= f9fmkt_OrderSt_NewDone);
      }
   }
   return true;
}

OmsErrCodeActSP OmsReportRunnerInCore::GetErrCodeAct(OmsReportRunnerInCore& runner, const OmsRequestBase* rpt) {
   if (rpt) { // 一般依序回報.
      runner.OrderRaw_.ErrCode_ = rpt->ErrCode();
      runner.OrderRaw_.RequestSt_ = rpt->ReportSt();
   }
   else { // ProcessPendingReport: 亂序回報後的補正.
      auto rptupd = runner.OrderRaw_.Request().LastUpdated();
      runner.OrderRaw_.ErrCode_ = rptupd->ErrCode_;
      runner.OrderRaw_.RequestSt_ = rptupd->RequestSt_;
   }
   OmsErrCodeActSP act = runner.Resource_.Core_.Owner_->GetErrCodeAct(runner.OrderRaw_.ErrCode_);
   if (!act)
      return nullptr;
   const OmsRequestTrade*  rptReq = dynamic_cast<const OmsRequestTrade*>(&runner.OrderRaw_.Request());
   auto                    iniReq = runner.OrderRaw_.Order().Initiator();
   fon9::TimeStamp         utcNow = fon9::UtcNow();
   fon9::DayTime           dayNow = fon9::GetDayTime(utcNow + fon9::GetLocalTimeZoneOffset());
   for (; act; act = act->Next_) {
      if (act->NewSending_.GetOrigValue() > 0) {
         if (!rptReq || !iniReq)
            continue;
         if (!IsNewSending(*rptReq, rpt))
             continue;
         if (utcNow - iniReq->LastUpdated()->UpdateTime_ > act->NewSending_)
            continue;
      }
      if (!act->CheckTime(dayNow))
         continue;
      if (!CheckSrc(ToStrView(act->Srcs_), rptReq))
         continue;
      if (!CheckSrc(ToStrView(act->NewSrcs_), iniReq))
         continue;
      if (!runner.OrderRaw_.CheckErrCodeAct(*act))
         continue;
      if (act->RerunTimes_ > 0) {
         unsigned runTimes = runner.RequestRunTimes();
         if (runTimes == 0) // 沒送過, 不會有 Rerun 的需求.
            continue;
         if (runTimes < act->RerunTimes_ + 1u) {
            if (act->IsAtNewDone_ && !IsNewSending(*rptReq, rpt))
               continue;
            return act;
         }
         if (!act->IsAtNewDone_)
            continue;
      }
      if (act->IsAtNewDone_) {
         if (runner.OrderRaw_.Order().LastOrderSt() >= f9fmkt_OrderSt_NewDone) {
            if (!IsNewSending(*rptReq, rpt))
               continue;
         }
         else {
            if (act->RerunTimes_ <= 0)
               continue;
            // 以上條件都成立, 但需等 NewDone 才 Rerun, 則應將此次異動放到 ReportPending.
            runner.OrderRaw_.UpdateOrderSt_ = f9fmkt_OrderSt_ReportPending;
            runner.IsReportPending_ = true;
         }
      }
      return act;
   }
   return nullptr;
}
OmsErrCodeActSP OmsReportRunnerInCore::CheckErrCodeAct(OmsReportRunnerInCore& runner, const OmsRequestBase* rpt) {
   if (OmsErrCodeActSP act = GetErrCodeAct(runner, rpt)) {
      if (act->ReqSt_)
         runner.OrderRaw_.RequestSt_ = act->ReqSt_;
      if (act->ReErrCode_ != OmsErrCode_MaxV)
         runner.OrderRaw_.ErrCode_ = act->ReErrCode_;
      if (runner.OrderRaw_.Request().RxKind() != f9fmkt_RxKind_RequestQuery)
         return act;
   }
   return nullptr;
}

} // namespaces
