// \file f9utw/UtwsSymb.hpp
// \author fonwinz@gmail.com
#ifndef __f9utw_UtwsSymb_hpp__
#define __f9utw_UtwsSymb_hpp__
#include "f9omstw/OmsSymb.hpp"
#include "f9omstw/OmsCxRequest.hpp"
#include "f9omstwf/OmsTwfExgSymbBasic.hpp"

namespace f9omstw {

/// 這裡提供一個 OMS 商品基本資料的範例.
class UtwsSymb : public OmsSymb, public TwfExgSymbBasic {
   fon9_NON_COPY_NON_MOVE(UtwsSymb);
   using base = OmsSymb;

protected:
   struct CondReq {
      int                     Priority_;
      char                    Padding____[4];
      const OmsCxBaseIniFn*   CxReq_;
      OmsRequestRunStep*      NextStep_;
      CondReq(int priority, const OmsCxBaseIniFn& cxReq, OmsRequestRunStep& nextStep)
         : Priority_{priority}, CxReq_{&cxReq}, NextStep_{&nextStep} {
      }
   };
   struct CondReqComp {
      bool operator()(const CondReq& lhs, int rhs) const {
         return lhs.Priority_ < rhs;
      }
      bool operator()(int lhs, const CondReq& rhs) const {
         return lhs < rhs.Priority_;
      }
   };
   using CondReqListImpl = std::vector<CondReq>;
   struct CondReqList : public fon9::MustLock<CondReqListImpl> {
      fon9_NON_COPY_NON_MOVE(CondReqList);
      CondReqList() = default;
      void lock() { this->Mutex_.lock(); }
      void unlock() { this->Mutex_.unlock(); }
      void Add(const MdLocker& mdLocker, int priority, const OmsCxBaseIniFn& cxReq, OmsRequestRunStep& nextStep) {
         // mdLocker 用的是 this->lock(), 所以 this->Mutex_ 必定已鎖.
         (void)mdLocker; assert(mdLocker.owns_lock());
         CondReqListImpl::iterator ipos = std::upper_bound(this->Base_.begin(), this->Base_.end(), priority, CondReqComp{});
         this->Base_.insert(ipos, CondReq{priority, cxReq, nextStep});
      }
   };
   CondReqList CondReqList_;

   void CheckCondReq(OmsCxBaseCondEvArgs& args, OmsCoreMgr& coreMgr, fon9::RevBufferFixedMem& rbuf,
                     bool (OmsCxBaseCondFn::*onOmsCxEvFn)(const OmsCxBaseCondEvArgs& args));

public:
   using base::base;
   ~UtwsSymb();

   fon9::fmkt::SymbData* GetSymbData(int tabid) override;
   fon9::fmkt::SymbData* FetchSymbData(int tabid) override;
   TwfExgSymbBasic* GetTwfExgSymbBasic() override;

   static fon9::seed::LayoutSP MakeLayout(fon9::seed::TreeFlag flags, bool isStkMarket = false);

   static OmsSymbSP SymbMaker(const fon9::StrView& symbid) {
      return new UtwsSymb{symbid};
   }
   //--------------------------------------------------------------------//
   void LockMd() override;
   void UnlockMd() override;
   /// 將 req.CondQty_ 及 req.CondPri_ 填入 cxRaw;
   /// 然後判斷現在條件是否成立.
   /// \retval false 條件已成立, 應立即送出, 不加入等候列表.
   /// \retval true  加入成功, 等後觸發.
   bool AddCondReq(const MdLocker& mdLocker, int priority, const OmsCxBaseIniFn& cxReq, OmsCxBaseOrdRaw& cxRaw, OmsRequestRunStep& nextStep, fon9::RevBuffer& rbuf);
   //--------------------------------------------------------------------//
   OmsMdLastPriceEv* GetMdLastPriceEv() override;
   void OnMdLastPriceEv(const OmsMdLastPrice& bf, OmsCoreMgr& coreMgr) override;
   //--------------------------------------------------------------------//
   struct MdLastPriceEv_OddLot : public OmsMdLastPriceEv {
      fon9_NON_COPY_NON_MOVE(MdLastPriceEv_OddLot);
      MdLastPriceEv_OddLot () = default;
      void OnMdLastPriceEv(const OmsMdLastPrice& bf, OmsCoreMgr&) override;
      friend class UtwsSymb;//using OmsMdLastPriceEv::SetIsNeedsOnMdLastPriceEv;
   };
   MdLastPriceEv_OddLot MdLastPriceEv_OddLot_;
   OmsMdLastPriceEv* GetMdLastPriceEv_OddLot() override;
   //--------------------------------------------------------------------//
   struct MdBSEv : public OmsMdBSEv {
      fon9_NON_COPY_NON_MOVE(MdBSEv);
      MdBSEv() = default;
      void OnMdBSEv(const OmsMdBS& bf, OmsCoreMgr&) override;
      friend class UtwsSymb;//using OmsMdBSEv::SetIsNeedsOnMdBSEv;
   };
   MdBSEv  MdBSEv_;
   OmsMdBSEv* GetMdBSEv() override;
   //--------------------------------------------------------------------//
   struct MdBSEv_OddLot : public OmsMdBSEv {
      fon9_NON_COPY_NON_MOVE(MdBSEv_OddLot);
      MdBSEv_OddLot() = default;
      void OnMdBSEv(const OmsMdBS& bf, OmsCoreMgr&) override;
      friend class UtwsSymb;//using OmsMdBSEv::SetIsNeedsOnMdBSEv;
   };
   MdBSEv_OddLot MdBSEv_OddLot_;
   OmsMdBSEv* GetMdBSEv_OddLot() override;
};

} // namespaces
#endif//__f9utw_UtwsSymb_hpp__
