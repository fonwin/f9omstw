// \file f9omstwf/OmsTwfRptFromB50.cpp
// \author fonwinz@gmail.com
#include "f9omstwf/OmsTwfRptFromB50.hpp"
#include "f9omstwf/OmsTwfRptTools.hpp"
#include "f9omstw/OmsCoreMgr.hpp"

namespace f9omstw {

TwfRptFromB50::TwfRptFromB50(OmsCoreMgr& coreMgr, OmsTwfFilled1Factory& twfFilFactory,
                             f9twf::ExgMapMgrSP exgMapMgr, f9twf::ExgSystemType sysType)
   : ExgSystemType_(sysType)
   , ExgMapMgr_(std::move(exgMapMgr))
   , CoreMgr_(coreMgr)
   , Fil1Factory_(twfFilFactory) {
}
fon9::io::RecvBufferSize TwfRptFromB50::OnDevice_LinkReady(fon9::io::Device& dev) {
   (void)dev;
   this->PkBufOfs_ = this->PkCount_ = 0;
   return fon9::io::RecvBufferSize::Default;
}
static inline unsigned GetB50PkSize(char msgType) {
   return static_cast<unsigned>(msgType == fon9::cast_to_underlying(f9twf::TmpMessageType_R(02))
                                ? sizeof(f9twf::TmpB50R02)
                                : sizeof(f9twf::TmpB50R32));
}
const char* TwfRptFromB50::FeedBuffer(fon9::DcQueue& rxbuf) {
   if (this->PkBufOfs_) {
      size_t sz = this->PkSize_ - this->PkBufOfs_;
      this->PkBufOfs_ += static_cast<unsigned>(rxbuf.Read(this->PkBuffer_ + this->PkBufOfs_, sz));
      if (this->PkBufOfs_ < this->PkSize_) {
         assert(rxbuf.empty());
         return nullptr;
      }
      assert(this->PkBufOfs_ == this->PkSize_);
      this->PkBufOfs_ = 0;
      ++this->PkCount_;
      return this->PkBuffer_;
   }
   const char* pbuf = static_cast<const char*>(rxbuf.Peek(this->PkBuffer_, kPkMinSize));
   if (pbuf == nullptr) {
      this->PkBufOfs_ = static_cast<unsigned>(rxbuf.Read(this->PkBuffer_, kPkMaxSize));
      if (this->PkBufOfs_ > 0)
         this->PkSize_ = GetB50PkSize(this->PkBuffer_[0]);
      assert(rxbuf.empty() && this->PkBufOfs_ < kPkMaxSize);
      return nullptr;
   }
   ++this->PkCount_;
   this->PkSize_ = GetB50PkSize(pbuf[0]);
   return pbuf;
}
//--------------------------------------------------------------------------//
fon9::io::RecvBufferSize TwfRptFromB50::OnDevice_Recv(fon9::io::Device& dev, fon9::DcQueue& rxbuf) {
   (void)dev;
   OmsCoreSP omsCore = this->CoreMgr_.CurrentCore();
   if (!omsCore)
      return fon9::io::RecvBufferSize::Default;
   while (auto* const pkBuf = this->FeedBuffer(rxbuf)) {
      // 使用 TwfFil1.Qty_ = 0; QtyCanceled_ = tmp.LeavesQty_; 的方式回報;
      const f9twf::TmpR1BfSym* pkBfSym;
      const f9twf::TmpR2BackNoCheckSum* pkAfSym;
      switch (static_cast<uint8_t>(pkBuf[0])) {
      case static_cast<uint8_t>(f9twf::TmpMessageType_R(02)):
         pkBfSym = reinterpret_cast<const f9twf::TmpB50R02*>(pkBuf);
         pkAfSym = reinterpret_cast<const f9twf::TmpB50R02*>(pkBuf);
         break;
      default:
      case static_cast<uint8_t>(f9twf::TmpMessageType_R(32)):
         pkBfSym = reinterpret_cast<const f9twf::TmpB50R32*>(pkBuf);
         pkAfSym = reinterpret_cast<const f9twf::TmpB50R32*>(pkBuf);
         break;
      }
      OmsRequestRunner runner{fon9::StrView{pkBuf, this->PkSize_}};
      runner.Request_ = this->Fil1Factory_.MakeReportIn(f9fmkt_RxKind_Filled, fon9::UtcNow());
      assert(dynamic_cast<OmsTwfFilled*>(runner.Request_.get()) != nullptr);
      OmsTwfFilled& rpt = *static_cast<OmsTwfFilled*>(runner.Request_.get());
      rpt.OrdNo_ = pkBfSym->OrderNo_;
      rpt.PriStyle_ = OmsReportPriStyle::NoDecimal;
      rpt.PosEff_ = f9omstw::OmsTwfPosEff{};
      rpt.IvacNo_ = f9twf::TmpGetValueU(pkAfSym->IvacNo_);
      rpt.Time_ = rpt.CrTime(); // pkAfSym->TransactTime_.ToTimeStamp();
      rpt.Qty_ = 0;
      rpt.QtyCanceled_ = f9twf::TmpGetValueU(pkAfSym->LeavesQty_);
      rpt.SetErrCode(OmsErrCode_SessionClosedCanceled);
      rpt.MatchKey_ = f9twf::TmpGetValueU(pkAfSym->UniqId_)
                    + 9000000000ull;
      SetupReport0Symbol(*runner.Request_, pkBfSym->IvacFcmId_, &pkBfSym->SymbolType_, rpt.Symbol_,
                         this->ExgMapMgr_->Lock(), this->ExgSystemType_);
      rpt.IsAbandonOrderNotFound_ = true;
      omsCore->MoveToCore(std::move(runner));
      // -----
      if (pkBuf != this->PkBuffer_) {
         assert(pkBuf == reinterpret_cast<const char*>(rxbuf.Peek1()));
         rxbuf.PopConsumed(this->PkSize_);
      }
   }
   dev.CommonTimerRunAfter(fon9::TimeInterval_Second(1));
   return fon9::io::RecvBufferSize::Default;
}
void TwfRptFromB50::OnDevice_CommonTimer(fon9::io::Device& dev, fon9::TimeStamp now) {
   (void)now;
   if (dev.Manager_) {
      dev.Manager_->OnSession_StateUpdated(dev,
            ToStrView(fon9::RevPrintTo<fon9::CharVector>("PkCount=", this->PkCount_)),
            fon9::LogLevel::Info);
   }
}
//--------------------------------------------------------------------------//
OmsRptFromB50_SessionFactory::~OmsRptFromB50_SessionFactory() {
}
fon9::io::SessionSP OmsRptFromB50_SessionFactory::CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& iocfg, std::string& errReason) {
   (void)mgr;
   f9twf::ExgSystemType sysType{};
   fon9::StrView cfg = ToStrView(iocfg.SessionArgs_);
   fon9::StrView tag, value;
   while (fon9::StrFetchTagValue(cfg, tag, value)) {
      if (tag == "SysType")
         sysType = static_cast<f9twf::ExgSystemType>(fon9::StrTo(value, 0u));
      else {
         errReason = tag.ToString("Unknown tag:");
         return nullptr;
      }
   }

   switch (sysType) {
   case f9twf::ExgSystemType::FutNormal:
   case f9twf::ExgSystemType::OptNormal:
   case f9twf::ExgSystemType::OptAfterHour:
   case f9twf::ExgSystemType::FutAfterHour:
      break;
   default:
      errReason = "Bad 'SysType' value";
      return nullptr;
   }
   return new TwfRptFromB50(*this->CoreMgr_, this->Fil1Factory_, this->ExgMapMgr_, sysType);
}
fon9::io::SessionServerSP OmsRptFromB50_SessionFactory::CreateSessionServer(fon9::IoManager& mgr, const fon9::IoConfigItem& iocfg, std::string& errReason) {
   (void)mgr; (void)iocfg;
   errReason = "FromB50: Server not supported";
   return nullptr;
}

} // namespaces
