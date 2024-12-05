// \file f9omstw/OmsCxUnit.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCxUnit_hpp__
#define __f9omstw_OmsCxUnit_hpp__
#include "f9omstw/OmsCxBase.hpp"
#include "f9omstw/OmsSymb.hpp"
#include "f9omstw/OmsErrCode.h"

namespace f9omstw {

//--------------------------------------------------------------------------//
class OmsCxMdEvArgs {
   fon9_NON_COPY_NON_MOVE(OmsCxMdEvArgs);
public:
   OmsCore* const          OmsCore_;
   OmsSymb&                CondSymb_;
   /// 因為同一個商品異動事件, 可能會有多個條件等候中,
   /// 所以建構時先不用設定 CxRaw_;
   /// 等需要檢查(呼叫 OmsCxUnit)時再填入;
   const OmsCxBaseRaw*     CxRaw_;
   /// 若是在收單階段(OmsCore/OmsRequestRunStep)的判斷條件是否成立, 則 BfXXX_ 為 nullptr;
   /// 若是行情異動時的觸發, 則對應的 BfXXX_, AfXXX_ 都是有效的指標.
   const OmsMdLastPriceEv* AfMdLastPrice_{nullptr};
   const OmsMdBSEv*        AfMdBS_{nullptr};
   const OmsMdLastPrice*   BfMdLastPrice_{nullptr};
   const OmsMdBS*          BfMdBS_{nullptr};
   const OmsCxSrcEvMask    EvMask_;
   char                    Padding7_[7];
   /// 若條件成立, 則可在此建立 Log 字串.
   fon9::RevBufferFixedSize<256> mutable LogRBuf_;

   OmsCxMdEvArgs(OmsCore& omsCore, OmsSymb& condSymb, const OmsMdLastPrice& bf, const OmsMdLastPriceEv& af, OmsCxSrcEvMask evMask)
      : OmsCore_(&omsCore)
      , CondSymb_(condSymb)
      , AfMdLastPrice_(&af)
      , BfMdLastPrice_(&bf)
      , EvMask_{evMask} {
   }
   OmsCxMdEvArgs(OmsCore& omsCore, OmsSymb& condSymb, const OmsMdBS& bf, const OmsMdBSEv& af, OmsCxSrcEvMask evMask)
      : OmsCore_(&omsCore)
      , CondSymb_(condSymb)
      , AfMdBS_(&af)
      , BfMdBS_(&bf)
      , EvMask_{evMask} {
   }
   OmsCxMdEvArgs(OmsSymb& condSymb, const OmsMdLastPriceEv* mdLastPrice, const OmsCxBaseRaw& cxRaw)
      : OmsCore_{nullptr}
      , CondSymb_(condSymb)
      , CxRaw_(&cxRaw)
      , AfMdLastPrice_(mdLastPrice)
      , EvMask_{OmsCxSrcEvMask::ForCheckFireNow} {
   }
   OmsCxMdEvArgs(OmsSymb& condSymb, const OmsMdBSEv* mdBS, const OmsCxBaseRaw& cxRaw)
      : OmsCore_(nullptr)
      , CondSymb_(condSymb)
      , CxRaw_(&cxRaw)
      , AfMdBS_(mdBS)
      , EvMask_{OmsCxSrcEvMask::ForCheckFireNow} {
   }
};
typedef bool (OmsCxUnit::*OnOmsCxMdEvFnT)(const OmsCxMdEvArgs& args);
//--------------------------------------------------------------------------//
/// 條件判斷單元.
/// 這裡衍生: 自訂的檢查方式(自訂策略).
class OmsCxUnit {
   fon9_NON_COPY_NON_MOVE(OmsCxUnit);
   OmsCxUnit() = delete;

   friend struct OmsCxCreatorInternal;
   void SetRegEvMask(OmsCxSrcEvMask evMask) { this->Cond_.RegEvMask_ = evMask; }

   /// 多條件單裡面, 使用相同商品資料的判斷單元.
   /// 同一個 OmsCxExpr 裡面的判斷單元, 相同商品只需註冊一次.
   /// 當行情有異動時, 透過 CondSameSymbNext_ 通知相同商品的 OmsCxUnit;
   OmsCxUnit* CondSameSymbNext_{nullptr};
   /// 多條件單裡面, 下一個, 使用不同商品資料的判斷單元.
   /// 協助 OmsCxReqBase 註冊條件使用;
   OmsCxUnit* CondOtherSymbNext_{nullptr};

