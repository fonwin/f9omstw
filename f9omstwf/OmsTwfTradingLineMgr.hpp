// \file f9omstwf/OmsTwfTradingLineMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfTradingLineMgr_hpp__
#define __f9omstwf_OmsTwfTradingLineMgr_hpp__
#include "f9twf/ExgTradingLineMgr.hpp"
#include "f9omstw/OmsTradingLineMgr.hpp"

namespace f9omstw {

class TwfTradingLineMgr : public OmsTradingLineMgrT<f9twf::ExgTradingLineMgr> {
   fon9_NON_COPY_NON_MOVE(TwfTradingLineMgr);
   using base = OmsTradingLineMgrT<f9twf::ExgTradingLineMgr>;
   OmsReqUID_Builder ReqUIDBuilder_;
public:
   using base::base;

   void ExgOrdIdToReqUID(const f9twf::TmpOrdId& ordid, OmsRequestId& requid) {
      this->ReqUIDBuilder_.MakeReqUID(requid, f9twf::TmpGetValueU(ordid));
   }
};
using TwfTradingLineMgrSP = fon9::intrusive_ptr<TwfTradingLineMgr>;

class TwfTradingLineGroup {
   fon9_NON_COPY_NON_MOVE(TwfTradingLineGroup);
public:
   TwfTradingLineMgrSP  TradingLineMgr_[f9twf::ExgSystemTypeCount()];
   using LineMgr = TwfTradingLineMgr;

   TwfTradingLineGroup() = default;
   ~TwfTradingLineGroup();

   void OnTDayChangedInCore(OmsResource& resource);

   /// 根據 runner.OrderRaw_.SessionId() 及 runner.OrderRaw_.Market() 取得 TwfTradingLineMgr;
   /// 若返回 nullptr, 則返回前, 會先執行 runner.Reject();
   TwfTradingLineMgr* GetLineMgr(OmsRequestRunnerInCore& runner) const;
};

/// 會在 owner 加入 2 個 seed:
/// - (ioargs.Name_ + "_io"): TwfTradingLineMgr
///   - 使用 cfgpath + ioargs.Name_ + "_io" + ".f9gv" 綁定設定檔.
/// - (ioargs.Name_ + "_cfg"): TwfTradingLineMgrCfgSeed
///   - 使用 cfgpath + ioargs.Name_ + "_cfg" + ".f9gv" 綁定設定檔.
/// - 如果名稱重複, 則傳回 nullptr;
TwfTradingLineMgrSP CreateTwfTradingLineMgr(fon9::seed::MaTree&  owner,
                                            std::string          cfgpath,
                                            fon9::IoManagerArgs& ioargs,
                                            f9twf::ExgMapMgrSP   exgMapMgr,
                                            f9twf::ExgSystemType exgSysType);

} // namespaces
#endif//__f9omstwf_OmsTwfTradingLineMgr_hpp__
