// \file f9omstwf/OmsTwfRptFromB50.hpp
//
// B50 匯入程序: 收盤後未成交，視為期交所刪單。
//
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfRptFromB50_hpp__
#define __f9omstwf_OmsTwfRptFromB50_hpp__
#include "fon9/io/Session.hpp"
#include "f9twf/ExgTmpTradingR1.hpp"
#include "f9twf/ExgMapMgr.hpp"
#include "f9omstwf/OmsTwfFilled.hpp"
#include "fon9/framework/IoFactory.hpp"

namespace f9omstw {

class TwfRptFromB50 : public fon9::io::Session {
   fon9_NON_COPY_NON_MOVE(TwfRptFromB50);
   using base = fon9::io::Session;
   enum {
      kPkMinSize = sizeof(f9twf::TmpB50R02),
      kPkMaxSize = sizeof(f9twf::TmpB50R32),
   };
   unsigned PkBufOfs_;
   unsigned PkSize_;
   unsigned PkCount_;
   char     PkBuffer_[kPkMaxSize];
   char     Padding___[3];
   const char* FeedBuffer(fon9::DcQueue& rxbuf);
   fon9::io::RecvBufferSize OnDevice_LinkReady(fon9::io::Device& dev) override;
   fon9::io::RecvBufferSize OnDevice_Recv(fon9::io::Device& dev, fon9::DcQueue& rxbuf) override;
   void OnDevice_CommonTimer(fon9::io::Device& dev, fon9::TimeStamp now) override;

public:
   const f9twf::ExgSystemType ExgSystemType_;
   const f9twf::ExgMapMgrSP   ExgMapMgr_;
   OmsCoreMgr&                CoreMgr_;
   OmsTwfFilled1Factory&      Fil1Factory_;

   TwfRptFromB50(OmsCoreMgr& coreMgr, OmsTwfFilled1Factory& twfFilFactory,
                 f9twf::ExgMapMgrSP exgMapMgr, f9twf::ExgSystemType sysType);
};
//--------------------------------------------------------------------------//
class OmsRptFromB50_SessionFactory : public fon9::SessionFactory {
   fon9_NON_COPY_NON_MOVE(OmsRptFromB50_SessionFactory);
   using base = fon9::SessionFactory;
public:
   const f9twf::ExgMapMgrSP   ExgMapMgr_;
   const OmsCoreMgrSP         CoreMgr_;
   OmsTwfFilled1Factory&      Fil1Factory_;

   OmsRptFromB50_SessionFactory(std::string           name,
                                OmsCoreMgrSP          coreMgr,
                                OmsTwfFilled1Factory& twfFilFactory,
                                f9twf::ExgMapMgrSP    exgMapMgr)
      : base(std::move(name))
      , ExgMapMgr_(std::move(exgMapMgr))
      , CoreMgr_(std::move(coreMgr))
      , Fil1Factory_(twfFilFactory) {
   }
   ~OmsRptFromB50_SessionFactory();

   fon9::io::SessionSP CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& iocfg, std::string& errReason) override;
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager& mgr, const fon9::IoConfigItem& iocfg, std::string& errReason) override;
};

} // namespaces
#endif//__f9omstwf_OmsTwfRptFromB50_hpp__
