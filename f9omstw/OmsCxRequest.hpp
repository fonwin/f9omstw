// \file f9omstw/OmsCxRequest.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCxRequest_hpp__
#define __f9omstw_OmsCxRequest_hpp__
#include "f9omstw/OmsMdEvent.hpp"
#include "f9omstw/OmsCx.h"
#include "f9omstw/OmsScBase.hpp"
#include "f9omstw/OmsSymb.hpp"
#include "fon9/ConfigUtils.hpp"

namespace f9omstw {

/// 需要那些事件來判斷條件?
enum class OmsCxSrcEvMask : uint8_t {
   MdLastPrice        = 0x01,
   MdLastPrice_OddLot = 0x02,
   MdBS               = 0x04,
   MdBS_OddLot        = 0x08,
};
fon9_ENABLE_ENUM_BITWISE_OP(OmsCxSrcEvMask);
// ======================================================================== //
/// - 包含 CondPri 及 CondQty 的修改;
/// - 其他 Cond 欄位不允許修改, 若要調整, 應刪除後重下;
struct OmsCxBaseChangeable {
   fon9::fmkt::Pri   CondPri_{fon9::fmkt::Pri::Null()};
   fon9::fmkt::Qty   CondQty_{0};

   template <class Derivid>
   static void CxMakeFields(fon9::seed::Fields& flds, const Derivid* = nullptr) {
      MakeFields(flds, fon9_OffsetOfBase(Derivid, OmsCxBaseChangeable));
   }
   static void MakeFields(fon9::seed::Fields& flds, int ofsadj);

   void ContinuePrevUpdate(const OmsCxBaseChangeable& prev) {
      *this = prev;
   }
};
/// 條件改單要求, 可包含 OmsCxBaseChgDat;
using OmsCxBaseChgDat = OmsCxBaseChangeable;
/// 若系統支援更改條件內容, 則 OrderRaw 需要紀錄[最後異動]的條件;
using OmsCxBaseOrdRaw = OmsCxBaseChangeable;
//--------------------------------------------------------------------------//
struct OmsCxBaseIniDat : public OmsCxBaseChangeable {
   /// 用來判斷條件的商品, 與下單商品可能不同.
   fon9::CharVector  CondSymbId_;
   fon9::CharAry<2>  CondName_{nullptr};
   fon9::CharAry<2>  CondOp_{nullptr};
   /// 先風控,風控通過後,才開始等候條件.
   fon9::EnabledYN   CondAfterSc_{};
   /// 需要那些事件來判斷條件?
   OmsCxSrcEvMask    CondSrcEvMask_{};
   char              CondPadding___[2];
   /// 某些判斷, 可能需要同時判斷 Pri 及 Qty, 所以建立2個獨立欄位, 因而不使用 union;

   template <class Derivid>
   static void CxMakeFields(fon9::seed::Fields& flds, const Derivid* = nullptr) {
      MakeFields(flds, fon9_OffsetOfBase(Derivid, OmsCxBaseIniDat));
   }
   static void MakeFields(fon9::seed::Fields& flds, int ofsadj);
};
//--------------------------------------------------------------------------//
class OmsCxBaseCondEvArgs {
   fon9_NON_COPY_NON_MOVE(OmsCxBaseCondEvArgs);
public:
   OmsSymb&                CondSymb_;
   /// 因為同一個商品異動事件, 可能會有多個條件等候中,
   /// 所以建構時先不用設定 CxRaw_;
   /// 等需要檢查(呼叫 CondFn_)時再填入;
   const OmsCxBaseOrdRaw*  CxRaw_;
   /// 若是在收單階段(OmsCore/OmsRequestRunStep)的判斷條件是否成立, 則 BfXXX_ 為 nullptr;
   /// 若是行情異動時的觸發, 則對應的 BfXXX_, AfXXX_ 都是有效的指標.
   const OmsMdLastPriceEv* AfMdLastPrice_{nullptr};
   const OmsMdBSEv*        AfMdBS_{nullptr};
   const OmsMdLastPrice*   BfMdLastPrice_{nullptr};
   const OmsMdBS*          BfMdBS_{nullptr};
   /// 若條件成立, 且 LogRBuf_ != nullptr; 則可在此建立 Log 字串.
   fon9::RevBuffer*        LogRBuf_{nullptr};

