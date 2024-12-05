// \file f9omstw/OmsCxUnit_Default.cpp
//
// f9omstw 預設提供的條件判斷單元:
//    LP 行情成交價
//    BP 行情最佳買進價
//    SP 行情最佳賣出價
//    LQ 行情成交單量(單筆撮合量)
//    TQ 行情成交總量
//    BQ 行情買進支撐
//    SQ 行情賣出壓力
// [第2碼小寫] 表示使用零股行情;
//
// \author fonwinz@gmail.com
#include "f9omstw/OmsCxCreator.hpp"
#include "f9omstw/OmsScBase.hpp"

namespace f9omstw {

// ======================================================================== //
#define DEF_CxUnit_Deal_EvMask         OmsCxSrcEvMask::MdLastPrice
#define DEF_CxUnit_Deal_EvMask_OddLot  OmsCxSrcEvMask::MdLastPrice_OddLot
#define DEF_CxUnit_Deal(_CondRunName, _ODDLOT)                                         \
struct CxUnit_Local : public OmsCxUnit {                                               \
   fon9_NON_COPY_NON_MOVE(CxUnit_Local);                                               \
   using base = OmsCxUnit;                                                             \
   using base::base;                                                                   \
   bool OnOmsCx_MdLastPriceEv##_ODDLOT(const OmsCxMdEvArgs& args) override {           \
      return CondRun##_CondRunName(args);                                              \
   }                                                                                   \
   bool OmsCx_IsNeedsFireNow(OmsSymb& condSymb, const OmsCxBaseRaw& cxRaw) override {  \
      OmsCxMdEvArgs args{condSymb, condSymb.GetMdLastPriceEv##_ODDLOT(), cxRaw};       \
      return CondRun##_CondRunName(args);                                              \
   }                                                                                   \
}
#define DEF_CxUnit_BS_EvMask         OmsCxSrcEvMask::MdBS
#define DEF_CxUnit_BS_EvMask_OddLot  OmsCxSrcEvMask::MdBS_OddLot
#define DEF_CxUnit_BS(_CondRunName, _ODDLOT)                                           \
struct CxUnit_Local : public OmsCxUnit {                                               \
   fon9_NON_COPY_NON_MOVE(CxUnit_Local);                                               \
   using base = OmsCxUnit;                                                             \
   using base::base;                                                                   \
   bool OnOmsCx_MdBSEv##_ODDLOT(const OmsCxMdEvArgs& args) override {                  \
      return CondRun##_CondRunName(args);                                              \
   }                                                                                   \
   bool OmsCx_IsNeedsFireNow(OmsSymb& condSymb, const OmsCxBaseRaw& cxRaw) override {  \
      OmsCxMdEvArgs args{condSymb, condSymb.GetMdBSEv##_ODDLOT(), cxRaw};              \
      return CondRun##_CondRunName(args);                                              \
   }                                                                                   \
}
// ======================================================================== //
#define case_Compare_Op(DEF_CxUnit_class, _CondName, _OPNAME, _ODDLOT) \
   case f9oms_CondOp_Compare##_OPNAME: {                               \
         DEF_CxUnit_class(_CondName##_OPNAME, _ODDLOT);                \
         return OmsCxUnitUP{new CxUnit_Local{args}};                   \
      }

#define DEF_CxUnit_Creator(DEF_CxUnit_class, _CondName, _ODDLOT)                 \
static OmsCxUnitUP CxUnit_Creator##_CondName##_ODDLOT (OmsCxCreatorArgs& args) { \
   if (fon9_UNLIKELY(!CxUnit_Parse##_CondName(args)))                            \
      return nullptr;                                                            \
   args.Cond_.SrcEvMask_ = DEF_CxUnit_class##_EvMask##_ODDLOT;                   \
   switch (args.CompOp_) {                                                       \
   default:                                                                      \
   case f9oms_CondOp_Compare_Unknown:                                            \
      args.RequestAbandon(OmsErrCode_CondSc_BadCondOp);                          \
      return nullptr;                                                            \
   case_Compare_Op(DEF_CxUnit_class, _CondName, _EQ,   _ODDLOT)                  \
   case_Compare_Op(DEF_CxUnit_class, _CondName, _NE,   _ODDLOT)                  \
   case_Compare_Op(DEF_CxUnit_class, _CondName, _LEQ,  _ODDLOT)                  \
   case_Compare_Op(DEF_CxUnit_class, _CondName, _GEQ,  _ODDLOT)                  \
   case_Compare_Op(DEF_CxUnit_class, _CondName, _Less, _ODDLOT)                  \
   case_Compare_Op(DEF_CxUnit_class, _CondName, _Grea, _ODDLOT)                  \
   }                                                                             \
}
// ======================================================================== //
static inline bool CxUnit_Parse_LP(OmsCxCreatorArgs& args) {
   return args.ParseCondArgsPQ(OmsCxCheckFlag::PriNeeds);
}

#define DEF_CondRun_LP(_CondName, CONDOP)                                                                     \
static inline bool CondRun_LP##_CondName(const OmsCxMdEvArgs& args) {                                         \
   return(args.AfMdLastPrice_->LastQty_ > 0 && args.AfMdLastPrice_->LastPrice_ CONDOP args.CxRaw_->CondPri_); \
}

DEF_CondRun_LP(_EQ,   == )
DEF_CondRun_LP(_NE,   != )
DEF_CondRun_LP(_LEQ,  <= )
DEF_CondRun_LP(_GEQ,  >= )
DEF_CondRun_LP(_Less, <  )
DEF_CondRun_LP(_Grea, >  )

DEF_CxUnit_Creator(DEF_CxUnit_Deal, _LP, )
DEF_CxUnit_Creator(DEF_CxUnit_Deal, _LP, _OddLot)
//--------------------------------------------------------------------------//
#define DEF_CondRun_BSP(_CondRunName, FldBS, CONDOP)                                                                         \
static inline bool CondRun##_CondRunName(const OmsCxMdEvArgs& args) {                                                        \
   return (args.AfMdBS_->FldBS##_.Qty_ > 0  &&  args.AfMdBS_->FldBS##_.Pri_ CONDOP CondPriTo##FldBS(args.CxRaw_->CondPri_)); \
}
// ----------
DEF_CondRun_BSP(_BP_EQ,   Buy, == )
DEF_CondRun_BSP(_BP_NE,   Buy, != )
DEF_CondRun_BSP(_BP_LEQ,  Buy, <= )
DEF_CondRun_BSP(_BP_GEQ,  Buy, >= )
DEF_CondRun_BSP(_BP_Less, Buy, <  )
DEF_CondRun_BSP(_BP_Grea, Buy, >  )
static inline bool CxUnit_Parse_BP(OmsCxCreatorArgs& args) {
   return args.ParseCondArgsPQ(OmsCxCheckFlag::PriAllowMarket);
}
DEF_CxUnit_Creator(DEF_CxUnit_BS, _BP, )
DEF_CxUnit_Creator(DEF_CxUnit_BS, _BP, _OddLot)
// ----------
// VC 使用 _SP 會有問題, 所以改成 _SPri
DEF_CondRun_BSP(_SPri_EQ,   Sell, == )
DEF_CondRun_BSP(_SPri_NE,   Sell, != )
DEF_CondRun_BSP(_SPri_LEQ,  Sell, <= )
DEF_CondRun_BSP(_SPri_GEQ,  Sell, >= )
DEF_CondRun_BSP(_SPri_Less, Sell, <  )
DEF_CondRun_BSP(_SPri_Grea, Sell, >  )
static inline bool CxUnit_Parse_SPri(OmsCxCreatorArgs& args) {
   return args.ParseCondArgsPQ(OmsCxCheckFlag::PriAllowMarket);
}
DEF_CxUnit_Creator(DEF_CxUnit_BS, _SPri, )
DEF_CxUnit_Creator(DEF_CxUnit_BS, _SPri, _OddLot)
// ======================================================================== //
static inline bool CxUnit_Parse_LQ(OmsCxCreatorArgs& args) {
   constexpr OmsCxCheckFlag kChkFlags[] = {
      OmsCxCheckFlag::QtyNeeds | OmsCxCheckFlag::QtyCannotZero, // 等於,     ==, =
      OmsCxCheckFlag::QtyNeeds,                                 // 不等於,   !=, ≠    !=0:表示開盤首筆成交;
      OmsCxCheckFlag::QtyNeeds | OmsCxCheckFlag::QtyCannotZero, // 小於等於, <=, ≤
      OmsCxCheckFlag::QtyNeeds | OmsCxCheckFlag::QtyCannotZero, // 大於等於, >=, ≥
      OmsCxCheckFlag::QtyNeeds | OmsCxCheckFlag::QtyCannotZero, // 小於,     <
      OmsCxCheckFlag::QtyNeeds,                                 // 大於,     >        >0:表示開盤首筆成交;
   };
   const auto idx = fon9::unsigned_cast(args.CompOp_);
   return args.ParseCondArgsPQ(idx >= fon9::numofele(kChkFlags) ? OmsCxCheckFlag::QtyNeeds : kChkFlags[idx]);
}

#define DEF_CondRun_LQ(_CondName, CONDOP)                              \
static inline bool CondRun_LQ##_CondName(const OmsCxMdEvArgs& args) {  \
   return(args.AfMdLastPrice_->LastQty_ CONDOP args.CxRaw_->CondQty_); \
}
DEF_CondRun_LQ(_EQ,   == )
DEF_CondRun_LQ(_NE,   != )
DEF_CondRun_LQ(_LEQ,  <= )
DEF_CondRun_LQ(_GEQ,  >= )
DEF_CondRun_LQ(_Less, <  )
DEF_CondRun_LQ(_Grea, >  )

