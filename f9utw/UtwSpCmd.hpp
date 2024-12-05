// \file f9utw/UtwSpCmd.hpp
// \author fonwinz@gmail.com
#ifndef __f9utw_UtwSpCmd_hpp__
#define __f9utw_UtwSpCmd_hpp__
#include "f9omstw/OmsCore.hpp"

namespace f9omstw {

struct SpCmdArgs {
   fon9::CharVector     SymbId_;
   OmsIvKey             IvKey_;
   OmsRequestFrom       ReqFrom_;
   OmsRequestPolicyCfg  PolicyCfg_;
   uint32_t             Filter_{};
   char                 Padding____[4];
};
//--------------------------------------------------------------------------//
class SpCmdBase : public fon9::seed::NamedSeed {
   fon9_NON_COPY_NON_MOVE(SpCmdBase);
   using base = fon9::seed::NamedSeed;

protected:
   /// 呼叫 OnSeedCommand() 時, ulk 在解鎖狀態.
   /// 預設: resHandler("Unknown command.");
   void OnSeedCommand(fon9::seed::SeedOpResult& res,
                      fon9::StrView cmdln,
                      fon9::seed::FnCommandResultHandler resHandler,
                      fon9::seed::MaTreeBase::Locker&& ulk,
                      fon9::seed::SeedVisitor* visitor) override;

public:
   template <class... NamedArgsT>
   SpCmdBase(NamedArgsT&&... namedargs)
      : base(std::forward<NamedArgsT>(namedargs)...) {
   }
};
//--------------------------------------------------------------------------//
class SpCmdMgr;

class SpCmdItem : public SpCmdBase {
   fon9_NON_COPY_NON_MOVE(SpCmdItem);
   using base = SpCmdBase;
   std::string State_;
protected:
   void UpdateState(std::string&& st);

public:
   SpCmdMgr&         SpCmdMgr_;
   const SpCmdArgs   Args_;

   template <class... NamedArgsT>
   SpCmdItem(SpCmdMgr& spCmdMgr, SpCmdArgs&& args, NamedArgsT&&... namedargs)
      : base(std::forward<NamedArgsT>(namedargs)...)
      , SpCmdMgr_(spCmdMgr)
      , Args_(std::move(args)) {
   }

   virtual void Start() = 0;
   static fon9::seed::LayoutSP MakeDefaultLayout();
};
using SpCmdItemSP = fon9::intrusive_ptr<SpCmdItem>;
//--------------------------------------------------------------------------//
class SpCmdOrd : public SpCmdItem {
   fon9_NON_COPY_NON_MOVE(SpCmdOrd);
   using base = SpCmdItem;
protected:
   OmsRequestPolicySP   RequestPolicy_;
   virtual void StartRunInCore(OmsResource&);
   void OmsLogUpdateState(std::string&& st);

public:
   using base::base;
   void Start() override;
};
//--------------------------------------------------------------------------//
class SpCmdForEachOrd : public SpCmdOrd {
   fon9_NON_COPY_NON_MOVE(SpCmdForEachOrd);
   using base = SpCmdOrd;
protected:
   /// 最後一次檢查的回報序號.
   OmsRxSNO LastCheckRxSNO_{};
   /// 預期在哪個序號結束.
   OmsRxSNO FinishedRxSNO_{};
   void StopRecover() {
      this->FinishedRxSNO_ = 0;
   }

   /// 呼叫 OnSeedCommand() 時, ulk 在解鎖狀態.
   void OnSeedCommand(fon9::seed::SeedOpResult& res,
                      fon9::StrView cmdln,
                      fon9::seed::FnCommandResultHandler resHandler,
                      fon9::seed::MaTreeBase::Locker&& ulk,
                      fon9::seed::SeedVisitor* visitor) override;
   virtual OmsRxSNO OnReportRecover(OmsCore&, const OmsRxItem* item) = 0;
   virtual void StartRunInCore(OmsResource&);
public:
   using base::base;
};
//--------------------------------------------------------------------------//
class SpCmdDord : public SpCmdForEachOrd {
   fon9_NON_COPY_NON_MOVE(SpCmdDord);
   using base = SpCmdForEachOrd;
   using DreqList = std::deque<const OmsRxItem*>;
   DreqList       DreqList_;
   fon9::SubConn  RptSubr_{};

   void StartRunInCore(OmsResource&) override;
   OmsRxSNO OnReportRecover(OmsCore&, const OmsRxItem* item) override;
   void OnReportHandle(OmsCore&, const OmsRxItem&);
   void RptUnsub();

   /// \retval false 當 this->ReqChgFactory_.MakeRequest(); 返回 nullptr;
   bool CheckForDord(const OmsRequestIni& iniReq);
   /// 例: 檢查 SymbId 是否符合預期.
   /// \retval false 表示此筆要求不用處理.
   virtual bool CheckForDord_ArgsFilter(const OmsRequestIni& iniReq) = 0;
   /// 建立一個刪單要求.
   virtual fon9::intrusive_ptr<OmsRequestUpd> MakeDeleteRequest(const OmsRequestIni& iniReq) = 0;
public:
   using base::base;
   ~SpCmdDord();
   void OnParentTreeClear(fon9::seed::Tree& tree) override;
};
//--------------------------------------------------------------------------//
class SpCmdMgr : public SpCmdBase {
   fon9_NON_COPY_NON_MOVE(SpCmdMgr);
   using base = SpCmdBase;
   std::atomic<size_t>  SpCmdId_{0};

   /// 呼叫 OnSeedCommand() 時, ulk 在解鎖狀態.
   void OnSeedCommand(fon9::seed::SeedOpResult& res,
                      fon9::StrView cmdln,
                      fon9::seed::FnCommandResultHandler resHandler,
                      fon9::seed::MaTreeBase::Locker&& ulk,
                      fon9::seed::SeedVisitor* visitor) override;
   SpCmdItemSP MakeSpCmdDord(fon9::StrView& strResult,
                             fon9::StrView cmdln,
                             fon9::seed::MaTreeBase::Locker&& ulk,
                             fon9::seed::SeedVisitor* visitor);

   virtual SpCmdItemSP MakeSpCmdDord_Impl(fon9::StrView& strResult, SpCmdArgs&& args, fon9::Named&& itemName) = 0;
   /// 預設返回 false 表示錯誤.
   virtual bool ParseSpCmdArg(SpCmdArgs& args, fon9::StrView tag, fon9::StrView value);
public:
   const OmsCoreSP            OmsCore_;
   const fon9::seed::MaTreeSP Sapling_;

   template <class... ArgsT>
   SpCmdMgr(OmsCoreSP omsCore, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , OmsCore_{std::move(omsCore)}
      , Sapling_{new fon9::seed::MaTree{SpCmdItem::MakeDefaultLayout()}} {
   }
   fon9::seed::TreeSP GetSapling() override;
};

} // namespaces
#endif//__f9utw_UtwSpCmd_hpp__