   OmsCxBaseCondEvArgs(OmsSymb& condSymb, const OmsMdLastPrice& bf, const OmsMdLastPriceEv& af)
      : CondSymb_(condSymb)
      , AfMdLastPrice_(&af)
      , BfMdLastPrice_(&bf){
   }
   OmsCxBaseCondEvArgs(OmsSymb& condSymb, const OmsMdBS& bf, const OmsMdBSEv& af)
      : CondSymb_(condSymb)
      , AfMdBS_(&af)
      , BfMdBS_(&bf) {
   }
   OmsCxBaseCondEvArgs(OmsSymb& condSymb, const OmsMdLastPriceEv* mdLastPrice, const OmsCxBaseOrdRaw& ordraw)
      : CondSymb_(condSymb)
      , CxRaw_(&ordraw)
      , AfMdLastPrice_(mdLastPrice) {
   }
   OmsCxBaseCondEvArgs(OmsSymb& condSymb, const OmsMdBSEv* mdBS, const OmsCxBaseOrdRaw& ordraw)
      : CondSymb_(condSymb)
      , CxRaw_(&ordraw)
      , AfMdBS_(mdBS) {
   }
   OmsCxBaseCondEvArgs(OmsSymb& condSymb, const OmsCxBaseOrdRaw& ordraw, OmsCxSrcEvMask evMask)
      : CondSymb_(condSymb)
      , CxRaw_(&ordraw)
      , AfMdLastPrice_(IsEnumContains(evMask, OmsCxSrcEvMask::MdLastPrice_OddLot) ? condSymb.GetMdLastPriceEv_OddLot() : condSymb.GetMdLastPriceEv())
      , AfMdBS_(IsEnumContains(evMask, OmsCxSrcEvMask::MdBS_OddLot) ? condSymb.GetMdBSEv_OddLot() : condSymb.GetMdBSEv()) {
   }
};

/// [檢查條件] 的基底.
/// 這裡衍生: 自訂的檢查方式(自訂策略).
struct OmsCxBaseCondFn {
   virtual ~OmsCxBaseCondFn();
   /// 這裡預設都是傳回 false;
   virtual bool OnOmsCx_MdLastPriceEv(const OmsCxBaseCondEvArgs& args);
   virtual bool OnOmsCx_MdLastPriceEv_OddLot(const OmsCxBaseCondEvArgs& args);
   /// 這裡預設都是傳回 false;
   virtual bool OnOmsCx_MdBSEv(const OmsCxBaseCondEvArgs& args);
   virtual bool OnOmsCx_MdBSEv_OddLot(const OmsCxBaseCondEvArgs& args);
   /// 檢查條件內容是否正確.
   /// 預設傳回 OmsErrCode_Null;
   virtual OmsErrCode OnOmsCx_CheckCondDat(const OmsCxBaseChangeable& dat);
   /// 若reqChg的內容允許(this->OnOmsCx_CheckCondDat(reqChg) 返回 OmsErrCode_Null),
   /// 則將修改後的內容填入 ordraw;
   virtual OmsErrCode OnOmsCx_AssignReqChgToOrd(const OmsCxBaseChgDat& reqChg, OmsCxBaseOrdRaw& ordraw);
   /// 收單階段(OmsCore/OmsRequestRunStep)的條件判斷.
   /// 這裡預設傳回 false;
   /// \retval true  則應立即送出, 不用等候條件.
   /// \retval false 需要等候條件, 成立才送出. 例: 最後成交單量,就不可在[收單階段]判斷,此時傳回false.
   virtual bool OmsCx_IsNeedsFireNow(OmsSymb& condSymb, const OmsCxBaseOrdRaw& cxRaw);
};
enum class OmsCxBaseCondFn_CheckFlag {
   CondQtyCannotZero = 0x01,
   CondPriCannotNull = 0x02,
};
fon9_ENABLE_ENUM_BITWISE_OP(OmsCxBaseCondFn_CheckFlag);
struct OmsCxBaseCondFn_CheckCond : public OmsCxBaseCondFn {
   OmsCxBaseCondFn_CheckFlag  CheckFlags_{};
   char                       Padding____[4];
   OmsCxBaseCondFn_CheckCond(OmsCxBaseCondFn_CheckFlag checkFlags) : CheckFlags_{checkFlags} {
   }
   ~OmsCxBaseCondFn_CheckCond();
   OmsErrCode OnOmsCx_CheckCondDat(const OmsCxBaseChangeable& dat) override;
};
using OmsCxBaseCondFnSP = std::unique_ptr<OmsCxBaseCondFn>;

/// 走[下單流程]的 Request 需要 CondFn_; 所以需繼承 OmsCxBaseIniFn;
/// 走[回報流程]的 Request 不需 CondFn_; 所以只要繼承 OmsCxBaseIniDat;
struct OmsCxBaseIniFn : public OmsCxBaseIniDat {
   fon9_NON_COPY_NON_MOVE(OmsCxBaseIniFn);
   OmsCxBaseIniFn() = default;
   virtual ~OmsCxBaseIniFn();
   /// 若 order 及條件單仍有效, 則取出條件要判斷的參數內容;
   virtual const OmsCxBaseOrdRaw* GetWaitingCxOrdRaw() const = 0;
   /// 若 order 及條件單仍有效, 則取出可執行的 RequestTrade;
   virtual const OmsRequestTrade* GetWaitingRequestTrade() const = 0;

