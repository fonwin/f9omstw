﻿// \file f9omstw/OmsCoreMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCoreMgr_hpp__
#define __f9omstw_OmsCoreMgr_hpp__
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsErrCodeActTree.hpp"
#include "fon9/SortedVector.hpp"

namespace f9omstw {

using OmsTDayChangedHandler = std::function<void(OmsCore&)>;

using OmsRequestFactoryPark = OmsFactoryPark_WithKeyMaker<OmsRequestFactory, &OmsRequestBase::MakeField_RxSNO, fon9::seed::TreeFlag::Unordered>;
using OmsRequestFactoryParkSP = fon9::intrusive_ptr<OmsRequestFactoryPark>;

using OmsOrderFactoryPark = OmsFactoryPark_NoKey<OmsOrderFactory, fon9::seed::TreeFlag::Unordered>;
using OmsOrderFactoryParkSP = fon9::intrusive_ptr<OmsOrderFactoryPark>;

using OmsEventFactoryPark = OmsFactoryPark_NoKey<OmsEventFactory, fon9::seed::TreeFlag::Unordered>;
using OmsEventFactoryParkSP = fon9::intrusive_ptr<OmsEventFactoryPark>;

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
   OmsCoreSP         CurrentCore_;
   bool              IsTDayChanging_{false};
   ErrCodeActSeed*   ErrCodeActSeed_;

protected:
   OmsOrderFactoryParkSP   OrderFactoryPark_;
   OmsRequestFactoryParkSP RequestFactoryPark_;
   OmsEventFactoryParkSP   EventFactoryPark_;

   void OnMaTree_AfterAdd(Locker&, fon9::seed::NamedSeed& seed) override;
   void OnMaTree_AfterClear() override;

public:
   OmsCoreMgr(std::string tabName);

   using TDayChangedEvent = fon9::Subject<OmsTDayChangedHandler>;
   TDayChangedEvent  TDayChangedEvent_;

   /// 取得 CurrentCore, 僅供參考.
   /// - 返回前有可能會收到 TDayChanged 事件.
   /// - 傳回 nullptr 表示目前沒有 CurrentCore.
   OmsCoreSP CurrentCore() const {
      ConstLocker locker{this->Container_};
      return this->CurrentCore_;
   }

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
};
fon9_WARN_POP;

} // namespaces
#endif//__f9omstw_OmsCore_hpp__