DEF_CxUnit_Creator(DEF_CxUnit_Deal, _LQ, )
DEF_CxUnit_Creator(DEF_CxUnit_Deal, _LQ, _OddLot)
//--------------------------------------------------------------------------//
static bool CxUnit_Parse_TQ(OmsCxCreatorArgs& args) {
   constexpr OmsCxCheckFlag kChkFlagsTQ = OmsCxCheckFlag::QtyNeeds | OmsCxCheckFlag::CxWaitTrueLock;
   constexpr OmsCxCheckFlag kChkFlagsN0 = kChkFlagsTQ | OmsCxCheckFlag::QtyCannotZero;
   OmsCxCheckFlag chkFlags;
   switch (args.CompOp_) {
   default:
   case f9oms_CondOp_Compare_Unknown:
   case f9oms_CondOp_Compare_EQ:
   case f9oms_CondOp_Compare_NE:
   case f9oms_CondOp_Compare_LEQ:
   case f9oms_CondOp_Compare_Less:
      args.RequestAbandon(OmsErrCode_CondSc_BadCondOp);
      return false;
   case f9oms_CondOp_Compare_GEQ:
      chkFlags = kChkFlagsN0;
      break;
   case f9oms_CondOp_Compare_Grea:
      chkFlags = kChkFlagsTQ;
      break;
   }
   return args.ParseCondArgsPQ(chkFlags);
}

