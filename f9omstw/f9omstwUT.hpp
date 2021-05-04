// \file f9omstw/f9omstwUT.hpp
//
// f9omstw 的單元測試.
//
// \author fonwinz@gmail.com
#ifndef __f9omstw_f9omstwUT_hpp__
#define __f9omstw_f9omstwUT_hpp__
#include "f9omstw/OmsCoreMgrSeed.hpp"
#include "fon9/framework/SeedSession.hpp"

namespace f9omstw {

extern fon9::StrView gBrkId; // "8610"
extern unsigned      gBrkCount;
extern unsigned      gLineNo;

#define kCSTR_DefaultLogPathFmt  "./logs/{0:f+'L'}/"
extern std::string gLogPathFmt;

//--------------------------------------------------------------------------//
fon9::RevBufferList CompareFieldValues(fon9::StrView fldvals, const fon9::seed::Fields& flds, const fon9::seed::RawRd& rd, char chSpl);

//--------------------------------------------------------------------------//
class OmsRequestStep_Send : public OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(OmsRequestStep_Send);
   using base = OmsRequestRunStep;
public:
   OmsRequestStep_Send() = default;
   /// 直接設定委託狀態為 Sending:
   /// runner.Update(f9fmkt_TradingRequestSt_Sending, "Sending");
   void RunRequest(f9omstw::OmsRequestRunnerInCore&& runner) override;
};

//--------------------------------------------------------------------------//
class OmsUtConsoleSeedSession : public fon9::SeedSession {
   fon9_NON_COPY_NON_MOVE(OmsUtConsoleSeedSession);
   using base = fon9::SeedSession;
   fon9::CountDownLatch Waiter_{1u};

   void OnAuthEventInLocking(State st, fon9::DcQueue&& msg) override;
   void OnRequestError(const fon9::seed::TicketRunner&, fon9::DcQueue&& errmsg) override;
   void OnRequestDone(const fon9::seed::TicketRunner&, fon9::DcQueue&& extmsg) override;
   uint16_t GetDefaultGridViewRowCount() override;

   /// add:
   ///   chks,field=value,...    seed
   RequestSP OnUnknownCommand(fon9::seed::SeedFairy::Request& req) override;

public:
   // gcc 不喜歡我直接使用 using base::base; 所以只好再寫一次了.
   OmsUtConsoleSeedSession(fon9::seed::MaTreeSP root, fon9::auth::AuthMgrSP authMgr, std::string ufrom)
      : base(std::move(root), std::move(authMgr), std::move(ufrom)) {
   }

   void WriteToConsole(fon9::DcQueue&& errmsg);
   void Wakeup();
   void Wait();
};
using OmsUtConsoleSeedSessionSP = fon9::intrusive_ptr<OmsUtConsoleSeedSession>;

//--------------------------------------------------------------------------//
/// 由衍生者實現對應的初始化作業.
class OmsCoreUT {
   fon9_NON_COPY_NON_MOVE(OmsCoreUT);

   fon9::intrusive_ptr<const OmsSymbTree> PrvSymbs_;
   fon9::intrusive_ptr<const OmsBrkTree>  PrvBrks_;

   /// 在 AddCore() 時, 必須將需要的資料表(例如: Brks,Ivac,Symb...) 加入 OmsCore;
   virtual void InitCoreTables(OmsResource& res) = 0;

protected:
   // 在正式環境, reqLast, ordraw 必須在 OmsCore thread 處理.
   // 這裡為驗證處理流程(不用太在乎速度),
   // 所以使用 omsCoreUT.WaitCoreDone(); 等後處理完畢後才使用 reqLast, ordraw.
   // 且不會有其他 thread 來競爭, 所以才能在 main thread 使用!
   OmsRequestSP   ReqLast_;

   /// 下單要求權限, 預設為 admin, 可任意委託書號.
   OmsRequestPolicySP   PoRequest_{new OmsRequestPolicy};

   /// 在此預設複製上次[帳號]、[庫存]、[商品] 基本資料.
   void RestoreCoreTables(OmsResource& res);
   /// 在重建新的 OmsCore 之後, 從舊的 OmsCore 複製商品資訊.
   /// 預設: *static_cast<fon9::fmkt::SymbTwaBase*>(&nsymb) = osymb;
   virtual void RestoreSymb(OmsSymb& nsymb, const OmsSymb& osymb);
   /// 在重建新的 OmsCore 之後, 從舊的 OmsCore 複製 券商&帳號 資訊.
   /// 預設: 複製全部的帳號資訊.
   virtual void RestoreBrk(OmsBrk& nbrk, const OmsBrk& obrk);
   virtual void RestoreIvac(OmsIvac& nivac, const OmsIvac& oivac);
   virtual void RestoreSubac(OmsSubac& nsubac, const OmsSubac& osubac);

   static void WaitCoreDone(OmsCore* core);

public:
   const OmsCoreMgrSP CoreMgr_;

   /// 通常由 OmsCoreMgr 計算風控.
   /// 建構完成後, 還需要, 由衍生者:
   /// - 建立 this->CoreMgr() 的 order factory, request factory, event factory...
   OmsCoreUT(OmsCoreMgrSP coreMgr);
   virtual ~OmsCoreUT();

   virtual bool OnCommand(fon9::StrView cmd, fon9::StrView args);

   bool AddCore(bool isRemoveLog);

   OmsCore* CurrentCore() const {
      return this->CoreMgr_->CurrentCore().get();
   }

   void WaitCoreDone() {
      this->WaitCoreDone(this->CurrentCore());
   }

   // 請注意: **正式環境不能** 取出 OmsResource 裡面的東西, 給另一個 thread 使用.
   // ordkey = RxSNO 或 BrkId-Market-Session-OrdNo
   const OmsOrderRaw* GetOrderRaw(fon9::StrView ordkey);
};

//--------------------------------------------------------------------------//
extern int MainOmsCoreUT(OmsCoreUT& omsCoreUT, int argc, const char** argv);

} // namespaces
#endif//__f9omstw_f9omstwUT_hpp__
