﻿// \file f9omstw/OmsCoreMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCoreMgr_hpp__
#define __f9omstw_OmsCoreMgr_hpp__
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsErrCodeActTree.hpp"
#include "fon9/SortedVector.hpp"

namespace f9omstw {

using OmsTDayChangedHandler = std::function<void(OmsCore&)>;
/// isReload != nullptr 表示是 backend reload 時產生的事件.
using OmsEventHandler = std::function<void(OmsResource&, const OmsEvent&, const OmsBackend::Locker* isReload)>;

using OmsRequestFactoryPark = OmsFactoryPark_WithKeyMaker<OmsRequestFactory, &OmsRequestBase::MakeField_RxSNO, fon9::seed::TreeFlag::Unordered>;
using OmsRequestFactoryParkSP = fon9::intrusive_ptr<OmsRequestFactoryPark>;

using OmsOrderFactoryPark = OmsFactoryPark_NoKey<OmsOrderFactory, fon9::seed::TreeFlag::Unordered>;
using OmsOrderFactoryParkSP = fon9::intrusive_ptr<OmsOrderFactoryPark>;

using OmsEventFactoryPark = OmsFactoryPark_NoKey<OmsEventFactory, fon9::seed::TreeFlag::Unordered>;
using OmsEventFactoryParkSP = fon9::intrusive_ptr<OmsEventFactoryPark>;

//--------------------------------------------------------------------------//

/// 如果有 Lg 的需求, 則進入下單流程前, 必須先填好 OmsRequestTrade::LgOut_;
/// 填入 OmsRequestTrade::LgOut_; 的時機:
/// (1) 收單程序, 例如: OmsRcServerFunc.cpp 使用 PolicyConfig_.UserRights_.LgOut_;
/// (2) 若收單程序沒填, 則透過 OmsCoreMgr::FnSetRequestLgOut_ 處理.
///     - 在 OmsCore.cpp 進入 OmsRequestRunnerInCore 之前呼叫 OmsCoreMgr::FnSetRequestLgOut_();
///     - 如果 OmsCoreMgr::FnSetRequestLgOut_ == nullptr, 則不再有機會改變 OmsRequestTrade::LgOut_;
///     - 這裡提供一個使用 OmsIvac::LgOut_; 的範例 OmsSetRequestLgOut_UseIvac()
void OmsSetRequestLgOut_UseIvac(OmsResource& res, OmsRequestTrade& req, OmsOrder& order);
using FnSetRequestLgOut = void (*)(OmsResource&, OmsRequestTrade&, OmsOrder&);

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
/// 管理 OmsCore 的生死.
/// - 每個 OmsCore 僅處理一交易日的資料, 不處理換日、清檔,
///   交易日結束時, 由 OmsCoreMgr 在適當時機刪除過時的 OmsCore.
/// - 新交易日開始時, 建立新的 OmsCore.
///   - OmsCore 建立時, 轉入隔日有效單.
///   - 匯入商品資料、投資人資料、庫存...
class OmsCoreMgr : public fon9::seed::MaTree {
   fon9_NON_COPY_NON_MOVE(OmsCoreMgr);
   using base = fon9::seed::MaTree;
   class CurrentCoreSapling : public fon9::seed::NamedSeed {
      fon9_NON_COPY_NON_MOVE(CurrentCoreSapling);
      using base = fon9::seed::NamedSeed;
   public:
      fon9::seed::TreeSP Sapling_;
      using base::base;
      fon9::seed::TreeSP GetSapling() override;
   };
   OmsCoreSP            CurrentCore_;
   CurrentCoreSapling&  CurrentCoreSapling_;
   bool                 IsTDayChanging_{false};
   ErrCodeActSeed*      ErrCodeActSeed_;

protected:
   OmsOrderFactoryParkSP   OrderFactoryPark_;
   OmsRequestFactoryParkSP RequestFactoryPark_;
   OmsEventFactoryParkSP   EventFactoryPark_;

