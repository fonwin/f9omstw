// \file f9omstw/f9omstwUT.cpp
//
// f9omstw 的單元測試.
//
// \author fonwinz@gmail.com
/*
# ----------------------------------------
# 建立下單要求
#  req ReqName Field=Value|...
#  若沒有「BrkId=」, 則 BrkId 會填入預設值.
#
# 建立回報
#  rpt RptName  Kind:QtyStyle:UID  ReportSt=hh|...other fields...
#      Kind: N=New, D=Delete, C=ChgQty, P=ChgPri, f=Filled...
#      QtyStyle: 股票數量類型:  0=BySessionId, 1=BoardLot(張數), 2=OddLot(股數)
#      UID: 針對哪一筆下單要求的回報(ReqUID), 若為「-」則表示針對上一筆(BrkId,Market,SessionId,OrdNo,ReqUID)
#      範例:
#         rpt TwsRpt  N:2:-  ReportSt=f2|Qty=3000|BeforeQty=3000
#         rpt TwsRpt  N:2    ReportSt=f2|Market=T|SessionId=N|OrdNo=A0001|Qty=3000|BeforeQty=3000
#         rpt TwsFil  f:2:-  MatchKey=1|Qty=1000|Pri=49|Time=0|IvacNo=10|Symbol=1101|Side=B
#
# 建立事件
#  ev EvName Field=Value|...
#
# ----------------------------------------
# 檢查最後回報內容.
# 可以沒有 {ErrCode:Message}, 表示不檢查 ErrCode 及 Message;
# 若沒提供 Message, 則不檢查 Message;
#  chkr {ErrCode:Message}  Field=Value|...
#
# 檢查最後回報後的委託內容
# 針對指定req的回報, rpt不會有對應的ord, 此時應使用 chko {BrkId-Market-Session-OrdNo} Field=Value|...
#  chko Field=Value|...
#
# 檢查指定委託書號的委託內容
#  chko {BrkId-Market-Session-OrdNo} Field=Value|...
#
# 檢查 seed 內容是否符合預期, 注意: 要檢查的欄位, 緊接著「chks,」之後, 且用「,」分隔多個欄位.
#  chks,Field=Value,...   SeedName
#
# ----------------------------------------
# 關閉後, 重新載入 OmsCore, 用來檢查程式重新啟動後, 風控計算是否正確.
# 重啟時, 會保留前一次的 [商品基本資料]、[帳號基本資料]、[庫存] 相關設定.
#  reload
#
# ----------------------------------------
# 印出正確&錯誤的數量
#  pi
# 錯誤的數量及發生錯誤的行號
#  pe
#
# ----------------------------------------
*/
#define _CRT_SECURE_NO_WARNINGS
#include "f9omstw/f9omstwUT.hpp"
#include "f9omstw/OmsCoreByThread.hpp"
#include "f9omstw/OmsScBase.hpp"       // gIsScLogAll;
#include "f9omstws/OmsTwsReport.hpp"   // QtyStyle;
#include "f9omstws/OmsTwsFilled.hpp"

#include "fon9/TimedFileName.hpp"
#include "fon9/FilePath.hpp"
#include "fon9/LogFile.hpp"
#include "fon9/ConsoleIO.h"
#include "fon9/CtrlBreakHandler.h"

