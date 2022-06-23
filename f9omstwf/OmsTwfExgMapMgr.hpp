// \file f9omstwf/OmsTwfExgMapMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfExgMapMgr_hpp__
#define __f9omstwf_OmsTwfExgMapMgr_hpp__
#include "f9twf/ExgMapMgr.hpp"
#include "f9omstw/OmsImporter.hpp"
#include "f9omstwf/OmsTwfExgSymbBasic.hpp"
#include "f9omstwf/TwfCurrencyIndex.hpp"

namespace f9omstw {

class TwfExgMapMgr : public f9twf::ExgMapMgr, public OmsFileImpCoreRun {
   fon9_NON_COPY_NON_MOVE(TwfExgMapMgr);
   using base = f9twf::ExgMapMgr;
   using baseCoreRun = OmsFileImpCoreRun;
   fon9::intrusive_ptr<TwfExgContractTree>   ContractTree_;

   using base::SetTDay;
   void Ctor();

protected:
   void OnP08Updated(const f9twf::P08Recs& p08recs, f9twf::ExgSystemType sysType, MapsConstLocker&& lk) override;
   void OnP09Updated(const f9twf::P09Recs& p09recs, f9twf::ExgSystemType sysType, MapsConstLocker&& lk) override;
   void OnP06Updated(const f9twf::ExgMapBrkFcmId& mapBrkFcmId, MapsLocker&& lk) override;

public:
   OmsCoreMgr&                   CoreMgr_;
   const CurrencyConfig_TreeSP   CurrencyTree_;

   template <class... ArgsT>
   TwfExgMapMgr(OmsCoreMgr& coreMgr, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , CoreMgr_(coreMgr)
      , CurrencyTree_{new CurrencyConfig_Tree} {
      this->Ctor();
      coreMgr.SetTwfExgMapMgr(this);
   }
   ~TwfExgMapMgr();

   /// 在設定 OMS 核心資料表之後,
   /// 載入相關期交所資料,
   /// 必須透過這裡啟動載入, 禁止直接呼叫 SetTDay();
   void OnAfter_InitCoreTables(OmsResource& res);

   /// 在 OnAfter_InitCoreTables() 之後, 才會有效.
   TwfExgContractTree* GetContractTree() const {
      return this->ContractTree_.get();
   }

   template <class Locker>
   bool RunCoreTask(Locker&& lk, OmsCoreTask&& task) {
      lk.unlock();
      return baseCoreRun::RunCoreTask(std::move(task));
   }
   bool RunCoreTask(nullptr_t, OmsCoreTask&& task) {
      return baseCoreRun::RunCoreTask(std::move(task));
   }
};
using TwfExgMapMgrSP = fon9::intrusive_ptr<TwfExgMapMgr>;

/// 匯入 P11: 漲跌停價.
/// 使用 "fon9/fmkt/SymbTabNames.h" 定義的 fon9_kCSTR_TabName_Ref 取得 f9twf::TwfSymbRef;
void TwfAddP11Importer(TwfExgMapMgr&);

/// 匯入 F02 漲跌停 level(FUNCTION-CODE=101):
/// - 預設使用 fon9::seed::FileImpMonitorFlag::AddTail;
/// - Ln.1: INFORMATION-TIME[6]=HHMMSS, NUMBER-RECORDS[10]=不含表頭之訊息筆數, [1]=\n
void TwfAddF02Importer(TwfExgMapMgr&);

/// 匯入 P13:交易人契約部位限制檔;
/// 會先等 P09 匯入完畢, 匯入時會設定 MainId;
void TwfAddP13Importer(TwfExgMapMgr&);

// 匯入 TwfContractRef.cfg: 契約 MainId 設定檔.
void TwfAddContractRefImporter(TwfExgMapMgr&);

/// 匯入 N09 股票選擇權/股票期貨契約逾越個股類全市場部位限制限單公告檔;
/// - 預設使用 fon9::seed::FileImpMonitorFlag::AddTail;
void TwfAddN09Importer(TwfExgMapMgr&);

/// 匯入 C01:匯率檔;
/// 會先等 TwfCurrencyIndex.cfg 匯入完畢;
void TwfAddC01Importer(TwfExgMapMgr&);

/// 匯入 C11:選擇權契約行情價;
/// 會先等 P09 OptNormal 匯入完畢;
void TwfAddC11Importer(TwfExgMapMgr&);

/// 匯入 P14:契約風險保證金資料定義檔;
void TwfAddP14Importer(TwfExgMapMgr&);

} // namespaces
#endif//__f9omstwf_OmsTwfExgMapMgr_hpp__
