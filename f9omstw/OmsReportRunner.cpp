// \file f9omstw/OmsReportRunner.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsReportRunner.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsRequestFactory.hpp"

namespace f9omstw {

inline void OmsLogSplit(fon9::RevBufferList& rbuf) {
   if (rbuf.cfront() == nullptr)
      fon9::RevPrint(rbuf, '\n');
   else
      fon9::RevPrint(rbuf, fon9_kCSTR_ROWSPL ">" fon9_kCSTR_CELLSPL);
}
void OmsReportChecker::ReportAbandon(fon9::StrView reason) {
   assert(this->Report_->RxSNO() == 0);
   this->CheckerSt_ = OmsReportCheckerSt::Abandoned;
   OmsLogSplit(this->ExLog_);
   if ((this->Report_->RxItemFlags() & OmsRequestFlag_ForcePublish) == OmsRequestFlag_ForcePublish) {
      this->Report_->LogAbandon(OmsErrCode_Bad_Report, reason.ToString(), this->Resource_, std::move(this->ExLog_));
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
   OmsMarketRec& mkt = brk->GetMarket(this->Report_->Market());
   OmsOrdNoMap*  ordnoMap = mkt.GetSession(this->Report_->SessionId()).GetOrdNoMap();
   if (fon9_LIKELY(ordnoMap != nullptr))
      return ordnoMap;
   if (this->Report_->SessionId() == f9fmkt_TradingSessionId_RptAutoSessionId
       && !OmsIsOrdNoEmpty(this->Report_->OrdNo_)) {
      // 有些回報沒有提供 SessionId, 在此自動填入 SessionId.
      // 系統必須設定全部的 Session 共用委託書號表:
      // 可參閱: OmsBrkTree::InitializeTwsOrdNoMap(); OmsBrkTree::InitializeTwfOrdNoMap();
      // ===> TODO: 這些回報的額外處理, 應該考慮使用額外機制:
      //            由使用者在進入 OmsCore 後, 執行回報前, 執行一些額外處置.
      if ((ordnoMap = mkt.GetSession(f9fmkt_TradingSessionId_Normal).GetOrdNoMap()) != nullptr) {
         if (auto* order = ordnoMap->GetOrder(this->Report_->OrdNo_)) {
            if (this->Report_->RxKind() == f9fmkt_RxKind_Filled) {
               if (auto* ini = order->Initiator()) {
                  this->Report_->OnSynReport(ini, nullptr);
                  return ordnoMap;
               }
            }
            if (auto* ordraw = order->Head()) {
               this->Report_->SetSessionId(ordraw->SessionId());
               return ordnoMap;
            }
         }
      }
   }
   this->ReportAbandon("Bad Market or SessionId");
   return nullptr;
}
const OmsRequestBase* OmsReportChecker::SearchOrigRequestId() {
   OmsRequestBase& rpt = *this->Report_;
   OmsRxSNO        sno;
   if (OmsParseForceReportReqUID(rpt, sno)) {
      if (auto* retval = this->Resource_.GetRequest(sno)) {
         rpt.ReqUID_ = retval->ReqUID_;
         return retval;
      }
      return nullptr;
   }
   fon9::HostId rptHostId;
   sno = OmsReqUID_Builder::ParseReqUID(rpt, rptHostId);
   if (rptHostId != fon9::LocalHostId_) // 不是本機的 rpt.ReqUID_, 不可使用 sno 取的 origReq;
      return nullptr;
   const OmsRequestBase* origReq = this->Resource_.GetRequest(sno);
   if (fon9_UNLIKELY(origReq == nullptr)) // 無法用 ReqUID 的 RxSNO 取得 request.
      return nullptr;
   // -----
   // ReqUID 除了 RxSNO, 還包含其他資訊(例:HostId), 所以仍需檢查 ReqUID 是否一致.
   // => 若 origReq 為繞進要求, rpt 為交易所回報, 則 (origReq->ReqUID_ != rpt.ReqUID_);
   // if (fon9_UNLIKELY(origReq->ReqUID_ != rpt.ReqUID_)) {
   //    return nullptr;
   // }
   // -----
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
   auto& ordraw = this->OrderRaw();
   if (ordraw.UpdateOrderSt_ == f9fmkt_OrderSt_ReportPending) {
      if (this->IsReportPending_)
         return;
      ordraw.UpdateOrderSt_ = ordraw.Order().LastOrderSt();
   }
   if (ordraw.RequestSt_ == f9fmkt_TradingRequestSt_PartExchangeRejected)
      // 雙邊單須等到完整回報(f9fmkt_TradingRequestSt_ExchangeRejected)時, 才重送.
      return;
   // 在這裡處理 ErrCodeAct: Rerun.
   // - 如果是新單失敗: 在此時 LeavesQty 可能為 0.
   //   在 RerunRequest() 時:
   //   - 恢復 LeavesQty 的數量.
   //   - 必要時設定 OrderSt (NewSending)
   // - 如果在異動結束後才 Rerun, 則 LeavesQty 會從 0 調回 NewQty,
   //   如此會造成委託狀態變化的怪異現象, 所以應在異動結束前 Rerun.
   auto step = ordraw.Request().Creator().RunStep_.get();
   if (step == nullptr)
      return;
   if (ordraw.Request().RxKind() == f9fmkt_RxKind_RequestNew) {
      // 如果是新單要重送, 但有收到「刪單要求」, 則不應再重送.
      if (this->HasDeleteRequest_)
         return;
   }

   // 如果是斷線後的回補回報, 是否還有需要 Rerun 呢?
   // - 此時 TradingLine 可能尚未回補完畢, 尚未進入 ApReady.
   //   => 要先等 TradingLine 進入 ApReady 才能 Rerun?
   // - 斷線後到重新連線回補, 中間可能已做過其他處置.
   // - 有回補功能的連線都會面臨此問題:
   //   - 證券的 FIX Session;
   //   - 期貨的 TMP Session;
   //
   // => 目前就直接處理:
   //    => 如果有其他線路, 就透過其他線路 Rerun.
   //    => 如果沒有線路 (TradingLine重新連線後,正在回補,尚未ApReady)
   //       => 就直接 No ready line. 拒絕吧!

   const auto reqst = ordraw.RequestSt_;
   if (!ordraw.OnBeforeRerun(*this))
      return;
   step->RerunRequest(std::move(*this));
   if (reqst != ordraw.RequestSt_) {
      if (ordraw.RequestSt_ == f9fmkt_TradingRequestSt_Sending && ordraw.Request().RxKind() == f9fmkt_RxKind_RequestNew)
         ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_NewSending;
      fon9::RevPrintAppendTo(ordraw.Message_, "(Re:", this->RequestRunTimes_, ')');
   }
}
void OmsReportRunnerInCore::CalcRequestRunTimes() {
   this->RequestRunTimes_ = 0;
   const OmsOrderRaw* ordraw = this->OrderRaw().Order().Head();
   while (ordraw) {
      if (ordraw->RequestSt_ == f9fmkt_TradingRequestSt_Sending
          && (&ordraw->Request() == &this->OrderRaw().Request())) {
         ++this->RequestRunTimes_;
      }
      if (ordraw->Request().RxKind() == f9fmkt_RxKind_RequestDelete)
         this->HasDeleteRequest_ = true;
      ordraw = ordraw->Next();
   }
}
void OmsReportRunnerInCore::UpdateReportImpl(OmsRequestBase& rpt) {
   auto& ordraw = this->OrderRaw();
   if (OmsIsOrdNoEmpty(ordraw.OrdNo_)) {
      // 在 OmsRequestBase::RunReportInCore_FromOrig_Precheck() 有處理: 委託書號對應.
      // 所以這裡直接填入 [來自回報] 的 [委託書號].
      ordraw.OrdNo_ = rpt.OrdNo_;
   }
   if (&ordraw.Request() != &rpt) {
      if (IsEnumContains(rpt.RequestFlags(), OmsRequestFlag_ReportNeedsLog)) {
         OmsLogSplit(this->ExLogForUpd_);
         rpt.RevPrint(this->ExLogForUpd_);
      }
   }
   if (ordraw.UpdateOrderSt_ == f9fmkt_OrderSt_ReportStale)
      ordraw.Message_.append("(stale)");
   this->Update(ordraw.RequestSt_);
   if (rpt.HasForceInternalFlag()) {
      // 例: rpt is OmsTwfReport3, 此時必定為 rpt.HasForceInternalFlag(),
      // 但: dynamic_cast<OmsRequestTrade*>(&rpt) == nullptr;
      // 因: OmsTwfReport3 繼承自 OmsRequestBase; 不是 OmsRequestTrade;
      ordraw.SetForceInternal();
   }
   else {
      assert(dynamic_cast<OmsRequestTrade*>(&rpt) != nullptr);
      if (static_cast<OmsRequestTrade*>(&rpt)->IsForceInternalRpt())
         ordraw.SetForceInternal();
   }
}
void OmsReportRunnerInCore::UpdateFilled(f9fmkt_OrderSt ordst, const OmsReportFilled& rptFilled) {
   this->UpdateSt(ordst, rptFilled.ReportSt());
   if (rptFilled.IsForceInternalRpt())
      this->OrderRaw().SetForceInternal();
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
   auto& ordraw = runner.OrderRaw();
   if (rpt) { // 一般依序回報.
      ordraw.ErrCode_ = rpt->ErrCode();
      ordraw.RequestSt_ = rpt->ReportSt();
   }
   else { // ProcessPendingReport: 亂序回報後的補正.
      auto rptupd = ordraw.Request().LastUpdated();
      ordraw.ErrCode_ = rptupd->ErrCode_;
      ordraw.RequestSt_ = rptupd->RequestSt_;
   }
   OmsErrCodeActSP act = runner.Resource_.Core_.Owner_->GetErrCodeAct(ordraw.ErrCode_);
   if (!act)
      return nullptr;
   const OmsRequestTrade*  rptReq = dynamic_cast<const OmsRequestTrade*>(&ordraw.Request());
   auto                    iniReq = ordraw.Order().Initiator();
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
      if (!ordraw.CheckErrCodeAct(*act))
         continue;
      if (act->RerunTimes_ > 0) {
         unsigned runTimes = runner.RequestRunTimes();
         if (runTimes == 0) // 沒送過, 不會有 Rerun 的需求.
            continue;
         if (runTimes < act->RerunTimes_ + 1u) {
            if (act->IsAtNewDone_ && !IsNewSending(*rptReq, rpt))
               continue;
            goto __RETURN_ACT;
         }
         if (!act->IsAtNewDone_)
            continue;
      }
      if (act->IsAtNewDone_) {
         if (ordraw.Order().LastOrderSt() >= f9fmkt_OrderSt_NewDone) {
            if (!IsNewSending(*rptReq, rpt))
               continue;
         }
         else {
            if (act->RerunTimes_ <= 0)
               continue;
            // 以上條件都成立, 但需等 NewDone 才 Rerun, 則應將此次異動放到 ReportPending.
            ordraw.UpdateOrderSt_ = f9fmkt_OrderSt_ReportPending;
            runner.IsReportPending_ = true;
         }
      }
   __RETURN_ACT:;
      if (rpt) { // 有需要 ACT 額外處理的 rpt, 則強制將 rpt 寫入 log, 以利追蹤系統行為.
         const_cast<OmsRequestBase*>(rpt)->SetReportNeedsLog();
         if (act->IsResetOkErrCode_) {
            ordraw.ErrCode_ = rpt->GetOkErrCode();
            fon9::RevPrint(runner.ExLogForUpd_, "ResetOkErrCode:Before=", rpt->ErrCode(), "|After=", ordraw.ErrCode_, '\n');
         }
      }
      return act;
   }
   return nullptr;
}
OmsErrCodeActSP OmsReportRunnerInCore::CheckErrCodeAct(OmsReportRunnerInCore& runner, const OmsRequestBase* rpt) {
   if (OmsErrCodeActSP act = GetErrCodeAct(runner, rpt)) {
      auto& ordraw = runner.OrderRaw();
      if (act->ReqSt_)
         ordraw.RequestSt_ = act->ReqSt_;
      if (act->ReErrCode_ != OmsErrCode_MaxV)
         ordraw.ErrCode_ = act->ReErrCode_;
      if (ordraw.Request().RxKind() != f9fmkt_RxKind_RequestQuery)
         return act;
   }
   return nullptr;
}

} // namespaces
