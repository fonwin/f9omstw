// \file f9omstws/OmsTwsImpT.cpp
// \author fonwinz@gmail.com
#include "f9omstws/OmsTwsImpT.hpp"
#include "f9omstw/OmsEventSessionSt.hpp"

namespace f9omstw {

ImpT30::Loader::Loader(OmsCoreSP core, size_t itemCount, ImpT30& owner)
   : OmsFileImpLoader{core}
   , Ofs_(owner.Ofs_) {
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
   item.PriRefs_.PriUpLmt_ = OmsTwsPri::Make<4>(fon9::StrTo(fon9::StrView{pbuf + this->Ofs_.PriUpLmt_, 9}, 0u));
   item.PriRefs_.PriRef_   = OmsTwsPri::Make<4>(fon9::StrTo(fon9::StrView{pbuf + this->Ofs_.PriRef_,   9}, 0u));
   item.PriRefs_.PriDnLmt_ = OmsTwsPri::Make<4>(fon9::StrTo(fon9::StrView{pbuf + this->Ofs_.PriDnLmt_, 9}, 0u));
}

fon9::seed::FileImpLoaderSP ImpT30::OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) {
   (void)rbufDesp; (void)monFlag;
   if (auto itemCount = addSize / this->Ofs_.LnSize_)
      return new Loader(static_cast<OmsFileImpTree*>(&this->OwnerTree_)->CoreMgr_.CurrentCore(), itemCount, *this);
   return nullptr;
}
//--------------------------------------------------------------------------//
ImpT32::Loader::Loader(OmsCoreSP core, size_t itemCount, unsigned lnsz)
   : OmsFileImpLoader{core}
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

fon9::seed::FileImpLoaderSP ImpT32::OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) {
   (void)rbufDesp; (void)monFlag;
   if (auto itemCount = addSize / this->LnSize_)
      return new Loader(static_cast<OmsFileImpTree*>(&this->OwnerTree_)->CoreMgr_.CurrentCore(), itemCount, this->LnSize_);
   return nullptr;
}
//--------------------------------------------------------------------------//
ImpT33::Loader::Loader(OmsCoreSP core, size_t itemCount, unsigned lnsz)
   : OmsFileImpLoader{core}
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

fon9::seed::FileImpLoaderSP ImpT33::OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, fon9::seed::FileImpMonitorFlag monFlag) {
   (void)rbufDesp; (void)monFlag;
   if (auto itemCount = addSize / this->LnSize_)
      return new Loader(static_cast<OmsFileImpTree*>(&this->OwnerTree_)->CoreMgr_.CurrentCore(), itemCount, this->LnSize_);
   return nullptr;
}
void ImpT33::FireEvent_SessionNormalClosed(OmsCore& core) const {
   if (auto* evfac = dynamic_cast<OmsEventSessionStFactory*>(core.Owner_->EventFactoryPark().GetFactory(f9omstw_kCSTR_OmsEventSessionStFactory_Name))) {
      core.EventToCore(evfac->MakeEvent(fon9::UtcNow(), this->Market_, f9fmkt_TradingSessionId_Normal, f9fmkt_TradingSessionSt_Closed));
   }
}

} // namespaces
