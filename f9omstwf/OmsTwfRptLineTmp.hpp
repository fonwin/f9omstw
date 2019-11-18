// \file f9omstwf/OmsTwfRptLineTmp.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfRptLineTmp_hpp__
#define __f9omstwf_OmsTwfRptLineTmp_hpp__
#include "f9twf/ExgLineTmpSession.hpp"
#include "f9omstwf/OmsTwfReport2.hpp"
#include "f9omstwf/OmsTwfReport3.hpp"
#include "f9omstwf/OmsTwfReport8.hpp"
#include "f9omstwf/OmsTwfReport9.hpp"
#include "f9omstwf/OmsTwfFilled.hpp"

namespace f9omstw {

class TwfLineTmpWorker {
   fon9_NON_COPY_NON_MOVE(TwfLineTmpWorker);

public:
   OmsCoreMgr&           CoreMgr_;

   OmsTwfReport2Factory& Rpt2Factory_;
   OmsTwfReport8Factory& Rpt8Factory_;
   OmsTwfReport9Factory& Rpt9Factory_;

   OmsTwfFilled1Factory& Fil1Factory_;
   OmsTwfFilled2Factory& Fil2Factory_;

   /// - 期交所 R03 沒有提供大部分的必要欄位(商品、帳號...),
   ///   - 無法判斷此筆失敗是屬於: 報價? 詢價? 一般下單?
   ///   - 因此無法根據 R03 建立正確的委託書(報價? 詢價? 一般下單?)
   /// - 所以只能處理「已存在的下單要求」, 因此:
   ///   - 沒有依欄位順序寫入 omslog 的需求.
   ///   - Rpt3Factory 不會加入 OmsCoreMgr.
   /// - 所以在這裡提供一個公用的 Rpt3Factory_;
   /// - 但是仍然可以在 coreMgr 裡面加入 OmsTwfReport3Factory.
   ///   例如: 在單元測試時需要用到.
   OmsTwfReport3Factory  Rpt3Factory_;

   TwfLineTmpWorker(OmsCoreMgr&           coreMgr,
                    OmsTwfReport2Factory& rpt2Factory,
                    OmsTwfReport8Factory& rpt8Factory,
                    OmsTwfReport9Factory& rpt9Factory,
                    OmsTwfFilled1Factory& fil1Factory,
                    OmsTwfFilled2Factory& fil2Factory)
      : CoreMgr_(coreMgr)
      , Rpt2Factory_(rpt2Factory)
      , Rpt8Factory_(rpt8Factory)
      , Rpt9Factory_(rpt9Factory)
      , Fil1Factory_(fil1Factory)
      , Fil2Factory_(fil2Factory)
      , Rpt3Factory_("TwfRpt3", nullptr) {
   }
};

//--------------------------------------------------------------------------//

/// 可收回報的線路: TMPDC, 交易線路.
class TwfRptLineTmp : public f9twf::ExgLineTmpSession {
   fon9_NON_COPY_NON_MOVE(TwfRptLineTmp);
   using base = f9twf::ExgLineTmpSession;

protected:
   void OnExgTmp_ApPacket(const f9twf::TmpHeader& pktmp) override;
   void OnExgTmp_ApReady() override;
   void OnExgTmp_ApBroken() override;

public:
   TwfLineTmpWorker&   Worker_;

   TwfRptLineTmp(TwfLineTmpWorker&            worker,
                 f9twf::ExgTradingLineMgr&    lineMgr,
                 const f9twf::ExgLineTmpArgs& lineArgs,
                 f9twf::ExgLineTmpLog&&       log);
};

} // namespaces
#endif//__f9omstwf_OmsTwfRptLineTmp_hpp__