   OmsCxBaseCondFnSP   CondFn_{};

   /// 根據 CondName_、CondOp_ 建立 CondFn_;
   /// 這裡支援的 CondName_:
   /// - "LP": 最後一筆成交價.
   /// - "LQ": 最後一筆成交量.
   /// - "TQ": 今日累計總成交量. CondOp_ 僅支援 > 及 >=
   /// - "BP": 最佳 [買進價]. 不考慮 [無買進報價] 的情況;
   /// - "SP": 最佳 [賣出價]. 不考慮 [無賣出報價] 的情況;
   /// - "BQ": 買進支撐. CondOp_ 不支援 == 及 !=
   ///   - 須配合買進報價及數量, 所以此時參數須包含: CondQty_ 及 CondPri_;
   ///   - 若沒有買進報價, 則: >, >= 條件不成立; <, <= 條件成立;
   /// - "SQ": 賣出壓力. CondOp 不支援 == 及 !=
   ///   - 須配合賣出報價及數量, 所以此時參數須包含: CondQty 及 CondPri;
   ///   - 若沒有賣出報價, 則: >, >= 條件成立;   <, <= 條件不成立;
   /// - 若以上[1]的 P,Q 為小寫, 則為判斷零股行情. (此時[0]仍維持大寫)
   ///
   /// \retval OmsErrCode_Null
   ///    - 沒有設定 CondName_、CondOp_;
   ///    - 有設定Cond及相關參數正確:建立 CondFn_;
   ///    - 條件判斷用不到的欄位, 可能直接忽略(不論填入任何內容);
   /// \retval OmsErrCode_CondSc_...
   ///    - 請參考 "f9omstw/OmsScBase.hpp" 裡面 OmsErrCode_CondSc_Adj 的定義;
   OmsErrCode CreateCondFn();
};
// ======================================================================== //
/// 協助合併 TBase::MakeFields() 及 TCxData::CxMakeFields();
template <class TBase, class TCxData>
class OmsCxCombine : public TBase, public TCxData {
   fon9_NON_COPY_NON_MOVE(OmsCxCombine);
   using base = TBase;
public:
   using base::base;
   OmsCxCombine() = default;
   static void MakeFields(fon9::seed::Fields& flds) {
      base::MakeFields(flds);
      TCxData::CxMakeFields(flds, static_cast<OmsCxCombine*>(nullptr));
   }
};

template <class TOrdRaw, class TCxData>
class OmsCxCombineOrdRaw : public OmsCxCombine<TOrdRaw, TCxData> {
   fon9_NON_COPY_NON_MOVE(OmsCxCombineOrdRaw);
   using base = OmsCxCombine<TOrdRaw, TCxData>;
public:
   using base::base;
   OmsCxCombineOrdRaw() = default;

