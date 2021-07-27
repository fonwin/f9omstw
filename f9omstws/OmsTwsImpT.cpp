// \file f9omstws/OmsTwsImpT.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsImpT.hpp"
#include "f9omstw/OmsEventSessionSt.hpp"
#include "f9tws/TwsTools.hpp"

namespace f9omstw {

TwsMarketMask  ImpT30::Ready_;

ImpT30::Loader::Loader(OmsFileImpTree& ownerTree, ImpT30& owner, size_t itemCount)
   : OmsFileImpLoader{ownerTree}
   , Ofs_(owner.Ofs_)
   , ExtLmtRate_(owner.ExtLmtRate_ ? owner.ExtLmtRate_->To<double>() : 0.0)
   , DefaultActMarketPri_(owner.DefaultActMarketPri_ ? *owner.DefaultActMarketPri_ : ActMarketPri::Market) {
   this->ImpItems_.resize(itemCount);
}
ImpT30::Loader::~Loader() {
}
size_t ImpT30::Loader::OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) {
   return this->ParseBlock(pbuf, bufsz, isEOF, this->Ofs_.LnSize_);
}
void ImpT30::Loader::OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) {
   (void)bufsz; (void)isEOF;
   ImpItem& item = this->ImpItems_[this->LnCount_];
   item.StkNo_ = *reinterpret_cast<f9tws::StkNo*>(pbuf);
   item.Refs_.PriUpLmt_ = OmsTwsPri::Make<4>(fon9::StrTo(fon9::StrView{pbuf + this->Ofs_.PriUpLmt_, 9}, 0u));
   item.Refs_.PriRef_   = OmsTwsPri::Make<4>(fon9::StrTo(fon9::StrView{pbuf + this->Ofs_.PriRef_,   9}, 0u));
   item.Refs_.PriDnLmt_ = OmsTwsPri::Make<4>(fon9::StrTo(fon9::StrView{pbuf + this->Ofs_.PriDnLmt_, 9}, 0u));
   if (this->Ofs_.GnDayTrade_ > 0) {
      if (const char ch = pbuf[this->Ofs_.GnDayTrade_])
         item.Refs_.GnDayTrade_ = (ch == ' ' ? GnDayTrade::Reject : static_cast<GnDayTrade>(ch));
   }
   item.Refs_.ActMarketPri_ = this->DefaultActMarketPri_;

   if (item.Refs_.PriDnLmt_ <= OmsTwsPri{1,2}
    && item.Refs_.PriUpLmt_ >= OmsTwsPri{9995,0}) {
      if (this->ExtLmtRate_ == 0.0) // 交易所沒有漲跌停限制, 且沒有設定 ExtLmtRate, 則禁止市價單.
         item.Refs_.ActMarketPri_ = ActMarketPri::Reject;
      else {
         fon9::fmkt::Pri up{10000,0};
         fon9::fmkt::Pri dn{};
         fon9::fmkt::Pri ref{item.Refs_.PriRef_.GetOrigValue(), item.Refs_.PriRef_.Scale};
         CalcLmt(f9tws::TwsSymbKindLvPriStep{item.StkNo_}.LvPriSteps_, ref, this->ExtLmtRate_, &up, &dn);

         if (item.Refs_.ActMarketPri_ == ActMarketPri::Market) {
            // 因交易所沒有漲跌停限制時, 禁止市價單, 所以此處使用 ActMarketPri::Limit,
            // 當市價下單時, 由風控改為漲跌停的限價下單.
            item.Refs_.ActMarketPri_ = ActMarketPri::Limit;
         }
         item.Refs_.PriUpLmt_.Assign(up);
         item.Refs_.PriDnLmt_.Assign(dn);
      }
   }
#ifdef _DEBUG // 檢查 CalcLmt() 計算是否正確.
   else if (this->ExtLmtRate_ != 0) {
      f9tws::TwsSymbKindLvPriStep lv;
      lv.Setup(item.StkNo_);
      // 排除權證的原因: 權證的漲跌停是以標的股漲跌停金額乘以權證行使比例計算，而不是以權證前一交易日的10%為基準。
      // 不排除: 國內成分槓桿及反向ETF:
      //          f9tws_SymbKind_ETF_L, f9tws_SymbKind_ETF_I, f9tws_SymbKind_ETN_L, f9tws_SymbKind_ETN_I
      //        漲跌幅限制則須以原本的10%乘上該ETF的報酬倍數。
      //        就印出這類商品的計算結果吧, 以免沒印東西出來, 好像沒執行檢查!
      if (lv.Kind_ < f9tws_SymbKind_W_BEGIN || f9tws_SymbKind_W_LAST < lv.Kind_) {
         fon9::fmkt::Pri up{10000,0};
         fon9::fmkt::Pri dn{};
         fon9::fmkt::Pri ref{item.Refs_.PriRef_.GetOrigValue(), item.Refs_.PriRef_.Scale};
         CalcLmt(lv.LvPriSteps_, ref, this->ExtLmtRate_, &up, &dn);
         OmsTwsPri dnCalc{fon9::unsigned_cast(dn.GetOrigValue()), dn.Scale};
         OmsTwsPri upCalc{fon9::unsigned_cast(up.GetOrigValue()), up.Scale};
         if ((item.Refs_.PriDnLmt_ != dnCalc || item.Refs_.PriUpLmt_ != upCalc)) {
            puts(fon9::RevPrintTo<std::string>("|StkNo=", item.StkNo_,
                                               "|kind=", lv.Kind_,
                                               "|up=", item.Refs_.PriUpLmt_,
                                               "|dn=", item.Refs_.PriDnLmt_,
                                               "|ref=", item.Refs_.PriRef_,
                                               "|calc.up=", upCalc,
                                               "|calc.dn=", dnCalc
                                               ).c_str());
         }
      }
   }
#endif
}

