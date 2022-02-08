// \file f9omstwf/OmsTwfRequest0.hpp
//
// TaiFex 下單要求(一般、詢價、報價)基底.
//
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfRequest0_hpp__
#define __f9omstwf_OmsTwfRequest0_hpp__
#include "f9omstw/OmsRequestTrade.hpp"
#include "f9omstw/OmsSymb.hpp"
#include "f9omstwf/OmsTwfTypes.hpp"

namespace f9omstw {

enum class RequestType : char {
   /// 一般委託.
   Normal,
   /// 詢價.
   QuoteR = 'R',
   /// 報價
   Quote = 'Q',
};

/// 「一般委託、詢價、報價」的共用欄位.
struct OmsTwfRequestIniDat0 {
   fon9_NON_COPY_NON_MOVE(OmsTwfRequestIniDat0);
   OmsTwfRequestIniDat0(RequestType rtype) : RequestType_{rtype} {
      memset(const_cast<RequestType*>(&this->RequestType_ + 1), 0, sizeof(*this) - sizeof(this->RequestType_));
   }

   /// 應由衍生者填入固定值.
   /// 下單線路根據此值決定實際用哪種 OmsTwfRequestIni:
   /// OmsTwfRequestIni1(一般交易); OmsTwfRequestIni7(詢價); OmsTwfRequestIni9(報價);
   const RequestType    RequestType_;

   /// 在 OmsTwfRequestIni0::BeforeReq_CheckOrdKey() 設定此值.
   f9twf::ExgCombSide   CombSide_;
   f9twf::TmpCombOp     CombOp_;

   OmsTwfSymbol         Symbol_;
   f9twf::TmpSource     TmpSource_;
};

class OmsTwfRequestIni0 : public OmsRequestIni, public OmsTwfRequestIniDat0 {
   fon9_NON_COPY_NON_MOVE(OmsTwfRequestIni0);
   using base = OmsRequestIni;
   OmsSymbSP   SymbLeg2_;

   using OmsTwfRequestIniDat0::CombSide_;
   using OmsTwfRequestIniDat0::CombOp_;

public:
   template <class... ArgsT>
   OmsTwfRequestIni0(RequestType rtype, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , OmsTwfRequestIniDat0{rtype} {
   }

   static void MakeFields(fon9::seed::Fields& flds);

   const char* IsIniFieldEqual(const OmsRequestBase& req) const override;

   /// - 設定 scRes.Symb_; 若為複式單, 則會設定 this->SymbLeg2_;
   /// - 如果沒填 SessionId, 則填入 scRes.Symb_->TradingSessionId_ 判斷: 日盤or夜盤:
   const OmsRequestIni* BeforeReq_CheckOrdKey(OmsRequestRunner& runner, OmsResource& res, OmsScResource& scRes) override;

   /// 使用 this->Symbol_ 取得 symb;
   /// - 如果是複式單, 則會重設 this->SymbLeg2(); this->CombOp_; this->CombSide_;
   /// - this->BeforeReq_CheckOrdKey(); 會呼叫此處.
   /// - 而「回報」、「系統重啟」會在 OmsOrder::GetSymb() 時, 透過 OmsOrder::FindSymb() 呼叫次處.
   OmsSymbSP RegetSymb(OmsResource& res);

   /// 在使用 SymbLeg2(), CombSide_, CombOp_ 之前,
   /// 必須先確定已呼叫過 order.GetSymb(); 否則可能無法正確取得[複式商品]資訊.
   OmsSymb* UnsafeSymbLeg2() const {
      return this->SymbLeg2_.get();
   }
   f9twf::ExgCombSide UnsafeCombSide() const {
      return this->CombSide_;
   }
   f9twf::TmpCombOp UnsafeCombOp() const {
      return this->CombOp_;
   }
   f9twf::ExgCombSide CombSide(OmsResource& res) const;
   f9twf::TmpCombOp CombOp(OmsResource& res) const;
};

} // namespaces
#endif//__f9omstwf_OmsTwfRequest0_hpp__