namespace f9omstw {

fon9::StrView  gBrkId{"8610"};
unsigned       gBrkCount = 1;
unsigned       gLineNo = 0;
std::string    gLogPathFmt;

std::vector<unsigned> gErrLineNo;
unsigned              gPassedCount;

//--------------------------------------------------------------------------//
fon9::RevBufferList CompareFieldValues(fon9::StrView fldvals, const fon9::seed::Fields& flds, const fon9::seed::RawRd& rd, char chSpl) {
   fon9::RevBufferList  rbuf{128};
   while (!fldvals.empty()) {
      fon9::StrView val = fon9::SbrFetchNoTrim(fldvals, chSpl);
      fon9::StrView fldName = fon9::StrFetchTrim(val, '=');
      auto    fld = flds.Get(fldName);
      if (fld == nullptr)
         fon9::RevPrint(rbuf, "fieldName=", fldName, "|err=field not found\n");
      else {
         fon9::RevBufferFixedSize<1024> out;
         fld->CellRevPrint(rd, nullptr, out);
         fon9::StrView outstr = ToStrView(out);
         val = fon9::StrNoTrimRemoveQuotes(val);
         if (val != outstr)
            fon9::RevPrint(rbuf, "[ERROR] ", fldName, '=', outstr, "|expect=", val, '\n');
      }
   }
   if (rbuf.cfront() == nullptr) {
      fon9::RevPrint(rbuf, "[OK   ] All fields match.\n");
      ++gPassedCount;
   }
   else {
      gErrLineNo.push_back(gLineNo);
   }
   fon9::RevPrint(rbuf, "@Ln=", gLineNo, '\n');
   return rbuf;// gcc 13 不喜歡 return std::move(rbuf); 會有警告;
}

//--------------------------------------------------------------------------//
void OmsRequestStep_Send::RunRequest(OmsRequestRunnerInCore&& runner) {
   runner.Update(f9fmkt_TradingRequestSt_Sending, "Sending");
}

//--------------------------------------------------------------------------//
class TicketRunnerCompare : public fon9::seed::TicketRunnerRead {
   fon9_NON_COPY_NON_MOVE(TicketRunnerCompare);
   using base = fon9::seed::TicketRunnerRead;
protected:
   void OnReadOp(const fon9::seed::SeedOpResult& res, const fon9::seed::RawRd* rd) override {
      if (!rd)
         return this->OnError(res.OpResult_);
      fon9::RevBufferList  rbuf = CompareFieldValues(&this->CompareFieldValues_, res.Tab_->Fields_, *rd, ',');
      this->Visitor_->OnTicketRunnerDone(*this, fon9::DcQueueList{rbuf.MoveOut()});
   }
public:
   const std::string CompareFieldValues_;
   TicketRunnerCompare(fon9::seed::SeedVisitor& visitor, fon9::StrView seed, fon9::StrView compareFieldValues)
      : base(visitor, seed)
      , CompareFieldValues_{compareFieldValues.ToString()} {
   }
};

//--------------------------------------------------------------------------//
void OmsUtConsoleSeedSession::OnAuthEventInLocking(State st, fon9::DcQueue&& msg) {
   (void)st;
   this->WriteToConsole(std::move(msg));
   this->Wakeup();
}
void OmsUtConsoleSeedSession::OnRequestError(const fon9::seed::TicketRunner&, fon9::DcQueue&& errmsg) {
   this->WriteToConsole(std::move(errmsg));
   this->Wakeup();
}
void OmsUtConsoleSeedSession::OnRequestDone(const fon9::seed::TicketRunner&, fon9::DcQueue&& extmsg) {
   if (!extmsg.empty())
      this->WriteToConsole(std::move(extmsg));
   this->Wakeup();
}
void OmsUtConsoleSeedSession::WriteToConsole(fon9::DcQueue&& errmsg) {
   fon9::DeviceOutputBlock(std::move(errmsg), [](const void* buf, size_t sz) {
      fwrite(buf, sz, 1, stdout);
      return fon9::Outcome<size_t>{sz};
   });
   fputs("\n", stdout);
}
uint16_t OmsUtConsoleSeedSession::GetDefaultGridViewRowCount() {
   fon9_winsize wsz;
   fon9_GetConsoleSize(&wsz);
   return static_cast<uint16_t>(wsz.ws_row);
}
OmsUtConsoleSeedSession::RequestSP OmsUtConsoleSeedSession::OnUnknownCommand(fon9::seed::SeedFairy::Request& req) {
   if (req.Command_ == "chks")
      return new TicketRunnerCompare{*this, req.SeedName_, req.CommandArgs_};
   return base::OnUnknownCommand(req);
}
void OmsUtConsoleSeedSession::Wakeup() {
   this->Waiter_.ForceWakeUp();
}
void OmsUtConsoleSeedSession::Wait() {
   this->Waiter_.Wait();
   this->Waiter_.AddCounter(1);
}

//--------------------------------------------------------------------------//
fon9::RevBufferList RevPrintFieldValues(const fon9::seed::Fields& flds, const fon9::seed::RawRd& rd) {
   fon9::RevBufferList  rbuf{128};
   size_t               ifld = flds.size();
   while (auto fld = flds.Get(--ifld)) {
      fon9::RevPutChar(rbuf, '\n');
      fld->CellRevPrint(rd, nullptr, rbuf);
      fon9::RevPrint(rbuf, fld->Name_, '=');
   }
   return rbuf;
}

bool PutFields(const char* facName, const fon9::seed::Fields& flds, const fon9::seed::RawWr& wr, fon9::StrView args) {
   fon9::StrView tag, value;
   while (fon9::StrFetchTagValue(args, tag, value)) {
      if (auto fld = flds.Get(tag))
         fld->StrToCell(wr, value);
      else {
         *const_cast<char*>(tag.end()) = '\0';
         printf("Field:%s not found, in factory:%s\n", tag.begin(), facName);
         return false;
      }
   }
   return true;
}

//--------------------------------------------------------------------------//
OmsCoreUT::OmsCoreUT(OmsCoreMgrSP coreMgr) : CoreMgr_{std::move(coreMgr)} {
}
OmsCoreUT::~OmsCoreUT() {
   this->ReqLast_.reset();
   this->CoreMgr_->OnParentSeedClear();
}
void OmsCoreUT::RestoreSymb(OmsSymb& nsymb, const OmsSymb& osymb) {
   *static_cast<fon9::fmkt::SymbTwaBase*>(&nsymb) = osymb;
}
void OmsCoreUT::RestoreSubac(OmsSubac& nsubac, const OmsSubac& osubac) {
   (void)nsubac; (void)osubac;
}
void OmsCoreUT::RestoreIvac(OmsIvac& nivac, const OmsIvac& oivac) {
   nivac.LgOut_ = oivac.LgOut_;
   for (auto& isubac : oivac.Subacs()) {
      if (auto* nsubac = nivac.FetchSubac(ToStrView(isubac->SubacNo_)))
         this->RestoreSubac(*nsubac, *isubac);
   }
}
void OmsCoreUT::RestoreBrk(OmsBrk& nbrk, const OmsBrk& obrk) {
   IvacNC ivacNC;
   const OmsIvacSP* ppIvac = obrk.GetFirstIvac(ivacNC);
   while (ppIvac) {
      if (auto* oivac = static_cast<const OmsIvac*>(ppIvac->get())) {
         if (auto* nivac = nbrk.FetchIvac(oivac->IvacNo_))
            this->RestoreIvac(*nivac, *oivac);
      }
      ppIvac = obrk.GetNextIvac(ivacNC);
   }
}
void OmsCoreUT::RestoreCoreTables(OmsResource& res) {
   OmsSymbTree* curSymbs = res.Symbs_.get();
   OmsBrkTree*  curBrks = res.Brks_.get();
   if (this->PrvSymbs_) {
      auto lkPrv = this->PrvSymbs_->SymbMap_.Lock();
      auto lkCur = curSymbs->SymbMap_.Lock();
      for (auto& isymb : *lkPrv) {
         if (OmsSymb* nsymb = static_cast<OmsSymb*>(curSymbs->FetchSymb(lkCur, isymb.first).get()))
            this->RestoreSymb(*nsymb, *static_cast<const OmsSymb*>(isymb.second.get()));
      }
   }
   if (this->PrvBrks_) {
      unsigned brkidx = 0;
      while (const auto* obrk = this->PrvBrks_->GetBrkRec(brkidx++)) {
         if (auto* nbrk = curBrks->GetBrkRec(ToStrView(obrk->BrkId_)))
            this->RestoreBrk(*nbrk, *obrk);
      }
   }
   this->PrvSymbs_.reset(curSymbs);
   this->PrvBrks_.reset(curBrks);
}
bool OmsCoreUT::AddCore(bool isRemoveLog) {
   this->ReqLast_.reset();
   const fon9::TimeStamp tday = fon9::LocalNow();
   auto fnCoreCreator = GetOmsCoreByThreadCreator(fon9::HowWait::Block);
   OmsCoreByThreadBaseSP core = fnCoreCreator(
      std::bind(&OmsCoreUT::InitCoreTables, this, std::placeholders::_1),
      tday, 0,
      this->CoreMgr_.get(),
      "omstw/f9omstwUT",
      "f9omstwUT"
   );
   if (auto oldCore = this->CoreMgr_->RemoveCurrentCore())
      this->WaitCoreDone(oldCore.get());
   // 新舊 OmsCore 交接完畢, 啟動新的 OmsCore.
   fon9::TimedFileName logfn(gLogPathFmt + "f9omstwUT.log", fon9::TimedFileName::TimeScale::Day);
   logfn.RebuildFileNameExcludeTimeZone(tday);
   if (isRemoveLog)
      ::remove(logfn.GetFileName().c_str());
   core->StartToCoreMgr(logfn.GetFileName(), -1/*CpuId*/, fon9::TimeInterval_Millisecond(1), -1);
   this->WaitCoreDone(core.get());
   if (core->CoreSt() != OmsCoreSt::CurrentCoreReady)
      return false;
   OmsRequestPolicy* poRequest{new OmsRequestPolicy};
   this->PoRequest_.reset(poRequest);
   core->RunCoreTask([poRequest](OmsResource& res) {
      poRequest->SetOrdTeamGroupCfg(res.OrdTeamGroupMgr_.SetTeamGroup("ut", "*"));
      poRequest->AddIvConfig(nullptr, "*", OmsIvConfig{OmsIvRight::AllowAll});
   });
   this->WaitCoreDone(core.get());
   return true;
}
void OmsCoreUT::WaitCoreDone(OmsCore* core) {
   if (!core)
      return;
   if (core->CoreSt() != OmsCoreSt::CurrentCoreReady)
      return;
   fon9::CountDownLatch waiter{1};
   if (core->RunCoreTask([&waiter](OmsResource&) {
      waiter.CountDown();
   }))
      waiter.Wait();
}
const OmsOrderRaw* OmsCoreUT::GetOrderRaw(fon9::StrView ordkey) {
   auto core = this->CurrentCore();
   if (!core)
      return nullptr;
   fon9::CountDownLatch waiter{1};
   const OmsOrderRaw*   retval = nullptr;
   core->RunCoreTask([&](OmsResource& res) {
      fon9::StrView key = StrFetchTrim(ordkey, '-');
      if (ordkey.empty()) {
         if (auto* item = res.Backend_.GetItem(fon9::StrTo(key, OmsRxSNO{})))
            if ((item = item->CastToOrder()) != nullptr)
               retval = static_cast<const OmsOrderRaw*>(item);
      }
      else if (auto* brk = res.Brks_->GetBrkRec(key)) {
         key = StrFetchTrim(ordkey, '-');
         auto& mkt = brk->GetMarket(static_cast<f9fmkt_TradingMarket>(key.Get1st()));
         key = StrFetchTrim(ordkey, '-');
         auto& ses = mkt.GetSession(static_cast<f9fmkt_TradingSessionId>(key.Get1st()));
         if (auto* map = ses.GetOrdNoMap()) {
            if (auto* ord = map->GetOrder(ordkey))
               retval = ord->Tail();
         }
      }
      waiter.CountDown();
   });
   waiter.Wait();
   return retval;
}

bool OmsCoreUT::OnCommand(fon9::StrView cmd, fon9::StrView args) {
   if (cmd == "sleep") {
      unsigned secs = fon9::StrTo(args, 0u);
      for (;;) {
         printf("\r%u ", secs);
         fflush(stdout);
         std::this_thread::sleep_for(std::chrono::seconds(1));
         if (secs <= 0)
            break;
         --secs;
      }
      printf("\r");
      return true;
   }
   if (cmd == "reload") {
      if (!this->AddCore(false)) {
         puts("Reload core error, please check log.");
      }
      return true;
   }

   fon9::StrView  tag, value;
   if (cmd == "ev") {
      *const_cast<char*>(cmd.end()) = '\0';
      tag = fon9::StrFetchTrim(args, &fon9::isspace); // factory name.
      auto evfac = this->CoreMgr_->EventFactoryPark().GetFactory(tag);
      if (evfac == nullptr) {
         *const_cast<char*>(tag.end()) = '\0';
         printf("Oms %s factory:%s not found.\n", cmd.begin(), tag.begin());
         return true;
      }
      auto ev = evfac->MakeReloadEvent();
      if (PutFields(evfac->Name_.c_str(), evfac->Fields_, fon9::seed::SimpleRawWr{*ev}, args))
         this->CurrentCore()->EventToCore(std::move(ev));
      return true;
   }

   if (cmd == "req" || cmd == "rpt") {
      *const_cast<char*>(cmd.end()) = '\0';
      // req ReqName                     Field=Value|...
      // rpt RptName  Kind:QtyStyle:UID  ReportSt=hh|...other fields...
      OmsRequestRunner reqRunner{args};
      tag = fon9::StrFetchTrim(args, &fon9::isspace); // factory name.
      auto reqfac = this->CoreMgr_->RequestFactoryPark().GetFactory(tag);
      if (reqfac == nullptr) {
         *const_cast<char*>(tag.end()) = '\0';
         printf("Oms %s factory:%s not found.\n", cmd.begin(), tag.begin());
         return true;
      }

      if (cmd == "req")
         reqRunner.Request_ = reqfac->MakeRequest(fon9::UtcNow());
      else { // cmd == "rpt"
         tag = StrFetchTrim(args, &fon9::isspace);
         f9fmkt_RxKind reqKind = static_cast<f9fmkt_RxKind>(tag.Get1st());
         reqRunner.Request_ = reqfac->MakeReportIn(reqKind, fon9::UtcNow());
         if (reqRunner.Request_) {
            // 從 tag 移除「Kind:」, tag 剩餘「QtyStyle:UID」.
            StrFetchTrim(tag, ':');
            const auto qtyStyle = static_cast<OmsReportQtyStyle>(StrTo(tag, 0u));
            if (auto* rpt = dynamic_cast<OmsTwsReport*>(reqRunner.Request_.get()))
               rpt->QtyStyle_ = qtyStyle;
            else if (auto* frpt = dynamic_cast<OmsTwsFilled*>(reqRunner.Request_.get()))
               frpt->QtyStyle_ = qtyStyle;
            // 從 tag 移除「QtyStyle:」, tag 剩餘「UID」.
            StrFetchTrim(tag, ':');
            if (tag == "-") { // 「-」: 針對前一筆 req 建立回報.
               if (this->ReqLast_) {
                  reqRunner.Request_->ReqUID_ = this->ReqLast_->ReqUID_;
                  reqRunner.Request_->BrkId_ = this->ReqLast_->BrkId_;
                  reqRunner.Request_->OrdNo_ = this->ReqLast_->OrdNo_;
                  reqRunner.Request_->SetMarket(this->ReqLast_->Market());
                  reqRunner.Request_->SetSessionId(this->ReqLast_->SessionId());
               }
            }
            else if (!tag.empty()) {
               reqRunner.Request_->ReqUID_.AssignFrom(tag);
            }
         }
      }
      this->ReqLast_ = reqRunner.Request_;
      if (!reqRunner.Request_) {
         printf("Oms %s factory:%s cannot make request.\n", cmd.begin(), reqfac->Name_.c_str());
         return true;
      }
      fon9::seed::SimpleRawWr    wr{*reqRunner.Request_};
      const fon9::seed::Field*   fld;
      if ((fld = reqfac->Fields_.Get("BrkId")) != nullptr)
         fld->StrToCell(wr, gBrkId);
      if (!PutFields(reqfac->Name_.c_str(), reqfac->Fields_, wr, args))
         return true;
      if (auto* reqt = dynamic_cast<OmsRequestTrade*>(reqRunner.Request_.get()))
         reqt->SetPolicy(this->PoRequest_);
      if (!this->CurrentCore()->MoveToCore(std::move(reqRunner))) {
         // auto emsg = reqRunner.Request_->AbandonReason();
         // printf("ErrCode=%u:%s\n", reqRunner.Request_->ErrCode(), (emsg ? emsg->c_str() : ""));
      }
      return true;
   }

   const OmsOrderRaw* ordraw;
   if (cmd == "chkr" || cmd == "chko") {
      // 檢查最後一次 "req" or "rpt" 的結果.
      if (!this->ReqLast_) {
         gErrLineNo.push_back(gLineNo);
         printf("@Ln=%u\n" "[ERROR] No last 'req' or 'rpt'.\n", gLineNo);
         return true;
      }
      fon9::BufferList  rbuf;
      if (cmd == "chkr") {
         // {ErrCode:Message}  ...others fields...
         // Message.empty() 表示不檢查 Message.
         if (args.Get1st() == '{') {
            value = fon9::SbrFetchInsideNoTrim(args);
            tag = StrFetchTrim(value, ':');
            if (!tag.empty()) {
               const auto ec = StrTo(tag, 0u);
               if (this->ReqLast_->ErrCode() != static_cast<OmsErrCode>(ec)) {
                  gErrLineNo.push_back(gLineNo);
                  printf("@Ln=%u\n" "[ERROR] Last req/rpt ErrCode=%u|expect=%u\n", gLineNo, this->ReqLast_->ErrCode(), ec);
                  return true;
               }
            }
            if (!value.empty()) {
               fon9::StrView emsg;
               if (const auto* amsg = this->ReqLast_->AbandonReason())
                  emsg = fon9::ToStrView(*amsg);
               if (value != emsg) {
                  *const_cast<char*>(value.end()) = '\0';
                  gErrLineNo.push_back(gLineNo);
                  printf("@Ln=%u\n" "[ERROR] Last req/rpt Message=%s|expect=%s\n",
                         gLineNo, emsg.begin() ? emsg.begin() : "", value.begin());
                  return true;
               }
            }
         }
         rbuf = CompareFieldValues(fon9::StrTrimHead(&args), this->ReqLast_->Creator().Fields_,
                                   fon9::seed::SimpleRawRd{*this->ReqLast_}, '|')
                  .MoveOut();
      }
      else {
         if (args.Get1st() == '{') {
            value = SbrFetchInsideNoTrim(args);
            if ((ordraw = this->GetOrderRaw(value)) == nullptr) {
               *const_cast<char*>(value.end()) = '\0';
               gErrLineNo.push_back(gLineNo);
               printf("@Ln=%u\n" "[ERROR] order not found: %s\n", gLineNo, value.begin());
               return true;
            }
         }
         else if ((ordraw = this->ReqLast_->LastUpdated()) == nullptr) {
            gErrLineNo.push_back(gLineNo);
            printf("@Ln=%u\n" "[ERROR] Last req/rpt has no order.\n", gLineNo);
            return true;
         }
         rbuf = CompareFieldValues(args, ordraw->Order().Creator().Fields_, fon9::seed::SimpleRawRd{*ordraw}, '|').MoveOut();
      }
      puts(fon9::BufferTo<std::string>(rbuf).c_str());
      return true;
   }

   if (cmd == "pr" || cmd == "po") {
      // 印出最後一次 "req" or "rpt" 的結果.
      if (!this->ReqLast_) {
         puts("No last 'req' or 'rpt'.");
         return true;
      }
      fon9::RevBufferList rbuf{0};
      if (cmd == "pr") {
         rbuf = RevPrintFieldValues(this->ReqLast_->Creator().Fields_, fon9::seed::SimpleRawRd{*this->ReqLast_});
         if (const auto* emsg = this->ReqLast_->AbandonReason())
            RevPrint(rbuf, "AbandonReason=", *emsg, '\n');
         RevPrint(rbuf, this->ReqLast_->Creator().Name_, "\n"
                  "RxSNO=", this->ReqLast_->RxSNO(), "\n"
                  "ErrCode=", this->ReqLast_->ErrCode(), '\n');
      }
      else {
         if (args.empty()) {
            if ((ordraw = this->ReqLast_->LastUpdated()) == nullptr) {
               puts("Last req/rpt has no order.");
               return true;
            }
         }
         else if ((ordraw = this->GetOrderRaw(args)) == nullptr) {
            *const_cast<char*>(args.end()) = '\0';
            printf("Order not found: %s\n", args.begin());
            return true;
         }
         rbuf = RevPrintFieldValues(ordraw->Order().Creator().Fields_, fon9::seed::SimpleRawRd{*ordraw});
         fon9::RevPrint(rbuf, ordraw->Order().Creator().Name_, "\n"
                        "RxSNO=", ordraw->RxSNO(), '\n');
      }
      puts(fon9::BufferTo<std::string>(rbuf.MoveOut()).c_str());
      return true;
   }

   if (cmd == "pi" || cmd=="pe") { // print info.
      printf("ErrCount=%u|PassedCount=%u\n", static_cast<unsigned>(gErrLineNo.size()), gPassedCount);
      if (cmd == "pe" && !gErrLineNo.empty()) {
         printf("ErrList:");
         for (auto lno : gErrLineNo)
            printf("|=%u", lno);
         printf("\n");
      }
      return true;
   }
   return false;
}

//--------------------------------------------------------------------------//
int MainOmsCoreUT(OmsCoreUT& omsCoreUT, int argc, const char** pargv) {
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif
   fon9_SetConsoleUTF8();
   fon9_SetupCtrlBreakHandler();
   // ----------------------------
   for (int L = 1; L < argc;) {
      const char* parg = *++pargv;
      switch (parg[0]) {
      case '-': case '/':  break;
      default:
      __UNKNOWN_ARGUMENT:
         printf("Unknown argument: %s\n", parg);
      __USAGE:
         puts("Usage:\n"
              "-l LogPathFmt\n"
              "   log to file: LogPathFmt + 'fon9sys.log'\n"
              "   default is log to console.\n"
              "   default log path = " kCSTR_DefaultLogPathFmt "\n");
         return 3;
      }
      L += 2;
      ++pargv;
      switch (parg[1]) {
      case 'l':
         gLogPathFmt = fon9::FilePath::AppendPathTail(fon9::StrView_cstr(*pargv));
         {
            auto res = fon9::InitLogWriteToFile(gLogPathFmt + "fon9sys.log", fon9::FileRotate::TimeScale::Day, 0, 0);
            if (res.IsError())
               puts(fon9::RevPrintTo<std::string>("InitLogFile|err=", res.GetError()).c_str());
         }
         break;
      case '?':   goto __USAGE;
      default:    goto __UNKNOWN_ARGUMENT;
      }
   }
   if (gLogPathFmt.empty())
      gLogPathFmt = kCSTR_DefaultLogPathFmt;
   gIsScLogAll = fon9::EnabledYN::Yes;
   // ----------------------------
   if (!omsCoreUT.AddCore(true))
      return 3;
   // ----------------------------
   OmsUtConsoleSeedSessionSP  seedMgr{new OmsUtConsoleSeedSession(omsCoreUT.CoreMgr_,
      new fon9::auth::AuthMgr(omsCoreUT.CoreMgr_, "MaAuth", nullptr),
      "seedMgr")};
   seedMgr->SetAdminMode();
   // ----------------------------
   char     cmdbuf[4096];
   unsigned fgetsErrCount = 0; (void)fgetsErrCount;
   while (fon9_AppBreakMsg == NULL) {
      printf("> ");
      if (!fgets(cmdbuf, sizeof(cmdbuf), stdin)) {
      #ifdef _MSC_VER
         if (++fgetsErrCount > 1000) // MSVC fgets 讀到 UTF8, 會有問題, 但仍可繼續讀下一行,
            break;                   // 所以, 連續錯太多次才視為真的 eof.
         ++gLineNo;                  // 這個問題, 在找到正確解法前, 先這樣處理吧...
         continue;
      #else
         break;
      #endif
      }
      omsCoreUT.WaitCoreDone();
      fgetsErrCount = 0;
      ++gLineNo;
      fon9::StrView args{fon9::StrView_cstr(cmdbuf)};
      if (fon9::StrTrim(&args).empty())
         continue;
      if (args.Get1st() == '#')
         continue;
      fon9::StrView cmd = StrFetchTrim(args, &fon9::isspace);
      if (cmd == "quit")
         break;
      if (!omsCoreUT.OnCommand(cmd, args)) {
         seedMgr->FeedLine(fon9::StrView{cmd.begin(), args.end()});
         seedMgr->Wait();
      }
   }
   // ----------------------------
   seedMgr.reset();
   return 0;
}

} // namespaces

//--------------------------------------------------------------------------//
#ifdef fon9_POSIX
fon9_GCC_WARN_DISABLE("-Wold-style-cast")
fon9_GCC_WARN_DISABLE_NO_PUSH("-Wnonnull")
// 使用 -static 靜態連結執行程式, 使用 std::condition_variable, std::thread 可能造成 crash 的問題.
void fixed_pthread_bug_in_cpp_std() {
   pthread_cond_signal((pthread_cond_t *) nullptr);
   pthread_cond_broadcast((pthread_cond_t *) nullptr);
   pthread_cond_init((pthread_cond_t *) nullptr,
      (const pthread_condattr_t *) nullptr);
   pthread_cond_destroy((pthread_cond_t *) nullptr);
   pthread_cond_timedwait((pthread_cond_t *) nullptr, (pthread_mutex_t *)
                          nullptr, (const struct timespec *) nullptr);
   pthread_cond_wait((pthread_cond_t *) nullptr, (pthread_mutex_t *)(nullptr));

   pthread_detach(pthread_self());
   pthread_join(pthread_self(), NULL);
}
fon9_GCC_WARN_POP
#endif
