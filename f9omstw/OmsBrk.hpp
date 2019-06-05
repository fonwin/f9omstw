// \file f9omstw/OmsBrk.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsBrk_hpp__
#define __f9omstw_OmsBrk_hpp__
#include "f9omstw/OmsIvac.hpp"
#include "f9omstw/OmsOrdNoMap.hpp"

namespace f9omstw {

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4582); // 'f9omstw::OmsMarketRec::SessionAry_': constructor is not implicitly called
class OmsSessionRec {
   fon9_NON_COPY_NON_MOVE(OmsSessionRec);
   // 可各自獨立, 也可能會共用, 例如: [上市][盤中] = [上市][定價] = [上市][零股].
   // - 台灣證券環境:「整股」、「定價」、「零股」應使用相同委託書號表.
   // - 台灣期權環境:「日盤」、「夜盤」可以使用不同委託書號表,
   //   但因「盤後」帳務屬於「次交易日」, 所以強烈建議「日盤、盤後」應使用不同櫃號.
   OmsOrdNoMapSP  OrdNoMap_;
public:
   OmsMarketRec&                 Market_;
   const f9fmkt_TradingSessionId SessionId_;

   OmsSessionRec(OmsMarketRec& mkt, f9fmkt_TradingSessionId ses) : Market_(mkt), SessionId_{ses} {
   }

   OmsOrdNoMapSP InitializeOrdNoMap();
   OmsOrdNoMapSP InitializeOrdNoMapRef(OmsOrdNoMapSP ordNoMap);
   OmsOrdNoMap* GetOrdNoMap() {
      return this->OrdNoMap_.get();
   }
   const OmsOrdNoMap* GetOrdNoMap() const {
      return this->OrdNoMap_.get();
   }
};

class OmsMarketRec {
   fon9_NON_COPY_NON_MOVE(OmsMarketRec);
   using SessionAry = std::array<OmsSessionRec, f9fmkt_TradingSessionId_MaxIndex + 1u>;
   union { // OmsSessionRec 沒有預設建構, OmsMarketRec() 建構時初始化 SessionAry_;
      SessionAry  SessionAry_;
   };
   void InitializeSessionAry();

public:
   OmsBrk&                    Brk_;
   const f9fmkt_TradingMarket Market_;

   OmsMarketRec(OmsBrk& brk, f9fmkt_TradingMarket mkt) :Brk_(brk), Market_{mkt} {
      this->InitializeSessionAry();
   }
   ~OmsMarketRec();

   OmsSessionRec& GetSession(f9fmkt_TradingSessionId sesid) {
      return this->SessionAry_[f9fmkt_TradingSessionId_ToIndex(sesid)];
   }
   const OmsSessionRec& GetSession(f9fmkt_TradingSessionId sesid) const {
      return this->SessionAry_[f9fmkt_TradingSessionId_ToIndex(sesid)];
   }
};

// 不同的 OMS 實現, 必須定義自己需要的 class UtwsBrk; 或 UtwfBrk;
/// 這裡做為底層提供:
/// - 委託書號對照表: Market + Session + OrdNo
/// - 投資人帳號資料: OmsIvacMap
class OmsBrk : public OmsIvBase {
   fon9_NON_COPY_NON_MOVE(OmsBrk);
   using base = OmsIvBase;
   void ClearOrdNoMap();

   using MarketAry = std::array<OmsMarketRec, f9fmkt_TradingMarket_MaxIndex + 1u>;
   union { // OmsMarketRec 沒有預設建構, OmsBrk() 建構時初始化 MarketAry_;
      MarketAry   MarketAry_;
   };
   void InitializeMarketAry();

protected:
   OmsIvacMap Ivacs_;

   virtual OmsIvacSP MakeIvac(IvacNo) = 0;

public:
   const fon9::CharVector  BrkId_;
   OmsBrk(const fon9::StrView& brkid) : base(OmsIvKind::Brk, nullptr), BrkId_(brkid) {
      this->InitializeMarketAry();
   }
   ~OmsBrk();

   // 除非 ivac->IvacNo_ 檢查碼有錯, 或確實無此帳號, 否則不應刪除 ivac, 因為:
   // - 若已有正確委託, 則該委託風控異動時, 仍會使用移除前的 OmsIvacSP.
   // - 若之後又建立一筆相同 IvacNo 的資料, 則會與先前移除的 OmsIvacSP 不同!
   OmsIvacSP RemoveIvac(IvacNo ivacNo);
   OmsIvac* FetchIvac(IvacNo ivacNo);
   OmsIvac* GetIvac(IvacNC ivacNC) const {
      if (auto* p = this->Ivacs_.Get(ivacNC))
         return p->get();
      return nullptr;
   }
   OmsIvac* GetIvac(IvacNo ivacNo) const {
      if (auto* p = this->Ivacs_.Get(IvacNoToNC(ivacNo)))
         if (auto* ivac = p->get())
            if (ivac->IvacNo_ == ivacNo)
               return ivac;
      return nullptr;
   }

   OmsMarketRec& GetMarket(f9fmkt_TradingMarket mkt) {
      return this->MarketAry_[f9fmkt_TradingMarket_ToIndex(mkt)];
   }
   const OmsMarketRec& GetMarket(f9fmkt_TradingMarket mkt) const {
      return this->MarketAry_[f9fmkt_TradingMarket_ToIndex(mkt)];
   }
   OmsOrdNoMap* GetOrdNoMap(const fon9::fmkt::TradingRequest& req) {
      return this->GetMarket(req.Market()).GetSession(req.SessionId()).GetOrdNoMap();
   }
   const OmsOrdNoMap* GetOrdNoMap(const fon9::fmkt::TradingRequest& req) const {
      return this->GetMarket(req.Market()).GetSession(req.SessionId()).GetOrdNoMap();
   }

   virtual void OnParentSeedClear();
   virtual void OnDailyClear();

   /// 建立 grid view, 包含 BrkId_; 不含尾端分隔符號.
   virtual void MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) = 0;
   virtual void OnPodOp(OmsBrkTree& brkTree, fon9::seed::FnPodOp&& fnCallback) = 0;
};
fon9_WARN_POP;

inline OmsBrk* GetBrk(OmsIvBase* ivr) {
   if (ivr)
      switch (ivr->IvKind_) {
      case OmsIvKind::Unknown:   break;
      case OmsIvKind::Subac:     return static_cast<OmsBrk*>(ivr->Parent_->Parent_.get());
      case OmsIvKind::Ivac:      return static_cast<OmsBrk*>(ivr->Parent_.get());
      case OmsIvKind::Brk:       return static_cast<OmsBrk*>(ivr);
      }
   return nullptr;
}

//--------------------------------------------------------------------------//

inline OmsIvac::OmsIvac(IvacNo ivacNo, OmsBrkSP parent)
   : base(OmsIvKind::Ivac, std::move(parent))
   , IvacNo_{ivacNo} {
}

} // namespaces
#endif//__f9omstw_OmsBrk_hpp__