OmsFileImpLoaderSP ImpT30::MakeLoader(OmsFileImpTree& owner, fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) {
   (void)rbufDesp; (void)monFlag;
   if (auto itemCount = addSize / this->Ofs_.LnSize_)
      return new Loader(owner, *this, itemCount);
   return nullptr;
}
//--------------------------------------------------------------------------//
TwsMarketMask  ImpT32::Ready_;

ImpT32::Loader::Loader(OmsFileImpTree& owner, size_t itemCount, unsigned lnsz)
   : OmsFileImpLoader{owner}
   , LnSize_{lnsz} {
   this->ImpItems_.resize(itemCount);
}
ImpT32::Loader::~Loader() {
}
size_t ImpT32::Loader::OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) {
   return this->ParseBlock(pbuf, bufsz, isEOF, this->LnSize_);
}
void ImpT32::Loader::OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) {
   // StkNo, TradeUnit[5], TradeCurrency[3]
   (void)bufsz; (void)isEOF;
   ImpItem& item = this->ImpItems_[this->LnCount_];
   item.StkNo_ = *reinterpret_cast<f9tws::StkNo*>(pbuf);
   pbuf += f9tws::StkNo::size();
   item.ShUnit_ = fon9::StrTo(fon9::StrView{pbuf, 5}, 0u);
}

OmsFileImpLoaderSP ImpT32::MakeLoader(OmsFileImpTree& owner, fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) {
   (void)rbufDesp; (void)monFlag;
   if (auto itemCount = addSize / this->LnSize_)
      return new Loader(owner, itemCount, this->LnSize_);
   return nullptr;
}
void ImpT32::OnLoadEmptyFile() {
   this->SetReadyFlag(this->Market_);
}
//--------------------------------------------------------------------------//
ImpT33::Loader::Loader(OmsFileImpTree& owner, size_t itemCount, unsigned lnsz)
   : OmsFileImpLoader{owner}
   , LnSize_{lnsz} {
   this->ImpItems_.resize(itemCount);
}
ImpT33::Loader::~Loader() {
}
size_t ImpT33::Loader::OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) {
   return this->ParseBlock(pbuf, bufsz, isEOF, this->LnSize_);
}
void ImpT33::Loader::OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) {
   (void)bufsz; (void)isEOF;
   ImpItem&  item = this->ImpItems_[this->LnCount_];
   item.StkNo_ = *reinterpret_cast<f9tws::StkNo*>(pbuf);
   item.PriFixed_ = OmsTwsPri::Make<4>(fon9::StrTo(fon9::StrView{pbuf + 6, 9}, 0u));
}

OmsFileImpLoaderSP ImpT33::MakeLoader(OmsFileImpTree& owner, fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) {
   (void)rbufDesp; (void)monFlag;
   if (auto itemCount = addSize / this->LnSize_)
      return new Loader(owner, itemCount, this->LnSize_);
   return nullptr;
}
void ImpT33::FireEvent_SessionNormalClosed(OmsCore& core) const {
   if (auto* evfac = dynamic_cast<OmsEventSessionStFactory*>(core.Owner_->EventFactoryPark().GetFactory(f9omstw_kCSTR_OmsEventSessionStFactory_Name))) {
      core.EventToCore(evfac->MakeEvent(fon9::UtcNow(), this->Market_, f9fmkt_TradingSessionId_Normal, f9fmkt_TradingSessionSt_Closed));
   }
}

} // namespaces
