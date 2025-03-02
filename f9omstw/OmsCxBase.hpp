// \file f9omstw/OmsCxBase.hpp
//
// [條件單] 與一般 [無條件單] 的主要的差異在於:
//  (1) Request 及 OrderRaw => 增加條件相關欄位(例:OmsCxBaseChangeable);
//  (2) Symb => 增加行情相關欄位, 處理需要判斷條件的要求(例:OmsCxReqToSymb);
//  (3) UsrDef => 將觸發執行的條件單, 轉移到 OmsCore 執行;
// 其餘直接直接使用[無條件單]的環境即可;
// ----------
// - 條件單從 [OmsCxReqBase衍生者] 的 BeforeReqInCore() 呼叫 OmsCxReqBase::BeforeReqInCore_CreateCxExpr() 解析條件內容, 建立條件運算式開始;
// - 在風控前呼叫 OmsCxReqBase::ToWaitCond_BfSc<>(); 處理;
//   在風控後呼叫 OmsCxReqBase::ToWaitCond_AfSc<>(); 處理;
//   - 是否先風控?
//   - 條件是否已成立? 透過 OmsCxReqBase::RegCondToSymb_T<>(); 處理:
//     - 計算條件運算式: 
//       - 設定運算式裡面全部的 CxUnit 狀態: OmsCxUnit::CheckNeedsFireNow(); => OmsCxUnit::OmsCx_IsNeedsFireNow();
//       - 計算運算式的結果: OmsCxExpr::IsTrue();
//     - 若條件運算式已成立, 則不用進入等候狀態, 直接執行下單要求;
//     - 若條件運算式未成立, 則將判斷單元註冊到 Symb:
//       - YourSymb::RegCondReq() => OmsCxSymb::CondReqList_Add();
// - 在修改條件內容後,也可能會立即觸發執行下單:
//   - 透過 OmsCxReqBase::OnOmsCx_AssignReqChgToOrd() => OmsCxUnit::OnOmsCx_AssignReqChgToOrd(); 修改條件內容;
//   - 修改成功後, 透過 OmsCxReqBase::OnAfterChgCond_Checker_T<>(); 檢查:
//     - 檢查條件是否成立: OmsCxUnit::CheckNeedsFireNow(); => OmsCxUnit::OmsCx_IsNeedsFireNow();
//     - 若條件成立: OmsCxSymb::DelCondReq() 移除等候中的條件單元, 並取得 NextStep;
//       - 若有取得 NextStep 則執行下單: YourOmsCoreUsrDef::AddCondFiredReq();
// - 當商品有資料異動時,觸發條件檢查:
//   - 由 MdSystem(OmsTwsMdSystem,OmsTwfMiSystem) 收行情, 當有異動時呼叫對應的 virtual function; 參考: "f9omstw/OmsMdEvent.hpp"
//   - 然後由使用者的 Symb 處理事件:
//     - 轉發給 OmsCxSymb::CondReqListImpl::CheckCondReq();
//       - 通知 OmsCxReqBase::OnOmsCxMdEv() => OmsCxReqBase::OnOmsCxMdEv_T<>()
//          => 透過 OmsCxReqBase::OnOmsCx_CalcExpr(); 重新計算條件是否成立?
//          => 若成立,則執行下單: YourOmsCoreUsrDef::AddCondFiredReq();
//       - 根據 OmsCxReqBase::OnOmsCxMdEv() 返回結果, 決定是否移除 CondReq.
//
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCxBase_hpp__
#define __f9omstw_OmsCxBase_hpp__
#include "f9omstw/OmsMdEvent.hpp"

/// 註冊 f9omstw 提供的 OmsCxUnit Creator: LP,BP,SP,LQ,TQ,BQ,SQ;
extern "C" void f9omstw_Reg_Default_CxUnitCreator();

