// \file f9omstw/OmsCxCreator.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCxCreator_hpp__
#define __f9omstw_OmsCxCreator_hpp__
#include "f9omstw/OmsCx.h"
#include "f9omstw/OmsCxExpr.hpp"
#include "f9omstw/OmsCxReqBase.hpp"
#include "f9omstw/OmsResource.hpp"

namespace f9omstw {

struct OmsCxCreatorArgs {
   fon9_NON_COPY_NON_MOVE(OmsCxCreatorArgs);
   OmsCxCreatorArgs() = delete;
   OmsCxCreatorArgs(OmsCxReqBase& owner, const fon9::StrView& cxArgsStr, OmsResource& res, const OmsRequestTrade& txReq, OmsSymbSP txSymb)
      : Owner_(owner), OmsResource_(res), TxSymb_(std::move(txSymb)), Policy_{txReq.Policy()}, CxArgsStr_{cxArgsStr} {
      this->CxNormalized_.reserve(fon9::StrTrimHead(&this->CxArgsStr_).size());
   }
   virtual ~OmsCxCreatorArgs() {}

   const OmsCxReqBase&     Owner_;
   OmsResource&            OmsResource_;
   const OmsSymbSP         TxSymb_;
   const OmsRequestPolicy* Policy_;
   fon9::CharVector        CxNormalized_;
   fon9::StrView           CxArgsStr_;
   OmsSymbSP               CondSymb_;
   /// 底下欄位由各個 Creator 負責填寫:
   ///   this->Cond_.SrcEvMask_;
   ///   this->Cond_.CheckFlags_;
   ///   this->Cond_.CondPri_;
   ///   this->Cond_.CondQty_;
   ///   this->IsAllowChgCond_;
   /// 底下欄位會在呼叫 Creator 之前, 先解析完畢;
   ///   this->Cond_.Name_;
   ///   this->Cond_.Op_;
   ///   this->CompOp_;
   OmsCxCondRec            Cond_;
   f9oms_CondOp_Compare    CompOp_;
   /// 只有在不支援改條件內容時,才需要將 IsAllowChgCond_ 設定為 false;
   bool                    IsAllowChgCond_{true};
   char                    Padding6_[6];

   virtual void RequestAbandon(OmsErrCode ec, fon9::StrView reason = nullptr) = 0;

   /// chkFlags 必須要有 CondPriNeeds 旗標;
   /// chkFlags 若有 CondPriAllowMarket, 則允許使用 'M' 設定判斷市價;
   /// 不改變 this->Cond_.CheckFlags_;
   /// \retval true  返回前呼叫 this->CxArgsStr_CheckRemain();
   /// \retval false 取得 MdPri 失敗, 或不符合 chkFlags, 則返回前會先呼叫 this->RequestAbandon();
   bool ParseMdPri(OmsCxCheckFlag chkFlags, MdPri& result);
   /// chkFlags 必須要有 CondQtyNeeds 旗標;
   /// chkFlags 若有 CondQtyCannotZero, 則不允許數量為 0;
   /// 不改變 this->Cond_.CheckFlags_;
   /// \retval true  返回前呼叫 this->CxArgsStr_CheckRemain();
   /// \retval false 取得 MdQty 失敗, 或不符合 chkFlags, 則返回前會先呼叫 this->RequestAbandon();
   bool ParseMdQty(OmsCxCheckFlag chkFlags, MdQty& result);
   /// 根據 chkFlags 取得 "Qty" or "Pri" or "Pri,Qty" 填入 this->Cond_.CondPri_; this->Cond_.CondQty_;
   /// 將 chkFlags 填入 this->Cond_.CheckFlags_;
   /// \retval false 有錯誤,則返回前會先呼叫 this->RequestAbandon();
   bool ParseCondArgsPQ(OmsCxCheckFlag chkFlags);
   /// 檢查: fon9::StrTrimHead(&this->CxArgsStr_).Get1st()
   /// ',' 排除; '@' 設定 this->IsUsrTrueLock_;
   void CxArgsStr_CheckRemain();
};
//--------------------------------------------------------------------------//
/// 解析 args.CxArgsStr_;
/// - 呼叫前,已將底下參數準備好:
///   - args.CondSymb_;
///   - args.CondName_;
///   - args.CondOp_;
/// - 解析後續參數, 如果參數正確, 則建立並返回 OmsCxUnit*;
/// - 將解析後的[條件參數], 填入 args.CxNormalized_;
/// - 若有失敗, 必須先呼叫 args.RequestAbandon(); 然後返回 nullptr;
typedef OmsCxUnitUP (*CxUnitCreator) (OmsCxCreatorArgs& args);

/// 註冊 CxUnitCreator;
/// condName 所指向的字串, 必須永久有效. 通常在程式啟動時註冊, 例如:
/// AddCxUnitCreator("XX", CxUnit_XX_Creator);
void AddCxUnitCreator(fon9::StrView condName, CxUnitCreator creator);

/// 取得 CxUnitCreator;
CxUnitCreator GetCxUnitCreator(fon9::StrView condName);

} // namespaces
#endif//__f9omstw_OmsCxCreator_hpp__