   void ContinuePrevUpdate(const OmsOrderRaw& prev) override {
      if (fon9_LIKELY(prev.UpdateOrderSt_ != f9fmkt_OrderSt_NewWaitingCond))
         TOrdRaw::ContinuePrevUpdate(prev);
      else {
         // 在 WaitingCond 狀態下的委託: assert(LeavesQty_ != 0)
         // (1) 若 LeavesQty_ != AfterQty_ : assert(AfterQty_ == 0); 則為: [尚未風控] 的條件單.
         // (2) 若 LeavesQty_ == AfterQty_ :                         則為: [已經風控] 的條件單;
         const auto afQty = static_cast<const OmsCxCombineOrdRaw*>(&prev)->AfterQty_;
         TOrdRaw::ContinuePrevUpdate(prev);
         this->BeforeQty_ = this->AfterQty_ = afQty;
      }
      TCxData::ContinuePrevUpdate(*static_cast<const OmsCxCombineOrdRaw*>(&prev));
   }
};
//--------------------------------------------------------------------------//
/// 例: TBase = OmsTwsRequestIni; TCxData = OmsCxBaseIniFn;
/// 檢查 txReq 是否仍需要等候條件.
/// \retval req      仍在等候條件成立後送出.
/// \retval nullptr  條件已取消, 或 Order 已失效(剩餘量<=0).
template <class OrdRaw, class TxReq>
inline const OrdRaw* GetWaitingOrdRaw(const TxReq& txReq) {
   if (auto* ordraw = txReq.LastUpdated()) {
      auto& order = ordraw->Order();
      if (order.IsWorkingOrder()) {
         if (txReq.RxKind() == f9fmkt_RxKind_RequestNew) {
            // 新單尚未送出前, 可能會 [更改條件內容], 此時新單的 txReq.LastUpdated() 不是更改後的內容.
            // 所以委託的最後一次異動 order.Tail(), 才是最後的[新單條件內容];
            // 但這種判斷方式, 不支援 [新單還在等候條件] 時的 [條件改單],
            // 因為 order.Tail() 可能會是 [條件改單] 的內容;
            ordraw = order.Tail();
            return (ordraw->UpdateOrderSt_ == f9fmkt_OrderSt_NewWaitingCond
                    ? static_cast<const OrdRaw*>(ordraw) : nullptr);
         }
         if (ordraw->RequestSt_ == f9fmkt_TradingRequestSt_WaitingCond)
            return static_cast<const OrdRaw*>(ordraw);
      }
   }
   return nullptr;
}

template <class TxReq, class TCxData, class TOrdRaw>
class OmsCxCombineReqIni : public OmsCxCombine<TxReq, TCxData> {
   fon9_NON_COPY_NON_MOVE(OmsCxCombineReqIni);
   using base = OmsCxCombine<TxReq, TCxData>;
public:
   using base::base;
   OmsCxCombineReqIni() = default;
   const TOrdRaw* GetWaitingCxOrdRaw() const override {
      return GetWaitingOrdRaw<TOrdRaw>(*this);
   }
   const OmsCxCombineReqIni* GetWaitingRequestTrade() const override {
      return GetWaitingOrdRaw<TOrdRaw>(*this) ? this : nullptr;
   }
};
// ======================================================================== //
// 為了相容, 底下為僅支援 LastPrice 判斷的輔助類別.
// ======================================================================== //
/// - 包含 CondPri 的修改;
/// - 其他 Cond 欄位不允許修改, 若要調整, 應刪除後重下;
struct OmsCxLastPriceChangeable {
   fon9::fmkt::Pri   CondPri_{fon9::fmkt::Pri::Null()};

   template <class Derivid>
   static void CxMakeFields(fon9::seed::Fields& flds, const Derivid* = nullptr) {
      MakeFields(flds, fon9_OffsetOfBase(Derivid, OmsCxLastPriceChangeable));
   }
   static void MakeFields(fon9::seed::Fields& flds, int ofsadj);

   void ContinuePrevUpdate(const OmsCxLastPriceChangeable& prev) {
      this->CondPri_ = prev.CondPri_;
   }
};
using OmsCxLastPriceChgDat = OmsCxLastPriceChangeable;
using OmsCxLastPriceOrdRaw = OmsCxLastPriceChangeable;
//--------------------------------------------------------------------------//
struct OmsCxLastPriceIniDat {
   fon9::CharAry<2>  CondOp_{nullptr};
   char              Padding___[6];
   fon9::fmkt::Pri   CondPri_{fon9::fmkt::Pri::Null()};

   template <class Derivid>
   static void CxMakeFields(fon9::seed::Fields& flds, const Derivid* = nullptr) {
      MakeFields(flds, fon9_OffsetOfBase(Derivid, OmsCxLastPriceIniDat));
   }
   static void MakeFields(fon9::seed::Fields& flds, int ofsadj);
};
using OmsCxLastPriceCondFn = bool (*)(const OmsCxLastPriceOrdRaw& ordraw, const OmsMdLastPrice& bf, const OmsMdLastPriceEv& af);
/// 走[下單流程]的 Request 需要 CondFn_; 所以需繼承 OmsCxLastPriceIniFn;
/// 走[回報流程]的 Request 不需 CondFn_; 所以只要繼承 OmsCxLastPriceIniDat;
struct OmsCxLastPriceIniFn : public OmsCxLastPriceIniDat {
   OmsCxLastPriceCondFn    CondFn_{};
   /// 根據 CondOp_、CondPri_ 設定 CondFn_;
   /// 若沒有指定 CondOp_、CondPri_ 則: CondFn_ = nullptr, 返回 true,
   /// 若 CondOp_、CondPri_ 不正確, 則: CondFn_ = nullptr, 返回 false,
   bool ParseCondOp();
};
// ======================================================================== //

} // namespaces
#endif//__f9omstw_OmsCxRequest_hpp__
