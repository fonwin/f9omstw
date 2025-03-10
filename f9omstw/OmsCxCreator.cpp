// \file f9omstw/OmsCxCreator.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCxCreator.hpp"
#include "f9omstw/OmsCxSymbTree.hpp"
#include "f9omstw/OmsScBase.hpp"
#include "fon9/SimpleFactory.hpp"

namespace f9omstw {

#ifdef _DEBUG
//#define _PRINT_DEBUG_INFO   // 已經驗證完畢,可以不用輸出了!
#endif
// ======================================================================== //
CxUnitCreator GetCxUnitCreator(fon9::StrView condName) {
   return fon9::SimpleFactoryRegister<CxUnitCreator>(condName, nullptr);
}
void AddCxUnitCreator(fon9::StrView condName, CxUnitCreator fnCreator) {
   fon9::SimpleFactoryRegister<CxUnitCreator>(condName, fnCreator);
}
// ======================================================================== //
void OmsCxCreatorArgs::CxArgsStr_CheckRemain() {
   const char ch = static_cast<char>(fon9::StrTrimHead(&this->CxArgsStr_).Get1st());
   switch (ch) {
   case ',':   break;
   case '@':   this->Cond_.CheckFlags_ |= OmsCxCheckFlag::UsrWaitTrueLock;  break;
   default:    return;
   }
   this->CxArgsStr_.IncBegin(1);
   this->CxNormalized_.push_back(ch);
}
bool OmsCxCreatorArgs::ParseMdPri(OmsCxCheckFlag chkFlags, MdPri& result) {
   result = fon9::StrTo(&this->CxArgsStr_, MdPri::Null());
   if (fon9_UNLIKELY(result.IsNull())) {
      if (!IsEnumContains(chkFlags, OmsCxCheckFlag::PriAllowMarket)
          || fon9::StrTrimHead(&this->CxArgsStr_).Get1st() != 'M') {
         this->RequestAbandon(OmsErrCode_CondSc_BadPri);
         return false;
      }
      this->CxArgsStr_.IncBegin(1);
      this->CxNormalized_.push_back('M');
   }
   else {
      fon9::RevBufferFixedSize<128> rbuf;
      fon9::RevPrint(rbuf, result);
      this->CxNormalized_.append(ToStrView(rbuf));
   }
   this->CxArgsStr_CheckRemain();
   return true;
}
bool OmsCxCreatorArgs::ParseMdQty(OmsCxCheckFlag chkFlags, MdQty& result) {
   result = fon9::StrTo(&this->CxArgsStr_, static_cast<MdQty>(0));
   if (fon9_UNLIKELY(result == 0 && IsEnumContains(chkFlags, OmsCxCheckFlag::QtyCannotZero))) {
      this->RequestAbandon(OmsErrCode_CondSc_BadQty);
      return false;
   }
   fon9::RevBufferFixedSize<128> rbuf;
   fon9::RevPrint(rbuf, result);
   this->CxNormalized_.append(ToStrView(rbuf));
   this->CxArgsStr_CheckRemain();
   return true;
}
bool OmsCxCreatorArgs::ParseCondArgsPQ(OmsCxCheckFlag chkFlags) {
   this->Cond_.CheckFlags_ = chkFlags;
   if (IsEnumContains(chkFlags, OmsCxCheckFlag::PriNeeds)) {
      if (fon9_UNLIKELY(!this->ParseMdPri(chkFlags, this->Cond_.CondPri_)))
         return false;
   }
   else {
      this->Cond_.CondPri_.AssignNull();
   }
   if (IsEnumContains(chkFlags, OmsCxCheckFlag::QtyNeeds)) {
      if (fon9_UNLIKELY(!this->ParseMdQty(chkFlags, this->Cond_.CondQty_)))
         return false;
   }
   else {
      this->Cond_.CondQty_ = 0;
   }
   return true;
}
// ======================================================================== //
struct OmsCxCreatorInternal : public OmsCxCreatorArgs {
   fon9_NON_COPY_NON_MOVE(OmsCxCreatorInternal);
   using base = OmsCxCreatorArgs;
   using base::base;

