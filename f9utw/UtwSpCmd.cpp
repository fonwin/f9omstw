// \file f9utw/UtwSpCmd.cpp
// \author fonwinz@gmail.com
#include "f9utw/UtwSpCmd.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/SeedVisitor.hpp"
#include "f9omstw/OmsPoIvListAgent.hpp"
#include "f9omstw/OmsPoUserRightsAgent.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsRequestFactory.hpp"

namespace f9omstw {
using namespace fon9::seed;

//--------------------------------------------------------------------------//
void SpCmdItem::OnSeedCommand(SeedOpResult& res,
                              fon9::StrView cmdln,
                              FnCommandResultHandler resHandler,
                              MaTreeBase::Locker&& ulk,
                              SeedVisitor* visitor) {
   (void)ulk; (void)cmdln; (void)visitor;
   res.OpResult_ = OpResult::bad_command_argument;
   resHandler(res, "Unknown command.");
}
OpResult SpCmdItem::FromPodOp_CheckSubscribeRights(Tab& tab, const SeedVisitor& visitor) {
   (void)tab;
   if (ToStrView(this->Args_.ReqFrom_.UserId_) == ToStrView(visitor.AuthR_.AuthcId_))
      return OpResult::no_error;
   return OpResult::access_denied;
}
OpResult SpCmdItem::FromPodOp_Subscribe(const MaTreePodOp& lk, fon9::SubConn* pSubConn, Tab& tab, FnSeedSubr subr) {
   this->NotifySubj_.Subscribe(pSubConn, subr);
   subr(SeedNotifySubscribeOK{*lk.Sender_, tab, lk.KeyText_, SimpleRawRd{*lk.Seed_}});
   return OpResult::no_error;
}
OpResult SpCmdItem::FromPodOp_Unsubscribe(const MaTreePodOp& lk, fon9::SubConn* pSubConn, Tab& tab) {
   (void)tab;
   return this->NotifySubj_.SafeUnsubscribe(lk.Locker_, pSubConn);
}
//--------------------------------------------------------------------------//
LayoutSP SpCmdItem::MakeDefaultLayout() {
   Fields fields;
   fields.Add(fon9_MakeField(SpCmdItem, Title_,       "CommandArgs"));
   fields.Add(fon9_MakeField(SpCmdItem, Description_, "From"));
   fields.Add(fon9_MakeField(SpCmdItem, State_,       "State"));
   return new Layout1(fon9_MakeField(SpCmdItem, Name_, "Id"),
                      new Tab{Named{"Items"}, std::move(fields)});
}
void SpCmdItem::UpdateState(std::string&& st) {
   auto lk = this->SpCmdMgr_.Sapling_->Lock();
   this->State_ = std::move(st);
}
//--------------------------------------------------------------------------//
void SpCmdOrd::OmsLogUpdateState(std::string&& st) {
   fon9::RevBufferList rbuf{128};
   fon9::RevPrint(rbuf, '\n');
   if (!st.empty()) {
      fon9::RevPrint(rbuf, "|st=", st);
      this->UpdateState(std::move(st));
   }
   fon9::RevPrint(rbuf, this->Name_, '|', this->GetTitle(), '|', this->GetDescription());
   this->SpCmdMgr_.OmsCore_->LogAppend(std::move(rbuf));
}
void SpCmdOrd::StartRunInCore(OmsResource& res) {
   this->RequestPolicy_ = this->Args_.PolicyCfg_.MakePolicy(res);
   this->OmsLogUpdateState(fon9::RevPrintTo<std::string>(fon9::UtcNow(), "|Start"));
}
void SpCmdOrd::Start() {
   this->SpCmdMgr_.OmsCore_->RunCoreTask(
      std::bind(&SpCmdOrd::StartRunInCore,
                fon9::intrusive_ptr<SpCmdOrd>(this),
                std::placeholders::_1));
}
//--------------------------------------------------------------------------//
void SpCmdForEachOrd::StartRunInCore(OmsResource& res) {
   base::StartRunInCore(res);
   // 取得當時最後序號 => 當作結束序號.
   this->FinishedRxSNO_ = res.Backend_.LastSNO();
   res.ReportRecover(this->LastCheckRxSNO_,
                     std::bind(&SpCmdForEachOrd::OnReportRecover,
                               fon9::intrusive_ptr<SpCmdForEachOrd>(this),
                               std::placeholders::_1,
                               std::placeholders::_2));
}
void SpCmdForEachOrd::OnSeedCommand(SeedOpResult& res,
                                    fon9::StrView cmdln,
                                    FnCommandResultHandler resHandler,
                                    MaTreeBase::Locker&& ulk,
                                    SeedVisitor* visitor) {
   // SpCmdForEachOrd: 要支援那些指令呢? pause:暫停, resume:繼續, rerun:重新重頭執行; info:提供現在執行情況....
   base::OnSeedCommand(res, cmdln, std::move(resHandler), std::move(ulk), visitor);
}
//--------------------------------------------------------------------------//
SpCmdDord::~SpCmdDord() {
   this->RptUnsub();
}
void SpCmdDord::StartRunInCore(OmsResource& res) {
   base::StartRunInCore(res);
   // 同時訂閱最新回報, 用來監測刪單要求.
   res.ReportSubject().Subscribe(
      &this->RptSubr_,
      std::bind(&SpCmdDord::OnReportHandle,
                fon9::intrusive_ptr<SpCmdDord>(this),
                std::placeholders::_1,
                std::placeholders::_2)
   );
}
void SpCmdDord::RptUnsub() {
   this->SpCmdMgr_.OmsCore_->ReportSubject().Unsubscribe(&this->RptSubr_);
}
void SpCmdDord::OnParentTreeClear(Tree& tree) {
   base::OnParentTreeClear(tree);
   this->RptUnsub();
}
void SpCmdDord::OnReportHandle(OmsCore& core, const OmsRxItem& item) {
   (void)core;
   DreqList::iterator ifind;
   const OmsRxItem* ireq;
   OmsErrCode ec;
   if (item.RxKind() == f9fmkt_RxKind_RequestDelete) {
      assert(dynamic_cast<const OmsRequestTrade*>(&item) != nullptr);
      if (!static_cast<const OmsRequestTrade*>(&item)->IsAbandoned())
         return;
      ireq = &item;
      ec = static_cast<const OmsRequestTrade*>(&item)->ErrCode();
   }
   else if (item.RxKind() == f9fmkt_RxKind_Order) {
      assert(dynamic_cast<const OmsOrderRaw*>(&item) != nullptr);
      if (static_cast<const OmsOrderRaw*>(&item)->RequestSt_ < f9fmkt_TradingRequestSt_Done)
         return;
      ireq = &static_cast<const OmsOrderRaw*>(&item)->Request();
      ec = static_cast<const OmsOrderRaw*>(&item)->ErrCode_;
   }
   else
      return;
   ifind = std::find(this->DreqList_.begin(), this->DreqList_.end(), ireq);
   if (ifind == this->DreqList_.end())
      return;
   if (ec == OmsErrCode_NoReadyLine) {
      this->StopRecover();
      this->DreqList_.clear();
   }
   else {
      this->DreqList_.erase(ifind);
   }
}
OmsRxSNO SpCmdDord::OnReportRecover(OmsCore& core, const OmsRxItem* item) {
   (void)core;
   if (this->FinishedRxSNO_ == 0 // 強制中斷.
       || item == nullptr) {     // 已回補完畢.
   __FINISHED:;
      this->OmsLogUpdateState(fon9::RevPrintTo<std::string>(fon9::UtcNow(), "|Done@RxSNO=", this->LastCheckRxSNO_));
   __NOTIFY:;
      this->RptUnsub();
      if (!this->NotifySubj_.IsEmpty()) {
         fon9::seed::MaTree& ownerTree = *static_cast<MaTree*>(this->SpCmdMgr_.GetSapling().get());
         auto subjLocker = ownerTree.Lock();
         fon9::seed::SeedSubj_Notify(this->NotifySubj_, ownerTree, *ownerTree.LayoutSP_->GetTab(0), &this->Name_, *this);
      }
      return 0; // 返回 0 表示停止回補, 不會再有回補訊息.
   }
   // 限制[尚未完成]的刪單要求數量.
   if (this->DreqList_.size() >= 5) {
      // 返回現在序號,表示先暫停,等下一輪回補.
      return item->RxSNO();
   }
   if (const auto* iniReq = dynamic_cast<const OmsRequestIni*>(item)) {
      if (fon9_LIKELY(this->CheckForDord(*iniReq))) {
         // 正常完成 Dord 的檢查, 可能有送出刪單, 或是沒必要送出.
      }
      else {
         this->OmsLogUpdateState("dord: Cannot make delete request. STOP cmd.");
         goto __NOTIFY;
      }
   }
   // 返回下一個要回補的序號, 一般而言返回 item->RxSNO() + 1;
   this->LastCheckRxSNO_ = item->RxSNO();
   if (this->LastCheckRxSNO_ == this->FinishedRxSNO_)
      goto __FINISHED;
   return this->LastCheckRxSNO_ + 1;
}
static inline bool IsWorkingDelete(const OmsOrderRaw& iord) {
   const auto& req = iord.Request();
   if (req.RxKind() != f9fmkt_RxKind_RequestDelete)
      return false;
   const OmsOrderRaw* ilast = req.LastUpdated();
   return((ilast ? ilast : &iord)->RequestSt_ < f9fmkt_TradingRequestSt_Done);
}
bool SpCmdDord::CheckForDord(const OmsRequestIni& iniReq) {
   if (!this->CheckForDord_ArgsFilter(iniReq))
      return true;
   const auto* ilast = iniReq.LastUpdated();
   if (ilast == nullptr)
      return true;
   const auto& order = ilast->Order();
   if (!order.IsWorkingOrder())
      return true;
   OmsIvConfig ivConfig;
   if (IsEnumContains(this->RequestPolicy_->GetIvRights(order.ScResource().Ivr_.get(), &ivConfig), OmsIvRight::DenyTradingChgQty))
      return true;
   // 檢查是否已存在刪單要求.
   const auto* orawLast = order.Tail();
   if (IsWorkingDelete(*orawLast))
      return true;
   const auto* iord = order.Head();
   while (iord) {
      if (IsWorkingDelete(*iord))
         return true;
      iord = iord->Next();
   }
   // ----- 執行刪單要求.
   auto req = this->MakeDeleteRequest(iniReq);
   if (!req)
      return false;
   OmsRequestRunner runner;
   runner.Request_ = req;
   req->SetIniFrom(iniReq);
   req->SetMarket(orawLast->Market());
   req->SetSessionId(orawLast->SessionId());
   req->OrdNo_ = orawLast->OrdNo_;
   req->UserId_ = this->Args_.ReqFrom_.UserId_;
   req->FromIp_ = this->Args_.ReqFrom_.FromIp_;
   req->SesName_ = this->Args_.ReqFrom_.SesName_;
   req->SetPolicy(this->RequestPolicy_);
   req->LgOut_ = ivConfig.LgOut_;
   // 等最後一筆刪單結果為非 Queuing, 才繼續刪單.
   // 若刪單送出結果為 Queuing, 則返回 item->RxSNO() 表示暫時停止, 等下一個回補週期.
   // 如果結果 ec=900:No ready line; 則不要再送.
   DreqList_.push_back(runner.Request_.get());
   this->SpCmdMgr_.OmsCore_->MoveToCore(std::move(runner));
   return true;
}
//--------------------------------------------------------------------------//
TreeSP SpCmdMgr::GetSapling() {
   return this->Sapling_;
}
bool SpCmdMgr::ParseSpCmdArg(SpCmdArgs& args, fon9::StrView tag, fon9::StrView value) {
   (void)args; (void)tag; (void)value;
   return false;
}
SpCmdItemSP SpCmdMgr::MakeSpCmdDord(fon9::StrView& strResult,
                                    fon9::StrView cmdln,
                                    MaTreeBase::Locker&& ulk,
                                    SeedVisitor* visitor) {
   (void)ulk;
   if (this->OmsCore_->PublishedSNO() == 0) {
      strResult = "dord: OmsCore is empty.";
      return nullptr;
   }
   if (!visitor) {
      strResult = "dord: Unknown visitor.";
      return nullptr;
   }
   // 僅支援1個Ivac, 及1個Symb;
   SpCmdArgs     args;
   fon9::StrView tag, value;
   while (StrFetchTagValue(cmdln, tag, value)) {
      if (tag == "Ivac") {
         if (!args.IvKey_.empty()) {
            strResult = "dord: Only supports one Ivac.";
            return nullptr;
         }
         args.IvKey_ = OmsIvKey::Normalize(value);
      }
      else if (tag == "Symb") {
         if (!args.SymbId_.empty()) {
            strResult = "dord: Only supports one Symb.";
            return nullptr;
         }
         args.SymbId_.assign(value);
      }
      else if (!this->ParseSpCmdArg(args, tag, value)) {
         strResult = "dord: Unknown arg or value";
         return nullptr;
      }
   }
   if (args.IvKey_.empty() || args.SymbId_.empty()) {
      strResult = "dord: Arg 'Ivac=' or 'Symb=' not found.";
      return nullptr;
   }
   // 透過 AuthMgr 取得 OmsPoIvList 權限: 要操作的帳號, 必須要有刪單權限.
   const auto& authr = visitor->AuthR_;
   // 刪單用不到 UserRights;
   // if (auto agUserRights = visitor->AuthR_.AuthMgr_->Agents_->Get<OmsPoUserRightsAgent>(fon9_kCSTR_OmsPoUserRightsAgent_Name))
   //    agUserRights->GetPolicy(visitor->AuthR_, args.PolicyCfg_.UserRights_, &args.PolicyCfg_.TeamGroupName_);
   if (!authr.AuthMgr_->GetPolicy<OmsPoIvListAgent>(fon9_kCSTR_OmsPoIvListAgent_Name, authr, args.PolicyCfg_.IvList_)) {
      // 沒有可用帳號.
      strResult = "dord: IvList is empty.";
      return nullptr;
   }
   fon9::StrView strIvKey{ToStrView(args.IvKey_)};
   if (strIvKey == "*") {
      // 指定該使用者全部的可用帳號;
   }
   else {
      OmsIvConfig ivConfig{OmsIvRight::DenyAll};
      auto        ivFind = args.PolicyCfg_.IvList_.find(args.IvKey_);
      if (ivFind != args.PolicyCfg_.IvList_.end())
         ivConfig = ivFind->second;
      else if (!args.PolicyCfg_.IvList_.empty()) {
         // 從 end 往 begin 方向尋找, 如果有 "*", 就會在最後才判斷.
         const auto ivBeg = args.PolicyCfg_.IvList_.begin();
         do {
            --ivFind;
            if (fon9::IsStrWildMatch(strIvKey, ToStrView(ivFind->first))) {
               ivConfig = ivFind->second;
               break;
            }
         } while (ivFind != ivBeg);
      }
      if (IsEnumContains(ivConfig.Rights_, OmsIvRight::DenyTradingChgQty)) {
         strResult = "dord: Deny Ivac.";
         return nullptr;
      }
      // args.PolicyCfg_.IvList_ 只保留 ivKey 的權限, 才不會刪除其他帳號的回報.
      args.PolicyCfg_.IvList_.clear();
      args.PolicyCfg_.IvList_.kfetch(args.IvKey_).second = ivConfig;
   }

   fon9::StrView userId;
   fon9::StrView fromIp;
   visitor->GetUserAndFrom(userId, fromIp);
   args.ReqFrom_.UserId_.AssignFrom(userId);
   args.ReqFrom_.FromIp_.AssignFrom(fromIp);

   {
      //const std::lock_guard<MaTreeBase::Locker> lock(ulk);
      // for (auto& item : *ulk) {
      //    if (auto* dord = dynamic_cast<SpCmdDord*>(item.get())) {
      //       if (dord->Args_.SymbId_ == args.SymbId_
      //           && ToStrView(dord->Args_.PolicyCfg_.IvList_.begin()->first) == strIvKey) {
      //          // 已有相同 filter 的 dord,
      //          // 應重設: FinishedRxSNO_, userId, fromIp;
      //          // 但是 => 避免先前有斷線之類的刪單失敗,
      //          //         所以即使有相同的 dord,
      //          //         仍再次執行一次!!
      //       }
      //    }
      // }
   }
   return this->MakeSpCmdDord_Impl(
      strResult,
      std::move(args),
      fon9::Named(fon9::RevPrintTo<std::string>("dord_", ++this->SpCmdId_, fon9::FmtDef{3, fon9::FmtFlag::IntPad0}),
                  fon9::RevPrintTo<std::string>("Ivac=", args.IvKey_, "|Symb=", args.SymbId_),
                  visitor->GetUFrom()));
}
void SpCmdMgr::OnSeedCommand(SeedOpResult& res,
                             fon9::StrView cmdln,
                             FnCommandResultHandler resHandler,
                             MaTreeBase::Locker&& ulk,
                             SeedVisitor* visitor) {
   fon9::intrusive_ptr<SpCmdItem> spCmd;
   fon9::StrView strResult;
   fon9::StrView cmdsel = fon9::StrFetchTrim(cmdln, fon9::isspace);
   std::string   resbuf;
   if (cmdsel == "?") {
      strResult = "dord\x1" "Delete order\x1" "Ivac=|Symb=";
   }
   else if (cmdsel == "dord") { // 是否要進行流量處理?
      spCmd = this->MakeSpCmdDord(strResult, cmdln, std::move(ulk), visitor);
   }
   else {
      base::OnSeedCommand(res, cmdsel, std::move(resHandler), std::move(ulk), visitor);
      return;
   }
   if (spCmd) {
      this->Sapling_->Add(spCmd);
      spCmd->Start();
      resbuf = fon9::RevPrintTo<std::string>(cmdsel, ".Running|id=", spCmd->Name_, '|', spCmd->GetTitle());
      strResult = fon9::ToStrView(resbuf);
   }
   resHandler(res, strResult);
}

} // namespaces