namespace f9omstw {

using OmsCxCondName = fon9::CharAry<2>;
using OmsCxCondOp = fon9::CharAry<2>;

class OmsCxUnit;
using OmsCxUnitUP = std::unique_ptr<OmsCxUnit>;
class OmsCxExpr;
using OmsCxExprUP = std::unique_ptr<OmsCxExpr>;
class OmsCxReqBase;

struct OmsCxCreatorArgs;
struct OmsCxCreatorInternal;

struct OmsCxReqToSymb {
   uint64_t             Priority_;
   const OmsCxReqBase*  CxRequest_;
   OmsCxUnit*           CxUnit_;
   OmsRequestRunStep*   NextStep_;
};
// ======================================================================== //
/// - 包含 CondPri 及 CondQty 的修改;
/// - 其他 Cond 欄位不允許修改, 若要調整, 應刪除後重下;
struct OmsCxBaseChangeable {
   /// 若為 Null, 則表示[市價].
   MdPri CondPri_{fon9::fmkt::Pri::Null()};
   MdQty CondQty_{0};

   template <class Derivid>
   static void CxMakeFields(fon9::seed::Fields& flds, const Derivid* = nullptr) {
      MakeFields(flds, fon9_OffsetOfBase(Derivid, OmsCxBaseChangeable));
   }
   static void MakeFields(fon9::seed::Fields& flds, int ofsadj);

   void ContinuePrevUpdate(const OmsCxBaseChangeable& prev) {
      *this = prev;
   }
};
//--------------------------------------------------------------------------//
using OmsCxBaseRaw = OmsCxBaseChangeable;
/// 若新單仍在等候條件, 且為單一條件, 則可以修改條件內容.
/// 可修改要條件內容(觸發的: 價、量);
/// 不能修改: 判斷式、CondSymb、CondName、CondOp...;
using OmsCxBaseChgDat = OmsCxBaseChangeable;
// ======================================================================== //
enum class OmsCxUnitSt : uint8_t {
   Initialing,
   /// 條件已加入 Symb 等候資料(行情)異動後判斷.
   /// 上次條件判斷時條件未成立,且整個運算式尚未成立,
   /// 續繼續等候資料(行情)異動, 可能下次異動時, 此單元會變為成立.
   FalseAndWaiting,
   /// 條件已加入 Symb 等候資料(行情)異動後判斷.
   /// 上次條件判斷時條件已成立,但整個運算式尚未成立,
   /// 續繼續等候資料(行情)異動, 可能下次異動時, 此單元會變成不成立.
   TrueAndWaiting,
   /// 條件已成立,不需要再等候.
   TrueLocked,
};
static inline bool IsOmsCxUnitSt_True(OmsCxUnitSt st) {
   return st >= OmsCxUnitSt::TrueAndWaiting;
}
//--------------------------------------------------------------------------//
/// OmsCxUnit 需要那些事件來判斷條件?
enum class OmsCxSrcEvMask : uint8_t {
   MdLastPrice = 0x01,
   MdLastPrice_OddLot = 0x02,
   MdBS = 0x04,
   MdBS_OddLot = 0x08,

   ForCheckFireNow = 0xff,
};
fon9_ENABLE_ENUM_BITWISE_OP(OmsCxSrcEvMask);
//--------------------------------------------------------------------------//
/// 條件內容包含哪些內容(Qty,Pri)? 條件內容的值如何才是正確?
enum class OmsCxCheckFlag : uint8_t {
   /// 需要Qty來判斷條件, 但若沒有 CondQtyCannotZero 則 Qty 可以是 0;
   QtyNeeds      = 0x01,
   QtyCannotZero = 0x02 | QtyNeeds,

   PriNeeds       = 0x10,
   PriAllowMarket = 0x20 | PriNeeds,

   /// 當條件成立時,以後的判斷都會條件成立.
   /// 例: TQ(總量) 的條件判斷;
   CxWaitTrueLock  = 0x40,
   UsrWaitTrueLock = 0x80,
};
fon9_ENABLE_ENUM_BITWISE_OP(OmsCxCheckFlag);
//--------------------------------------------------------------------------//
struct OmsCxCondRec : public OmsCxBaseChangeable {
   OmsCxCondName  Name_;
   OmsCxCondOp    Op_;
   /// 需要那些事件來判斷條件?
   OmsCxSrcEvMask SrcEvMask_{};
   OmsCxCheckFlag CheckFlags_{};
private:
   friend class OmsCxUnit;
   OmsCxUnitSt    mutable CurrentSt_{OmsCxUnitSt::Initialing};
   OmsCxSrcEvMask mutable RegEvMask_{};
};
// ======================================================================== //

} // namespaces
#endif//__f9omstw_OmsCxBase_hpp__