   uint16_t                CxUnitCount_{0};
   uint16_t                CxSymbCount_{0};
   char                    Padding____[4];
   std::vector<OmsCxUnit*> CxUnitList_;

   OmsCxExprUP CreateCxExpr();
   OmsCxExprUP CreateCxExpr(bool isWaitRB);
   OmsCxUnitUP CreateCxUnit();

   /// 建立 OmsCxUnit::CondSameSymbNext_ 及 CondOtherSymbNext_;
   /// 返回 FirstCxUnit_;
   OmsCxUnit* MakeCxUnitList();
};
struct OmsCxCreator_BeforeReqInCore : public OmsCxCreatorInternal {
   fon9_NON_COPY_NON_MOVE(OmsCxCreator_BeforeReqInCore);
   using base = OmsCxCreatorInternal;
   template <class... ArgsT>
   OmsCxCreator_BeforeReqInCore(OmsRequestRunner& runner, ArgsT&&... args) : base(std::forward<ArgsT>(args)...), Runner_(runner) {
   }
   OmsRequestRunner& Runner_;
   void RequestAbandon(OmsErrCode ec, fon9::StrView reason) override {
      this->Runner_.RequestAbandon(&this->OmsResource_, ec, reason.ToString());
   }
};
struct OmsCxCreator_RunInCore : public OmsCxCreatorInternal {
   fon9_NON_COPY_NON_MOVE(OmsCxCreator_RunInCore);
   using base = OmsCxCreatorInternal;
   template <class... ArgsT>
   OmsCxCreator_RunInCore(OmsRequestRunnerInCore& runner, ArgsT&&... args) : base(std::forward<ArgsT>(args)...), Runner_(runner) {
   #if _DEBUG
      auto& ordraw = this->Runner_.OrderRaw();
      assert(ordraw.RequestSt_ == f9fmkt_TradingRequestSt_WaitingCond || ordraw.RequestSt_ == f9fmkt_TradingRequestSt_WaitingCondAtOther);
      assert(ordraw.UpdateOrderSt_ == f9fmkt_OrderSt_NewWaitingCond || ordraw.UpdateOrderSt_ == f9fmkt_OrderSt_NewWaitingCondAtOther);
   #endif
   }
   OmsRequestRunnerInCore& Runner_;
   void RequestAbandon(OmsErrCode ec, fon9::StrView reason) override {
      auto& ordraw = this->Runner_.OrderRaw();
      ordraw.ErrCode_ = ec;
      ordraw.Message_.assign(reason);
   }
};
// ======================================================================== //
bool OmsCxReqBase::InternalCreateCxExpr(OmsCxCreatorInternal& args) {
   if (fon9_UNLIKELY(args.Policy_ == nullptr)) {
      args.RequestAbandon(OmsErrCode_CondSc_Deny);
      return false;
   }
   #ifdef _PRINT_DEBUG_INFO
      printf("\n" "orig  express:\t"  "%s", this->CxArgs_.begin());
   #endif
   auto cxExpr = args.CreateCxExpr();
   if (!cxExpr)
      return false;
   this->CxExpr_      = new CxExpress{std::move(*cxExpr)};
   this->CxArgs_      = std::move(args.CxNormalized_);
   this->FirstCxUnit_ = args.MakeCxUnitList();
   this->CxSymbCount_ = args.CxSymbCount_;
   this->CxUnitCount_ = args.CxUnitCount_;
   // 單一條件才允許改條件內容.
   this->IsAllowChgCond_ = (this->CxUnitCount_ <= 1) && args.IsAllowChgCond_;
   //-----
   #ifdef _PRINT_DEBUG_INFO
      fon9::RevBufferFixedSize<512> rbuf;
      rbuf.RewindEOS();
      this->CxExpr_->RevPrint(rbuf);
      printf("\n" "debug express:\t" "%s", rbuf.GetCurrent());
      printf("\n" " CxNormalized:\t" "%s", this->CxArgs_.cbegin());
      printf("\n" "UnitCount=%u, SymbCount=%u", this->CxUnitCount_, this->CxSymbCount_);
      // -----
      const OmsCxUnit* first = this->FirstCxUnit_;
      do {
         for (const OmsCxUnit* node = first; node != nullptr; node = node->CondSameSymbNext()) {
            rbuf.RewindEOS();
            node->RevPrint(rbuf);
            printf("\n" "(%s)%s", node->CondSymb_->SymbId_.begin(), rbuf.GetCurrent());
         }
         puts("");
         first = first->CondOtherSymbNext();
      } while (first);
   #endif
   //-----
   return true;
}
bool OmsCxReqBase::BeforeReqInCore_CreateCxExpr(OmsRequestRunner& runner, OmsResource& res, const OmsRequestTrade& txReq, OmsSymbSP txSymb) {
   fon9::StrView cxArgsStr{ToStrView(this->CxArgs_)};
   if (fon9_LIKELY(cxArgsStr.empty())) {
      this->CxArgs_.clear();
      return true;
   }
   OmsCxCreator_BeforeReqInCore args(runner, *this, cxArgsStr, res, txReq, std::move(txSymb));
   return this->InternalCreateCxExpr(args);
}
bool OmsCxReqBase::RecreateCxExpr(OmsRequestRunnerInCore& runner, const OmsRequestTrade& txReq) {
   // 不支援重啟條件單: 重啟條件風險太高;
   (void)txReq;
   assert(!this->CxArgs_.empty() && this->CxExpr_ == nullptr);
   auto& ordraw = runner.OrderRaw();
   auto& order = ordraw.Order();
   assert(ordraw.UpdateOrderSt_ == f9fmkt_OrderSt_NewWaitingCond || ordraw.UpdateOrderSt_ == f9fmkt_OrderSt_NewWaitingCondAtOther);
   if (ordraw.UpdateOrderSt_ == f9fmkt_OrderSt_NewWaitingCond || ordraw.UpdateOrderSt_ == f9fmkt_OrderSt_NewWaitingCondAtOther) {
      ordraw.RequestSt_ = f9fmkt_TradingRequestSt_WaitingCond;
      OmsSymbSP txSymb = order.ScResource().Symb_;
      OmsCxCreator_RunInCore args{runner, *this, ToStrView(this->CxArgs_), runner.Resource_, txReq, std::move(txSymb)};
      return this->InternalCreateCxExpr(args);
   }
   ordraw.ErrCode_ = OmsErrCode_RequestNotSupportThisOrder;
   return false;
}
// ======================================================================== //
OmsCxExprUP OmsCxCreatorInternal::CreateCxExpr() {
   if (OmsCxExprUP retval = this->CreateCxExpr(false)) {
      while (retval->IsSingleCx()) {
         assert(retval->Right_.Expr_ == nullptr);
         if (IsEnumContains(retval->CombSt_, OmsCxExpr::CombSt_CurrIsUnit))
            break;
         OmsCxExpr* expr = retval->Curr_.Expr_;
         retval->Curr_.Expr_ = nullptr;
         retval.reset(expr);
      }
      return retval;
   }
   return nullptr;
}
//--------------------------------------------------------------------------//
OmsCxExprUP OmsCxCreatorInternal::CreateCxExpr(bool isWaitRB) {
   OmsCxExprUP thisExpr;
   if (fon9::StrTrimHead(&this->CxArgsStr_).Get1st() == '(') {
      const auto iBrPos = this->CxNormalized_.size();
      this->CxNormalized_.push_back('(');
      this->CxArgsStr_.IncBegin(1);
      OmsCxExprUP currExpr = this->CreateCxExpr(true);
      if (!currExpr)
         return nullptr;
      if (fon9::StrTrimHead(&this->CxArgsStr_).Get1st() != ')') {
         this->RequestAbandon(OmsErrCode_CondSc_BadCondExpress);
         return nullptr;
      }
      this->CxArgsStr_.IncBegin(1);
      // -----
      if (fon9_UNLIKELY(currExpr->IsSingleCx())) {
         this->CxNormalized_.erase(iBrPos, 1);
         thisExpr = std::move(currExpr);
      }
      else {
         this->CxNormalized_.push_back(')');
         thisExpr.reset(new OmsCxExpr);
         thisExpr->Curr_.Expr_ = currExpr.release();
      }
   }
   else {
      if (++this->CxUnitCount_ > this->Policy_->CondExpMaxC()) {
         this->RequestAbandon(OmsErrCode_CondSc_OverExpMaxC);
         return nullptr;
      }
      OmsCxUnitUP currUnit = this->CreateCxUnit();
      if (!currUnit)
         return nullptr;
      thisExpr.reset(new OmsCxExpr);
      thisExpr->Curr_.Unit_ = currUnit.release();
      thisExpr->CombSt_ |= OmsCxExpr::CombSt_CurrIsUnit;
   }
   // -----
   OmsCxExpr::CombSt combOp;
   switch (fon9::StrTrimHead(&this->CxArgsStr_).Get1st()) {
   case '&':   combOp = OmsCxExpr::CombSt_AND; break;
   case '|':   combOp = OmsCxExpr::CombSt_OR;  break;
   case ')':
      if (!isWaitRB)
         goto __BAD_EXPRESS;
      /* fall through */
   case EOF:
      return thisExpr;
   default:
   __BAD_EXPRESS:;
      this->RequestAbandon(OmsErrCode_CondSc_BadCondExpress);
      return nullptr;
   }
   this->CxNormalized_.push_back(*this->CxArgsStr_.begin());
   this->CxArgsStr_.IncBegin(1);
   // -----
   OmsCxExpr* rightExpr;
   if ((thisExpr->Right_.Expr_ = rightExpr = this->CreateCxExpr(isWaitRB).release()) == nullptr)
      return nullptr;
   thisExpr->CombSt_ |= combOp;
   if (rightExpr->IsCxUnit()) {
      thisExpr->CombSt_ |= OmsCxExpr::CombSt_RightIsUnit;
      thisExpr->Right_.Unit_ = rightExpr->Curr_.Unit_;
      rightExpr->Curr_.Unit_ = nullptr;
      delete rightExpr;
   }
   return thisExpr;
}
//--------------------------------------------------------------------------//
OmsCxUnitUP OmsCxCreatorInternal::CreateCxUnit() {
   // 若有多個參數,則使用「,」分隔; 「SymbId.」可省略=下單商品;
   // SymbId.CondName==Value
   // SymbId.BQ>Qty,Pri
   OmsErrCode     ec;
   fon9::StrView  symbId;
   for (const char* cxp = this->CxArgsStr_.begin();;) {
      switch (const char ch = *cxp) {
      case '\0': // 非預期的結束.
         ec = OmsErrCode_CondSc_BadCondName;
      __REQUEST_ABANDON:;
         this->RequestAbandon(ec);
         return nullptr;
      default:
         ++cxp;
         continue;
      case '\'':
      case '"': // 如果商品Id有特殊符號,應使用 ' 或 " 框起來;
         if (!symbId.empty()) {
            ec = OmsErrCode_CondSc_BadCondExpress;
            goto __REQUEST_ABANDON;
         }
         symbId = fon9::SbrFetchInsideNoTrim(this->CxArgsStr_);
         if ((cxp = this->CxArgsStr_.Find('.')) == nullptr) {
            ec = OmsErrCode_CondSc_BadCondExpress;
            goto __REQUEST_ABANDON;
         }
         cxp = fon9::StrFindTrimHead(cxp + 1, this->CxArgsStr_.end());
         this->CxArgsStr_.SetBegin(cxp);
         this->CxNormalized_.push_back(ch);
         this->CxNormalized_.append(symbId);
         this->CxNormalized_.push_back(ch);
         this->CxNormalized_.push_back('.');
         continue;
      case '.':
         if (!symbId.empty()) {
            ec = OmsErrCode_CondSc_BadCondExpress;
            goto __REQUEST_ABANDON;
         }
         symbId.Reset(this->CxArgsStr_.begin(), cxp);
         cxp = fon9::StrFindTrimHead(cxp + 1, this->CxArgsStr_.end());
         this->CxArgsStr_.SetBegin(cxp);
         this->CxNormalized_.append(fon9::StrTrimTail(&symbId));
         this->CxNormalized_.push_back('.');
         continue;
      case ':': // 某些 CondName 不一定會使用 [判斷運算元], 此時 CondOp 為 ':', 後面接參數;
      case '!': // !=
      case '=': // ==
      case '<': // < or <=
      case '>': // > or >=
         fon9::StrView condName{this->CxArgsStr_.begin(), cxp};
         if (fon9_UNLIKELY(!this->Policy_->IsCondAllowed(fon9::StrTrimTail(&condName)))) {
            ec = OmsErrCode_CondSc_Deny;
            goto __REQUEST_ABANDON;
         }
         this->CxNormalized_.append(condName);
         this->Cond_.Name_.AssignFrom(condName);
         // -----
         fon9::StrView condOp;
         if (ch == ':' || *(cxp + 1) != '=')
            condOp.Reset(cxp, cxp + 1);
         else {
            condOp.Reset(cxp, cxp + 2);
            ++cxp;
         }
         this->CxArgsStr_.SetBegin(++cxp);
         this->CxNormalized_.append(condOp);
         this->Cond_.Op_.AssignFrom(condOp);
         this->CompOp_ = f9oms_CondOp_Compare_Parse(this->Cond_.Op_.Chars_);
         this->Cond_.CheckFlags_ = OmsCxCheckFlag{};
         // -----
         if (fon9_LIKELY(symbId.empty()))
            this->CondSymb_ = this->TxSymb_;
         else {
            this->CondSymb_ = OmsGetCrossMarketSymb(this->OmsResource_.Symbs_.get(), symbId);
         }
         if (!this->CondSymb_) {
            ec = OmsErrCode_CondSc_SymbNotFound;
            goto __REQUEST_ABANDON;
         }
         // -----
         if (CxUnitCreator cxuCreator = GetCxUnitCreator(condName)) {
            if (OmsCxUnitUP cxu = cxuCreator(*this)) {
               this->CxUnitList_.push_back(cxu.get());
               return cxu;
            }
            return nullptr;
         }
         ec = OmsErrCode_CondSc_BadCondName;
         goto __REQUEST_ABANDON;
      }
   }
}
//--------------------------------------------------------------------------//
OmsCxUnit* OmsCxCreatorInternal::MakeCxUnitList() {
   // 使用 list 將相同商品的 CxUnit 串起來, 這樣就只要註冊 list.head 即可. (list.head.CondSrcEvMask_ 必須包含整個串列的 CxUnit);
   assert(this->CxUnitList_.size() == this->CxUnitCount_);
   OmsCxUnit* first = nullptr;
   OmsCxUnit* prev = nullptr;
   for (size_t icur = 0;;) {
      ++this->CxSymbCount_;
      OmsCxUnit*     head = this->CxUnitList_[icur];
      auto           evMask = head->Cond_.SrcEvMask_;
      OmsSymb* const symb = head->CondSymb_.get();
      size_t          isymb = 0;
      for (size_t inxt = icur + 1; inxt < this->CxUnitCount_; ++inxt) {
         OmsCxUnit* next = this->CxUnitList_[inxt];
         if (next->CondSameSymbNext_ == nullptr) {
            if (symb == next->CondSymb_.get()) {
               next->CondSameSymbNext_ = head;
               head = next;
               evMask |= next->Cond_.SrcEvMask_;;
            }
            else if (isymb == 0) {
               isymb = inxt;
            }
         }
      }
      head->SetRegEvMask(evMask);
      if (first == nullptr)
         first = head;
      if (prev)
         prev->CondOtherSymbNext_ = head;
      if ((icur = isymb) == 0)
         break;
      prev = head;
   }
   return first;
}
// ======================================================================== //
OmsCxUnit::OmsCxUnit(const OmsCxCreatorArgs& args)
   : Cond_{args.Cond_}
   , CondSymb_{std::move(args.CondSymb_)}
{
}
//--------------------------------------------------------------------------//

} // namespaces