#define DEF_CondRun_TQ(_OpName, CONDOP)                                 \
static inline bool CondRun_TQ##_OpName(const OmsCxMdEvArgs& args) {     \
   return(args.AfMdLastPrice_->TotalQty_ CONDOP args.CxRaw_->CondQty_); \
}
DEF_CondRun_TQ(_GEQ,  >= )
DEF_CondRun_TQ(_Grea, >  )

fon9_WARN_DISABLE_SWITCH
#define DEF_CxUnit_Creator_TQ(DEF_CxUnit_class, _ODDLOT)                 \
static OmsCxUnitUP CxUnit_Creator_TQ##_ODDLOT (OmsCxCreatorArgs& args) { \
   if (fon9_UNLIKELY(!CxUnit_Parse_TQ(args)))                            \
      return nullptr;                                                    \
   args.Cond_.SrcEvMask_ = DEF_CxUnit_Deal_EvMask##_ODDLOT;              \
   switch (args.CompOp_) {                                               \
   default: /* 其他 case 已在 CxUnit_Parse_TQ() 排除 */                   \
   case_Compare_Op(DEF_CxUnit_class, _TQ, _GEQ,  _ODDLOT)                \
   case_Compare_Op(DEF_CxUnit_class, _TQ, _Grea, _ODDLOT)                \
   }                                                                     \
}
DEF_CxUnit_Creator_TQ(DEF_CxUnit_Deal, )
DEF_CxUnit_Creator_TQ(DEF_CxUnit_Deal, _OddLot)
fon9_WARN_POP
//--------------------------------------------------------------------------//
#define DEF_CondRun_BSQ(_CondRunName, FldBS, WhenNoQty, COND_PRI, COND_QTY)   \
static bool CondRun##_CondRunName(const OmsCxMdEvArgs& args) {                \
   if (args.AfMdBS_->FldBS##_.Qty_ <= 0)                                      \
      return WhenNoQty;                                                       \
   const auto condPri = CondPriTo##FldBS(args.CxRaw_->CondPri_);              \
   if (args.AfMdBS_->FldBS##_.Pri_  COND_PRI  condPri)                        \
      return true;                                                            \
   return (args.AfMdBS_->FldBS##_.Pri_  ==        condPri                     \
        && args.AfMdBS_->FldBS##_.Qty_  COND_QTY  args.CxRaw_->CondQty_);     \
}
static inline bool CxUnit_Parse_BSQ(OmsCxCreatorArgs& args) {
   switch (args.CompOp_) {
   default:
   case f9oms_CondOp_Compare_Unknown:
   case f9oms_CondOp_Compare_EQ:
   case f9oms_CondOp_Compare_NE:
      args.RequestAbandon(OmsErrCode_CondSc_BadCondOp);
      return false;
   case f9oms_CondOp_Compare_LEQ:
   case f9oms_CondOp_Compare_GEQ:
   case f9oms_CondOp_Compare_Less:
   case f9oms_CondOp_Compare_Grea:
      return args.ParseCondArgsPQ(OmsCxCheckFlag::QtyCannotZero | OmsCxCheckFlag::PriAllowMarket);
   }
}
#define DEF_CxUnit_Creator_BSQ(DEF_CxUnit_class, _CondName, _ODDLOT)             \
static OmsCxUnitUP CxUnit_Creator##_CondName##_ODDLOT (OmsCxCreatorArgs& args) { \
   if (fon9_UNLIKELY(!CxUnit_Parse##_CondName(args)))                            \
      return nullptr;                                                            \
   args.Cond_.SrcEvMask_ = DEF_CxUnit_BS_EvMask##_ODDLOT;                        \
   switch (args.CompOp_) {                                                       \
   default: /* 已在 CxUnit_Parse_BSQ() 判斷過 */                                  \
   case_Compare_Op(DEF_CxUnit_class, _CondName, _LEQ,  _ODDLOT)                  \
   case_Compare_Op(DEF_CxUnit_class, _CondName, _GEQ,  _ODDLOT)                  \
   case_Compare_Op(DEF_CxUnit_class, _CondName, _Less, _ODDLOT)                  \
   case_Compare_Op(DEF_CxUnit_class, _CondName, _Grea, _ODDLOT)                  \
   }                                                                             \
}
// ----------
static inline bool CxUnit_Parse_BQ(OmsCxCreatorArgs& args) {
   return CxUnit_Parse_BSQ(args);
}
DEF_CondRun_BSQ(_BQ_LEQ,  Buy, true,  <, <= )
DEF_CondRun_BSQ(_BQ_GEQ,  Buy, false, >, >= )
DEF_CondRun_BSQ(_BQ_Less, Buy, true,  <, <  )
DEF_CondRun_BSQ(_BQ_Grea, Buy, false, >, >  )

fon9_WARN_DISABLE_SWITCH
DEF_CxUnit_Creator_BSQ(DEF_CxUnit_BS, _BQ, )
DEF_CxUnit_Creator_BSQ(DEF_CxUnit_BS, _BQ, _OddLot)
fon9_WARN_POP
// ----------
static inline bool CxUnit_Parse_SQ(OmsCxCreatorArgs& args) {
   return CxUnit_Parse_BSQ(args);
}
DEF_CondRun_BSQ(_SQ_LEQ,  Sell, false, <, >= )
DEF_CondRun_BSQ(_SQ_GEQ,  Sell, true,  >, <= )
DEF_CondRun_BSQ(_SQ_Less, Sell, false, <, >  )
DEF_CondRun_BSQ(_SQ_Grea, Sell, true,  >, <  )

fon9_WARN_DISABLE_SWITCH
DEF_CxUnit_Creator_BSQ(DEF_CxUnit_BS, _SQ, )
DEF_CxUnit_Creator_BSQ(DEF_CxUnit_BS, _SQ, _OddLot)
fon9_WARN_POP
// ======================================================================== //

} // namespaces

extern "C" void f9omstw_Reg_Default_CxUnitCreator() {
   AddCxUnitCreator("LP", &f9omstw::CxUnit_Creator_LP);
   AddCxUnitCreator("Lp", &f9omstw::CxUnit_Creator_LP_OddLot);
   AddCxUnitCreator("BP", &f9omstw::CxUnit_Creator_BP);
   AddCxUnitCreator("Bp", &f9omstw::CxUnit_Creator_BP_OddLot);
   AddCxUnitCreator("SP", &f9omstw::CxUnit_Creator_SPri);
   AddCxUnitCreator("Sp", &f9omstw::CxUnit_Creator_SPri_OddLot);
   AddCxUnitCreator("LQ", &f9omstw::CxUnit_Creator_LQ);
   AddCxUnitCreator("Lq", &f9omstw::CxUnit_Creator_LQ_OddLot);
   AddCxUnitCreator("TQ", &f9omstw::CxUnit_Creator_TQ);
   AddCxUnitCreator("Tq", &f9omstw::CxUnit_Creator_TQ_OddLot);
   AddCxUnitCreator("BQ", &f9omstw::CxUnit_Creator_BQ);
   AddCxUnitCreator("Bq", &f9omstw::CxUnit_Creator_BQ_OddLot);
   AddCxUnitCreator("SQ", &f9omstw::CxUnit_Creator_SQ);
   AddCxUnitCreator("Sq", &f9omstw::CxUnit_Creator_SQ_OddLot);
}
