// \file f9omstws/OmsTwsTradingLineFix.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsTradingLineFix.hpp"
#include "f9omstws/OmsTwsTradingLineMgr.hpp"

#include "fon9/fix/FixBusinessReject.hpp"
#include "fon9/fix/FixAdminDef.hpp"
#include "fon9/fix/FixApDef.hpp"

namespace f9omstw {

static void OnFixReject(const f9fix::FixRecvEvArgs& rxargs, const f9fix::FixOrigArgs& orig) {
   // SessionReject or BusinessReject: 從 orig 取回 req. 然後處理 reject.
   fon9_LOG_INFO("OnFixReject|orig=", orig.MsgStr_, "|rx=", rxargs.MsgStr_);
}
static void OnFixReport(const f9fix::FixRecvEvArgs& rxargs) {
   //
   // 回報處理機制:
   // => 建立一個對應的 req: OmsTwsRequestIni、OmsTwsRequestFilled...
   //    => 新增一個 RequestFactory::MakeRequestForReport();
   //    => 不應該使用 RequestFactory.MakeRequest() 的 ObjSupplier 機制.
   //       因為若在 OmsCore 有找到「原始req」, 則此處建立的 req 會被銷毀, 此為經常狀況,
   //       反而不常發生「沒找到原始req」的情況 => 因此此處建立的 req 通常會被銷毀.
   //       => 若使用 ObjSupplier 則可能會等到 OmsCore 解構時才釋放整塊記憶體 => 會浪費大量記憶體.
   //    => 新刪改: 為了可以在 Reload 時還原「被保留的回報」, 所以這裡應使用 OmsTwsRequestIni 儲存全部欄位.
   //       且需額外增加: BeforeQty, AfterQty.
   //       => 如果來到 InCore 時, 確定 OmsTwsRequestIni 已存在對應的 req, 則 OmsTwsRequestIni 會被刪除.
   //          直接使用 req 處理後續的 OrderRaw.
   //       => 如果來到 InCore 時, 委託存在, 但沒有對應的 req (例如: 外部回報 or 尚未收到的其他 f9oms 下單要求)
   //          => 直接使用 OmsTwsRequestIni 處理委託異動? 或是建立新的 OmsTwsRequestChg 處理?
   //    => 查單: 沒有「保留回報」的需求? 所以只要能確認是同一筆委託即可?
   //    => 成交沒有 BeforeQty, AfterQty, 只有 MatchKey(用來判斷是否重複).
   //       => 但仍需要 IvacNo, Symbol, OType, Side... 來判斷是否為同一筆委託.
   // => 建立 Reporter 保留必要的欄位位置: 使用 StrVref(對應到 ExLog 裡面的 FixMsgStr 的位置).
   //    => 全部儲存在 OmsTwsRequestIni 或 OmsTwsRequestFilled, 不需要保留額外欄位?
   // => 上述資料都準備好之後, 就可以進入 OmsCore 處理回報了.
   //    => OmsRequestBase 的 union 裡面增加一個 Reporter, OmsRequestFlag_Reporter
   //    => 當來到 InCore 時, 將 Reporter 取出, 移除 OmsRequestFlag_Reporter 旗標.
   //    => 然後透過 Reporter 處理回報.
   //
   OmsRequestRunner runner{rxargs.MsgStr_};
   fon9::StrView    fixMsgStr{runner.ExLog_.GetCurrent(), rxargs.MsgStr_.size()};

   // 為了可以在 Reload 時還原「被保留的回報」,
   // 所以這裡應使用 OmsTwsRequestIni 儲存全部欄位.
   //struct TwsFixReporter : public OmsReporter {
   //   fon9_NON_COPY_NON_MOVE(TwsFixReporter);
   //   IvacNo         IvacNo_;
   //   OmsTwsSymbol   StkNo_;
   //   f9fmkt_Side    Side_;
   //   TwsOType       OType_;
   //   OmsTwsQty      BeforeQty_;
   //   OmsTwsQty      AfterQty_;
   //   void RunReport(OmsRequestRunnerInCore&& runner) override {
   //   }
   //};


   //
   // ClOrdID = RxSNO => 取得 request.
   // => 檢查: OrigClOrdID == ReqUID 且 OrdKey 一致.
   //    => 找到 request
   //    => 處理 request 的後續異動.
   // => 否則: 沒找到 request, 使用 OrdKey 找 order
   //    => 若為成交回報:
   //       => 若有找到 order: 建立成交回報.
   //       => 若沒找到 order:
   //          => 保留此次筆成交, 等候 order?
   //          => 拋棄此次筆成交?
   //    => 新刪改查回報:
   //       => 保留此次回報, 等 request?
   //       => 建立 request 處理此次回報?
   //       => 拋棄此次回報?
   //

#if 0
   unsigned stcode = fon9::StrTo(fldText->Value_, 0u);
   switch (stcode) {
   case 4: // 0004:WAIT FOR MATCH: 需要重送(刪改查)?
      isNeedsResend = true;
      break;
   case 5: // 0005:ORDER NOT FOUND
           // 是否需要再送一次刪單? 因為可能新單還在路上(尚未送達交易所), 就已送出刪單要求,
           // 若刪單要求比新單要求早送達交易所, 就會有 "0005:ORDER NOT FOUND" 的錯誤訊息.
      switch (req->OrderSt_) {
      case TOrderSt_NewSending:
      case TOrderSt_OtherNewSending:
         // 超過 n 次就不要再送了.
         isNeedsResend = (++req->ResendCount_ < 5);
         break;
      }
      break;
   }
#endif
}
static void OnFixExecutionReport(const f9fix::FixRecvEvArgs& rxargs) {
   OnFixReport(rxargs);
}
static void OnFixCancelReject(const f9fix::FixRecvEvArgs& rxargs) {
   OnFixReport(rxargs);
}
//--------------------------------------------------------------------------//
TwsTradingLineFixFactory::TwsTradingLineFixFactory(OmsCoreMgr& coreMgr, std::string fixLogPathFmt, Named&& name)
   : base(std::move(fixLogPathFmt), std::move(name))
   , CoreMgr_(coreMgr) {
   this->FixConfig_.Fetch(f9fix_kMSGTYPE_SessionReject).FixRejectHandler_ = &OnFixReject;
   this->FixConfig_.Fetch(f9fix_kMSGTYPE_BusinessReject).FixRejectHandler_ = &OnFixReject;
   this->FixConfig_.Fetch(f9fix_kMSGTYPE_ExecutionReport).FixMsgHandler_ = &OnFixExecutionReport;
   this->FixConfig_.Fetch(f9fix_kMSGTYPE_OrderCancelReject).FixMsgHandler_ = &OnFixCancelReject;
}
fon9::TimeStamp TwsTradingLineFixFactory::GetTDay() {
   if (auto core = this->CoreMgr_.CurrentCore())
      return core->TDay();
   return fon9::TimeStamp{};
}
fon9::io::SessionSP TwsTradingLineFixFactory::CreateTradingLine(
   f9tws::ExgTradingLineMgr&           lineMgr,
   const f9tws::ExgTradingLineFixArgs& args,
   f9fix::IoFixSenderSP                fixSender) {
   return new TwsTradingLineFix(lineMgr, this->FixConfig_, args, std::move(fixSender));
}
//--------------------------------------------------------------------------//
TwsTradingLineFix::TwsTradingLineFix(f9fix::IoFixManager&                mgr,
                                     const f9fix::FixConfig&             fixcfg,
                                     const f9tws::ExgTradingLineFixArgs& lineargs,
                                     f9fix::IoFixSenderSP&&              fixSender)
   : base{mgr, fixcfg, lineargs, std::move(fixSender)}
   , StrSendingBy_{fon9::RevPrintTo<fon9::CharVector>("Sending by ", lineargs.BrkId_, lineargs.SocketId_)} {
}

} // namespaces