   void OnMaTree_AfterAdd(Locker&, fon9::seed::NamedSeed& seed) override;
   void OnMaTree_AfterClear() override;

public:
   const FnSetRequestLgOut FnSetRequestLgOut_{};

   OmsCoreMgr(FnSetRequestLgOut fnSetRequestLgOut);

   using TDayChangedEvent = fon9::Subject<OmsTDayChangedHandler>;
   TDayChangedEvent  TDayChangedEvent_;

   /// 重新載入時的 OmsEvent 比 TDayChangedEvent_ 還要早觸發.
   using OmsEventHandlerSubject = fon9::Subject<OmsEventHandler>;
   OmsEventHandlerSubject  OmsEvent_;

   /// 取得 CurrentCore, 僅供參考.
   /// - 返回前有可能會收到 TDayChanged 事件.
   /// - 傳回 nullptr 表示目前沒有 CurrentCore.
   OmsCoreSP CurrentCore() const {
      ConstLocker locker{this->Container_};
      return this->CurrentCore_;
   }
   fon9::TimeStamp CurrentCoreTDay() const {
      ConstLocker locker{this->Container_};
      if (this->CurrentCore_)
         return this->CurrentCore_->TDay();
      return fon9::TimeStamp{};
   }
   /// 在某些情況下, 必須重新啟動 CurrentCore, 但啟動前必須先關閉 CurrentCore,
   /// 所以在此提供一個關閉 CurrentCore 的方法,
   /// 但您仍必須確定沒有其他人保有 OmsCoreSP, 才會真正地將 CurrentCore 刪除;
   OmsCoreSP RemoveCurrentCore();

   void SetRequestFactoryPark(OmsRequestFactoryParkSP&& facPark) {
      assert(this->RequestFactoryPark_.get() == nullptr);
      this->RequestFactoryPark_ = std::move(facPark);
   }
   void SetOrderFactoryPark(OmsOrderFactoryParkSP&& facPark) {
      assert(this->OrderFactoryPark_.get() == nullptr);
      this->OrderFactoryPark_ = std::move(facPark);
   }
   void SetEventFactoryPark(OmsEventFactoryParkSP&& facPark) {
      assert(this->EventFactoryPark_.get() == nullptr);
      this->EventFactoryPark_ = std::move(facPark);
   }
   const OmsRequestFactoryPark& RequestFactoryPark() const {
      return *this->RequestFactoryPark_;
   }
   const OmsOrderFactoryPark& OrderFactoryPark() const {
      return *this->OrderFactoryPark_;
   }
   const OmsEventFactoryPark& EventFactoryPark() const {
      return *this->EventFactoryPark_;
   }

   /// 從 cfgfn 載入 OmsErrCodeAct 的設定.
   void ReloadErrCodeAct(fon9::StrView cfgfn) {
      this->ErrCodeActSeed_->ReloadErrCodeAct(cfgfn, this->Lock());
   }
   OmsErrCodeActSP GetErrCodeAct(OmsErrCode ec) const {
      return this->ErrCodeActSeed_->GetErrCodeAct(ec);
   }

   /// 更新風控: 委託資料有異動時. 預設: do nothing.
   virtual void UpdateSc(OmsRequestRunnerInCore& runner);
   /// 重算風控: Backend.Reload 重新載入後, 從第一筆委託往後, 載入重算.
   /// 預設: do nothing.
   virtual void RecalcSc(OmsResource& resource, OmsOrder& order);

   /// OmsCore 通知收到 OmsEvent;
   /// 預設: do nothing.
   virtual void OnEventInCore(OmsResource& resource, OmsEvent& omsEvent, fon9::RevBuffer& rbuf);

   /// Backend.Reload 重新載入後, 重新處理 OmsEvent.
   /// 預設: do nothing.
   virtual void ReloadEvent(OmsResource& resource, const OmsEvent& omsEvent, const OmsBackend::Locker& reloadItems);
};
fon9_WARN_POP;

} // namespaces
#endif//__f9omstw_OmsCore_hpp__
