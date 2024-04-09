// \file f9omstw/OmsSymb.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsSymb_hpp__
#define __f9omstw_OmsSymb_hpp__
#include "fon9/fmkt/SymbTwa.hpp"
#include "fon9/fmkt/SymbTws.hpp"
#include "fon9/fmkt/SymbTwf.hpp"
#include "fon9/fmkt/FmktTypes.hpp"

namespace f9omstw {

class OmsResource;
class TwfExgSymbBasic;
class OmsMdLastPriceEv;
class OmsMdBSEv;

class OmsSymb : public fon9::fmkt::SymbTwa {
   fon9_NON_COPY_NON_MOVE(OmsSymb);
   using base = fon9::fmkt::SymbTwa;
   const fon9::fmkt::MdReceiverSt* MdReceiverStPtr_{nullptr};

public:
   using base::base;
   ~OmsSymb();

   /// 台灣期交所商品換盤事件.
   /// 在 TwfExgMapMgr::OnP08Updated() 觸發.
   /// 預設 do nothing.
   virtual void OnTwfSessionChanged(OmsResource& coreResource);
   /// - 重設 [複式商品] 的 SessionId.
   ///   - 因為 [複式商品] 的 SessionId, 更新的地方: 行情收到[複式商品]的資料時.
   ///   - 若尚未收到行情, 則[複式商品]的 SessionId 不會更新.
   /// - 因此, 當有可能是複式商品時, 若要取得 [複式商品的 SessionId], 則要先重設;
   ///   - 使用 leg1 的 SessionId;
   /// - 預設: do nothing.
   virtual void ResetTwfCombSessionId(OmsResource& coreResource);
   /// 收盤事件, 預設 do nothing.
   /// 觸發時機: OmsTwfMiSystem.I140:306
   virtual void OnTradingSessionClosed(OmsResource& coreResource);
   /// 行情超過 Hb interval 仍然沒有任何資料, 則觸發此事件.
   /// 在 OmsTwfMiSystem.OnMiChannelNoData() 觸發.
   /// 預設 do nothing.
   virtual void OnMdNoData(OmsResource& coreResource);

   fon9::fmkt::MdReceiverSt MdReceiverSt() const {
      return this->MdReceiverStPtr_ ? *this->MdReceiverStPtr_ : fon9::fmkt::MdReceiverSt::DailyClear;
   }
   void SetMdReceiverStPtr(const fon9::fmkt::MdReceiverSt* pMdReceiverSt) {
      this->MdReceiverStPtr_ = pMdReceiverSt;
   }

   /// 提供給一些通用模組使用, 會比使用 dynamic_cast<OmsMdLastPriceEv*>(); 快一些!
   /// 預設傳回 nullptr;
   virtual TwfExgSymbBasic* GetTwfExgSymbBasic();
   /// 取得現在的漲跌停價, 若不支援, 則不改變內容, 返回 false.
   /// 例如, 期貨複式單, 就無法從[複式商品]直接取得, 需要用 leg1, leg2 的價格組合.
   /// 衍生者可能會在建構時(例: OnBeforeInsertToTree()), 儲存 Leg1, Leg2, 此時就可以支援了.
   virtual bool GetPriLmt(fon9::fmkt::Pri* upLmt, fon9::fmkt::Pri* dnLmt) const;

   virtual OmsMdLastPriceEv* GetMdLastPriceEv();
   virtual OmsMdBSEv* GetMdBSEv();

   class MdLocker {
      fon9_NON_COPY_NON_MOVE(MdLocker);
      MdLocker() = delete;
      OmsSymb* Owner_;
   public:
      MdLocker(OmsSymb& owner) : Owner_(&owner) {
         owner.LockMd();
      }
      ~MdLocker() {
         if (this->Owner_)
            this->Owner_->UnlockMd();
      }
      void Unlock() {
         assert(this->Owner_);
         this->Owner_->UnlockMd();
         this->Owner_ = nullptr;
      }
      bool owns_lock() const {
         return this->Owner_ != nullptr;
      }
   };
   /// 預設: do nothing;
   virtual void LockMd();
   virtual void UnlockMd();
};
using OmsSymbSP = fon9::intrusive_ptr<OmsSymb>;

// TODO: 是否要針對不同市場, 使用不同的 Symb?
// using OmsTwsSymb = fon9::fmkt::SymbTws;
// using OmsTwsSymbSP = fon9::intrusive_ptr<OmsTwsSymb>;
// using OmsTwfSymb = fon9::fmkt::SymbTwf;
// using OmsTwfSymbSP = fon9::intrusive_ptr<OmsTwfSymb>;

} // namespaces
#endif//__f9omstw_OmsSymb_hpp__