   /// 根據條件判斷結果設定 CurrentSt;
   bool OnCondFnResult(bool isTrue) {
      if (isTrue) {
         this->Cond_.CurrentSt_ = (this->IsAnyWaitTrueLock() ? OmsCxUnitSt::TrueLocked : OmsCxUnitSt::TrueAndWaiting);
         return true;
      }
      this->Cond_.CurrentSt_ = OmsCxUnitSt::FalseAndWaiting;
      return false;
   }
   /// - 新單收單階段(OmsCore/OmsRequestRunStep)的條件判斷.
   /// - 改條件內容之後的判斷.
   /// - 這裡預設傳回 false; 由衍生者實作;
   /// \retval true  則應立即送出, 不用等候條件.
   /// \retval false 需要等候條件, 成立才送出. 例: 最後成交單量,就不可在[收單階段]判斷,此時傳回false.
   virtual bool OmsCx_IsNeedsFireNow(OmsSymb& condSymb, const OmsCxBaseRaw& cxRaw);

public:
   const OmsCxCondRec   Cond_;
   const OmsSymbSP      CondSymb_;
   // -------------------------------------------------------------------------------- //
   OmsCxUnit(const OmsCxCreatorArgs& args);
   virtual ~OmsCxUnit();

   OmsCxUnit* CondSameSymbNext() const { return this->CondSameSymbNext_; }
   OmsCxUnit* CondOtherSymbNext() const { return this->CondOtherSymbNext_; }
   OmsCxSrcEvMask RegEvMask() const { return this->Cond_.RegEvMask_; }
   // -------------------------------------------------------------------------------- //
   /// 輸出條件參數, 不包含 CondName、CondOp;
   /// 預設 根據 this->CheckFlags_ 輸出: Qty,Pri 或 Qty 或 Pri;
   /// \retval true rbuf有新的輸出內容.
   virtual bool RevPrintCxArgs(fon9::RevBuffer& rbuf) const;
   void RevPrint(fon9::RevBuffer& rbuf) const;
   // -------------------------------------------------------------------------------- //
   OmsCxUnitSt CurrentSt() const {
      return this->Cond_.CurrentSt_;
   }
   bool IsCxWaitTrueLock() const {
      return IsEnumContains(this->Cond_.CheckFlags_, OmsCxCheckFlag::CxWaitTrueLock);
   }
   bool IsUsrWaitTrueLock() const {
      return IsEnumContains(this->Cond_.CheckFlags_, OmsCxCheckFlag::UsrWaitTrueLock);
   }
   bool IsAnyWaitTrueLock() const {
      return IsEnumContainsAny(this->Cond_.CheckFlags_, (OmsCxCheckFlag::CxWaitTrueLock | OmsCxCheckFlag::UsrWaitTrueLock));
   }
   // -------------------------------------------------------------------------------- //
   /// 是否允許更改條件?
   /// 預設為 false;
   virtual bool IsAllowChgCond() const;

   /// 檢查條件內容是否正確, 通常用於[改條件]時,檢查改單內容是否正確.
   /// 預設根據 this->CheckFlags_ 檢查 dat.CondPri_ 及 dat.CondQty_ 是否正確;
   /// 若正確則傳回 OmsErrCode_Null;
   virtual OmsErrCode OnOmsCx_CheckCondDat(const OmsCxBaseChangeable& dat) const;

   /// 若 reqChg 的內容允許(this->OnOmsCx_CheckCondDat(reqChg) 返回 OmsErrCode_Null),
   /// 則將 reqChg 的內容填入 cxRaw;
   virtual OmsErrCode OnOmsCx_AssignReqChgToOrd(const OmsCxBaseChgDat& reqChg, OmsCxBaseRaw& cxRaw) const;
   // -------------------------------------------------------------------------------- //
   /// 這裡預設都是傳回 false;
   virtual bool OnOmsCx_MdLastPriceEv(const OmsCxMdEvArgs& args);
   virtual bool OnOmsCx_MdLastPriceEv_OddLot(const OmsCxMdEvArgs& args);
   /// 這裡預設都是傳回 false;
   virtual bool OnOmsCx_MdBSEv(const OmsCxMdEvArgs& args);
   virtual bool OnOmsCx_MdBSEv_OddLot(const OmsCxMdEvArgs& args);

   /// 若已經 TrueLocked 則直接返回 true;
   /// 否則執行 onOmsCxEvFn 判斷函式, 然後設定CurrentSt, 並返回條件是否成立;
   bool OnOmsCx_CallEvFn(OnOmsCxMdEvFnT onOmsCxEvFn, OmsCxMdEvArgs& args) {
      if (this->Cond_.CurrentSt_ == OmsCxUnitSt::TrueLocked)
         return true;
      if ((args.EvMask_ & this->Cond_.SrcEvMask_) == OmsCxSrcEvMask{})
         return false;
      return this->OnCondFnResult((this->*onOmsCxEvFn)(args));
   }
   // -------------------------------------------------------------------------------- //
   /// 檢查現在是否條件成立.
   /// 若已經 TrueLock 則直接傳回 true;
   /// 否則執行 this->OmsCx_IsNeedsFireNow(), 然後設定CurrentSt, 並返回條件是否成立;
   bool CheckNeedsFireNow(OmsSymb& condSymb, const OmsCxBaseRaw& cxRaw) {
      assert(this->CondSymb_.get() == &condSymb);
      if (this->Cond_.CurrentSt_ == OmsCxUnitSt::TrueLocked)
         return true;
      return this->OnCondFnResult(this->OmsCx_IsNeedsFireNow(condSymb, cxRaw));
   }
};

} // namespaces
#endif//__f9omstw_OmsCxUnit_hpp__
