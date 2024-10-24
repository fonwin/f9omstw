// \file f9omstw/OmsCxRequest.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCxRequest.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void OmsCxBaseIniDat::MakeFields(fon9::seed::Fields& flds, int ofsadj) {
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxBaseIniDat, CondName));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxBaseIniDat, CondSymbId));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxBaseIniDat, CondAfterSc));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxBaseIniDat, CondOp));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxBaseIniDat, CondPri));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxBaseIniDat, CondQty));
}
void OmsCxBaseChangeable::MakeFields(fon9::seed::Fields& flds, int ofsadj) {
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxBaseChangeable, CondPri));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxBaseChangeable, CondQty));
}
//--------------------------------------------------------------------------//
OmsCxBaseCondFn::~OmsCxBaseCondFn() {
}
bool OmsCxBaseCondFn::OnOmsCx_MdLastPriceEv(const OmsCxBaseCondEvArgs& args) {
   (void)args;
   return false;
}
bool OmsCxBaseCondFn::OnOmsCx_MdLastPriceEv_OddLot(const OmsCxBaseCondEvArgs& args) {
   (void)args;
   return false;
}
bool OmsCxBaseCondFn::OnOmsCx_MdBSEv(const OmsCxBaseCondEvArgs& args) {
   (void)args;
   return false;
}
bool OmsCxBaseCondFn::OnOmsCx_MdBSEv_OddLot(const OmsCxBaseCondEvArgs& args) {
   (void)args;
   return false;
}
OmsErrCode OmsCxBaseCondFn::OnOmsCx_CheckCondDat(const OmsCxBaseChangeable& dat) {
   (void)dat;
   return OmsErrCode_Null;
}
OmsErrCode OmsCxBaseCondFn::OnOmsCx_AssignReqChgToOrd(const OmsCxBaseChgDat& reqChg, OmsCxBaseOrdRaw& ordraw) {
   OmsErrCode retval = this->OnOmsCx_CheckCondDat(reqChg);
   if (retval == OmsErrCode_Null)
      ordraw = reqChg;
   return retval;
}
bool OmsCxBaseCondFn::OmsCx_IsNeedsFireNow(OmsSymb& condSymb, const OmsCxBaseOrdRaw& cxRaw) {
   (void)condSymb; (void)cxRaw;
   return false;
}
//--------------------------------------------------------------------------//
OmsCxBaseCondFn_CheckCond::~OmsCxBaseCondFn_CheckCond() {
}
OmsErrCode OmsCxBaseCondFn_CheckCond::OnOmsCx_CheckCondDat(const OmsCxBaseChangeable& dat) {
   if (IsEnumContains(this->CheckFlags_, OmsCxBaseCondFn_CheckFlag::CondQtyCannotZero) && dat.CondQty_ == 0)
      return OmsErrCode_CondSc_BadQty;
   if (IsEnumContains(this->CheckFlags_, OmsCxBaseCondFn_CheckFlag::CondPriCannotNull) && dat.CondPri_.IsNull())
      return OmsErrCode_CondSc_BadPri;
   return OmsErrCode_Null;
}
//--------------------------------------------------------------------------//
OmsCxBaseIniFn::~OmsCxBaseIniFn() {
}
//--------------------------------------------------------------------------//
typedef OmsCxBaseCondFn_CheckCond* (*OmsCxBaseCondFn_Creator) (OmsCxBaseIniDat& rthis);

#define CCF_None     OmsCxBaseCondFn_CheckFlag{}
#define CCF_Qty0     OmsCxBaseCondFn_CheckFlag::CondQtyCannotZero
#define CCF_PriN     OmsCxBaseCondFn_CheckFlag::CondPriCannotNull
#define DEFINE_OmsCxBaseCondFn_Creator(MdType, CondChkFlags, COND_OP, ODDLOT)               \
[](OmsCxBaseIniDat& rthis) {                                                                \
   struct OmsCxBaseCondFn_Local : public OmsCxBaseCondFn_CheckCond {                        \
      OmsCxBaseCondFn_Local() = delete;                                                     \
      using OmsCxBaseCondFn_CheckCond::OmsCxBaseCondFn_CheckCond;                           \
      bool OnOmsCx_##MdType##Ev##ODDLOT(const OmsCxBaseCondEvArgs& args) override {         \
         return COND_OP;                                                                    \
      }                                                                                     \
      bool OmsCx_IsNeedsFireNow(OmsSymb& condSymb, const OmsCxBaseOrdRaw& cxRaw) override { \
         OmsCxBaseCondEvArgs args{condSymb, condSymb.Get##MdType##Ev##ODDLOT(), cxRaw};     \
         return COND_OP;                                                                    \
      }                                                                                     \
   };                                                                                       \
   rthis.CondSrcEvMask_ = OmsCxSrcEvMask::MdType##ODDLOT;                                   \
   return static_cast<OmsCxBaseCondFn_CheckCond*>(new OmsCxBaseCondFn_Local(CondChkFlags)); \
}
#define DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdType, CondChkFlags, COND_OP, ODDLOT)     \
[](OmsCxBaseIniDat& rthis) {                                                                \
   struct OmsCxBaseCondFn_Local : public OmsCxBaseCondFn_CheckCond {                        \
      OmsCxBaseCondFn_Local() = delete;                                                     \
      using OmsCxBaseCondFn_CheckCond::OmsCxBaseCondFn_CheckCond;                           \
      bool OnOmsCx_##MdType##Ev##ODDLOT(const OmsCxBaseCondEvArgs& args) override {         \
         return COND_OP;                                                                    \
      }                                                                                     \
   };                                                                                       \
   rthis.CondSrcEvMask_ = OmsCxSrcEvMask::MdType##ODDLOT;                                   \
   return static_cast<OmsCxBaseCondFn_CheckCond*>(new OmsCxBaseCondFn_Local(CondChkFlags)); \
}

static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_LastPrice_Creators_OddLot[]{
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_PriN, (args.AfMdLastPrice_->LastQty_ > 0  &&  args.AfMdLastPrice_->LastPrice_  == args.CxRaw_->CondPri_), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_PriN, (args.AfMdLastPrice_->LastQty_ > 0  &&  args.AfMdLastPrice_->LastPrice_  != args.CxRaw_->CondPri_), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_PriN, (args.AfMdLastPrice_->LastQty_ > 0  &&  args.AfMdLastPrice_->LastPrice_  <= args.CxRaw_->CondPri_), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_PriN, (args.AfMdLastPrice_->LastQty_ > 0  &&  args.AfMdLastPrice_->LastPrice_  >= args.CxRaw_->CondPri_), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_PriN, (args.AfMdLastPrice_->LastQty_ > 0  &&  args.AfMdLastPrice_->LastPrice_  <  args.CxRaw_->CondPri_), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_PriN, (args.AfMdLastPrice_->LastQty_ > 0  &&  args.AfMdLastPrice_->LastPrice_  >  args.CxRaw_->CondPri_), _OddLot),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_BuyPri_Creators_OddLot[]{
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  == args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  != args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  <= args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  >= args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  <  args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  >  args.CxRaw_->CondPri_),  _OddLot),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_SellPri_Creators_OddLot[]{
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ == args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ != args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ <= args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ >= args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ <  args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ >  args.CxRaw_->CondPri_),  _OddLot),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_TotalQty_Creators_OddLot[]{
   nullptr, // 總成交量只會越來越大, 所以僅支援 > 及 >= 判斷.
   nullptr,
   nullptr,
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_Qty0, (args.AfMdLastPrice_->TotalQty_ >= args.CxRaw_->CondQty_), _OddLot), // CCF_Qty0: TotalQty >= 0, 則一定會立即觸發, 所以無意義;
   nullptr,
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_None, (args.AfMdLastPrice_->TotalQty_ >  args.CxRaw_->CondQty_), _OddLot),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_LastQty_Creators_OddLot[]{
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, CCF_Qty0, (args.AfMdLastPrice_->LastQty_ == args.CxRaw_->CondQty_), _OddLot), // LastQty == 0 : 永遠不會成立;
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, CCF_None, (args.AfMdLastPrice_->LastQty_ != args.CxRaw_->CondQty_), _OddLot), // LastQty != 0 : 只要有成交就成立;
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, CCF_Qty0, (args.AfMdLastPrice_->LastQty_ <= args.CxRaw_->CondQty_), _OddLot), // LastQty <= 0 : 永遠不會成立;
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, CCF_None, (args.AfMdLastPrice_->LastQty_ >= args.CxRaw_->CondQty_), _OddLot), // LastQty >= 0 : 只要有成交就成立;
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, CCF_Qty0, (args.AfMdLastPrice_->LastQty_ <  args.CxRaw_->CondQty_), _OddLot), // LastQty <  0 : 永遠不會成立;
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, CCF_None, (args.AfMdLastPrice_->LastQty_ >  args.CxRaw_->CondQty_), _OddLot), // LastQty >  0 : 只要有成交就成立;
};
// 買進支撐/賣出壓力: 必須要有報價(Qty_ > 0)才能判斷; 因為有可能買賣剛被吃完, 尚未有新報價;
#define CONDOP_BS_Qty(FLDBS, WhenNoQty, COND1, COND2)             \
   (args.AfMdBS_->FLDBS.Qty_ <= 0)                                \
   ? WhenNoQty                                                    \
   : (args.AfMdBS_->FLDBS.Pri_  COND1  args.CxRaw_->CondPri_      \
       || (args.AfMdBS_->FLDBS.Pri_ ==  args.CxRaw_->CondPri_     \
          && args.AfMdBS_->FLDBS.Qty_  COND1##COND2  args.CxRaw_->CondQty_))
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_BuyQty_Creators_OddLot[]{
   nullptr, // 買賣數量判斷,必須配合買賣報價,否則若僅判斷買賣數量,沒有任何意義.
   nullptr,
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Buy_, true,  <, =), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Buy_, false, >, =), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Buy_, true,  <,  ), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Buy_, false, >,  ), _OddLot),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_SellQty_Creators_OddLot[]{
   nullptr, // 買賣數量判斷,必須配合買賣報價,否則若僅判斷買賣數量,沒有任何意義.
   nullptr,
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Sell_, false, <, =), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Sell_, true,  >, =), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Sell_, false, <,  ), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Sell_, true,  >,  ), _OddLot),
};
//--------------------------------------------------------------------------//
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_LastPrice_Creators[]{
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_PriN, (args.AfMdLastPrice_->LastQty_   > 0  && args.AfMdLastPrice_->LastPrice_  == args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_PriN, (args.AfMdLastPrice_->LastQty_   > 0  && args.AfMdLastPrice_->LastPrice_  != args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_PriN, (args.AfMdLastPrice_->LastQty_   > 0  && args.AfMdLastPrice_->LastPrice_  <= args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_PriN, (args.AfMdLastPrice_->LastQty_   > 0  && args.AfMdLastPrice_->LastPrice_  >= args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_PriN, (args.AfMdLastPrice_->LastQty_   > 0  && args.AfMdLastPrice_->LastPrice_  <  args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_PriN, (args.AfMdLastPrice_->LastQty_   > 0  && args.AfMdLastPrice_->LastPrice_  >  args.CxRaw_->CondPri_), ),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_BuyPri_Creators[]{
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  == args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  != args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  <= args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  >= args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  <  args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  >  args.CxRaw_->CondPri_), ),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_SellPri_Creators[]{
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ == args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ != args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ <= args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ >= args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ <  args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        CCF_PriN, (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ >  args.CxRaw_->CondPri_), ),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_TotalQty_Creators[]{
   nullptr, // 總成交量只會越來越大, 所以僅支援 > 及 >= 判斷.
   nullptr,
   nullptr,
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_Qty0, (args.AfMdLastPrice_->TotalQty_ >= args.CxRaw_->CondQty_), ), // CCF_Qty0: TotalQty >= 0, 則一定會立即觸發, 所以無意義;
   nullptr,
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, CCF_None, (args.AfMdLastPrice_->TotalQty_ >  args.CxRaw_->CondQty_), ),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_LastQty_Creators[]{
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, CCF_Qty0, (args.AfMdLastPrice_->LastQty_ == args.CxRaw_->CondQty_), ), // LastQty == 0 : 永遠不會成立;
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, CCF_None, (args.AfMdLastPrice_->LastQty_ != args.CxRaw_->CondQty_), ), // LastQty != 0 : 只要有成交就成立;
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, CCF_Qty0, (args.AfMdLastPrice_->LastQty_ <= args.CxRaw_->CondQty_), ), // LastQty <= 0 : 永遠不會成立;
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, CCF_None, (args.AfMdLastPrice_->LastQty_ >= args.CxRaw_->CondQty_), ), // LastQty >= 0 : 只要有成交就成立;
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, CCF_Qty0, (args.AfMdLastPrice_->LastQty_ <  args.CxRaw_->CondQty_), ), // LastQty <  0 : 永遠不會成立;
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, CCF_None, (args.AfMdLastPrice_->LastQty_ >  args.CxRaw_->CondQty_), ), // LastQty >  0 : 只要有成交就成立;
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_BuyQty_Creators[]{
   nullptr, // 買賣數量判斷,必須配合買賣報價,否則若僅判斷買賣數量,沒有任何意義.
   nullptr,
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Buy_, true,  <, =), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Buy_, false, >, =), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Buy_, true,  <,  ), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Buy_, false, >,  ), ),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_SellQty_Creators[]{
   nullptr, // 買賣數量判斷,必須配合買賣報價,否則若僅判斷買賣數量,沒有任何意義.
   nullptr,
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Sell_, false, <, =), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Sell_, true,  >, =), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Sell_, false, <,  ), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CCF_PriN, CONDOP_BS_Qty(Sell_, true,  >,  ), ),
};
//--------------------------------------------------------------------------//
OmsErrCode OmsCxBaseIniFn::CreateCondFn() {
   if (fon9_LIKELY(this->CondOp_.empty1st() && this->CondName_.empty1st())) {
      // 沒設定 Cond_; 是否要判斷其他 cond 欄位?
      return OmsErrCode_Null;
   }
   f9oms_CondOp_Compare opcomp = f9oms_CondOp_Compare_Parse(this->CondOp_.Chars_);
   if (opcomp == f9oms_CondOp_Compare_Unknown)
      return OmsErrCode_CondSc_BadCondOp;
   OmsCxBaseCondFn_Creator* OmsCxBaseCondFn_Creators;
   switch (this->CondName_.Chars_[0]) {
   case 'L':
      switch (this->CondName_.Chars_[1]) {
      case 'P': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_LastPrice_Creators;          break;
      case 'Q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_LastQty_Creators;            break;
      case 'p': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_LastPrice_Creators_OddLot;   break;
      case 'q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_LastQty_Creators_OddLot;     break;
      default:  return OmsErrCode_CondSc_BadCondName;
      }
      break;
   case 'T':
      switch (this->CondName_.Chars_[1]) {
      case 'Q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_TotalQty_Creators;        break;
      case 'q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_TotalQty_Creators_OddLot; break;
      default:  return OmsErrCode_CondSc_BadCondName;
      }
      break;
   case 'B':
      switch (this->CondName_.Chars_[1]) {
      case 'P': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_BuyPri_Creators;          break;
      case 'Q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_BuyQty_Creators;          break;
      case 'p': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_BuyPri_Creators_OddLot;   break;
      case 'q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_BuyQty_Creators_OddLot;   break;
      default:  return OmsErrCode_CondSc_BadCondName;
      }
      break;
   case 'S':
      switch (this->CondName_.Chars_[1]) {
      case 'P': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_SellPri_Creators;         break;
      case 'Q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_SellQty_Creators;         break;
      case 'p': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_SellPri_Creators_OddLot;  break;
      case 'q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_SellQty_Creators_OddLot;  break;
      default:  return OmsErrCode_CondSc_BadCondName;
      }
      break;
   default:
      return OmsErrCode_CondSc_BadCondName;
   }
   auto fnCreator = OmsCxBaseCondFn_Creators[opcomp];
   if (fnCreator == nullptr)
      return OmsErrCode_CondSc_BadCondOp;
   OmsCxBaseCondFnSP condFn(fnCreator(*this));
   const OmsErrCode  retval = condFn->OnOmsCx_CheckCondDat(*this);
   if (retval == OmsErrCode_Null)
      this->CondFn_ = std::move(condFn);
   return retval;
}
// ======================================================================== //
void OmsCxLastPriceIniDat::MakeFields(fon9::seed::Fields& flds, int ofsadj) {
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxLastPriceIniDat, CondOp));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxLastPriceIniDat, CondPri));
}
void OmsCxLastPriceChangeable::MakeFields(fon9::seed::Fields& flds, int ofsadj) {
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, OmsCxLastPriceChangeable, CondPri));
}
//--------------------------------------------------------------------------//
#define DEFINE_LastPrice_Cond_Fn(OP)                                                           \
[](const OmsCxLastPriceOrdRaw& ordraw, const OmsMdLastPrice& bf, const OmsMdLastPriceEv& af) { \
   (void)bf;                                                                                   \
   return af.LastPrice_  OP  ordraw.CondPri_;                                                  \
}

typedef bool (*LastPriceCondFnT)(const OmsCxLastPriceOrdRaw& ordraw, const OmsMdLastPrice& bf, const OmsMdLastPriceEv& af);
static LastPriceCondFnT LastPriceCondFn[] = {
   DEFINE_LastPrice_Cond_Fn(== ),
   DEFINE_LastPrice_Cond_Fn(!= ),
   DEFINE_LastPrice_Cond_Fn(<= ),
   DEFINE_LastPrice_Cond_Fn(>= ),
   DEFINE_LastPrice_Cond_Fn(<  ),
   DEFINE_LastPrice_Cond_Fn(>  ),
};

bool OmsCxLastPriceIniFn::ParseCondOp() {
   if (fon9_LIKELY(this->CondOp_.empty1st())) {
      // 沒設定 CondOp_; 是否要判斷其他 cond 欄位?
      return true;
   }
   f9oms_CondOp_Compare opcomp = f9oms_CondOp_Compare_Parse(this->CondOp_.Chars_);
   if (opcomp == f9oms_CondOp_Compare_Unknown)
      return false;
   this->CondFn_ = LastPriceCondFn[opcomp];
   return true;
}

} // namespaces
