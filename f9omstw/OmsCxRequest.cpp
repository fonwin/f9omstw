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
bool OmsCxBaseCondFn::OmsCx_IsNeedsFireNow(OmsSymb& condSymb, const OmsCxBaseOrdRaw& cxRaw) {
   (void)condSymb; (void)cxRaw;
   return false;
}
//--------------------------------------------------------------------------//
OmsCxBaseIniFn::~OmsCxBaseIniFn() {
}
//--------------------------------------------------------------------------//
typedef OmsCxBaseCondFn* (*OmsCxBaseCondFn_Creator) (OmsCxBaseIniDat& rthis);

#define DEFINE_OmsCxBaseCondFn_Creator(MdType, COND_OP, ODDLOT)                             \
[](OmsCxBaseIniDat& rthis) {                                                                \
   struct OmsCxBaseCondFn_Local : public OmsCxBaseCondFn {                                  \
      bool OnOmsCx_##MdType##Ev##ODDLOT(const OmsCxBaseCondEvArgs& args) override {         \
         return COND_OP;                                                                    \
      }                                                                                     \
      bool OmsCx_IsNeedsFireNow(OmsSymb& condSymb, const OmsCxBaseOrdRaw& cxRaw) override { \
         OmsCxBaseCondEvArgs args{condSymb, condSymb.Get##MdType##Ev##ODDLOT(), cxRaw};     \
         return COND_OP;                                                                    \
      }                                                                                     \
   };                                                                                       \
   rthis.CondSrcEvMask_ = OmsCxSrcEvMask::MdType##ODDLOT;                                   \
   return static_cast<OmsCxBaseCondFn*>(new OmsCxBaseCondFn_Local);                         \
}
#define DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdType, COND_OP, ODDLOT)           \
[](OmsCxBaseIniDat& rthis) {                                                        \
   struct OmsCxBaseCondFn_Local : public OmsCxBaseCondFn {                          \
      bool OnOmsCx_##MdType##Ev##ODDLOT(const OmsCxBaseCondEvArgs& args) override { \
         return COND_OP;                                                            \
      }                                                                             \
   };                                                                               \
   rthis.CondSrcEvMask_ = OmsCxSrcEvMask::MdType##ODDLOT;                           \
   return static_cast<OmsCxBaseCondFn*>(new OmsCxBaseCondFn_Local);                 \
}

static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_LastPrice_Creators_OddLot[]{
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_ > 0  &&  args.AfMdLastPrice_->LastPrice_  == args.CxRaw_->CondPri_), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_ > 0  &&  args.AfMdLastPrice_->LastPrice_  != args.CxRaw_->CondPri_), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_ > 0  &&  args.AfMdLastPrice_->LastPrice_  <= args.CxRaw_->CondPri_), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_ > 0  &&  args.AfMdLastPrice_->LastPrice_  >= args.CxRaw_->CondPri_), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_ > 0  &&  args.AfMdLastPrice_->LastPrice_  <  args.CxRaw_->CondPri_), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_ > 0  &&  args.AfMdLastPrice_->LastPrice_  >  args.CxRaw_->CondPri_), _OddLot),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_BuyPri_Creators_OddLot[]{
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  == args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  != args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  <= args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  >= args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  <  args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  >  args.CxRaw_->CondPri_),  _OddLot),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_SellPri_Creators_OddLot[]{
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ == args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ != args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ <= args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ >= args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ <  args.CxRaw_->CondPri_),  _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ >  args.CxRaw_->CondPri_),  _OddLot),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_TotalQty_Creators_OddLot[]{
   nullptr, // 總成交量只會越來越大, 所以僅支援 > 及 >= 判斷.
   nullptr,
   nullptr,
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->TotalQty_ >= args.CxRaw_->CondQty_), _OddLot),
   nullptr,
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->TotalQty_ >  args.CxRaw_->CondQty_), _OddLot),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_LastQty_Creators_OddLot[]{
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_ == args.CxRaw_->CondQty_), _OddLot),
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_ != args.CxRaw_->CondQty_), _OddLot),
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_ <= args.CxRaw_->CondQty_), _OddLot),
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_ >= args.CxRaw_->CondQty_), _OddLot),
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_ <  args.CxRaw_->CondQty_), _OddLot),
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_ >  args.CxRaw_->CondQty_), _OddLot),
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
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Buy_, true,  <, =), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Buy_, false, >, =), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Buy_, true,  <,  ), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Buy_, false, >,  ), _OddLot),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_SellQty_Creators_OddLot[]{
   nullptr, // 買賣數量判斷,必須配合買賣報價,否則若僅判斷買賣數量,沒有任何意義.
   nullptr,
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Sell_, false, <, =), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Sell_, true,  >, =), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Sell_, false, <,  ), _OddLot),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Sell_, true,  >,  ), _OddLot),
};
//--------------------------------------------------------------------------//
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_LastPrice_Creators[]{
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_   > 0  && args.AfMdLastPrice_->LastPrice_  == args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_   > 0  && args.AfMdLastPrice_->LastPrice_  != args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_   > 0  && args.AfMdLastPrice_->LastPrice_  <= args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_   > 0  && args.AfMdLastPrice_->LastPrice_  >= args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_   > 0  && args.AfMdLastPrice_->LastPrice_  <  args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_   > 0  && args.AfMdLastPrice_->LastPrice_  >  args.CxRaw_->CondPri_), ),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_BuyPri_Creators[]{
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  == args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  != args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  <= args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  >= args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  <  args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Buy_.Qty_  > 0  &&  args.AfMdBS_->Buy_.Pri_  >  args.CxRaw_->CondPri_), ),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_SellPri_Creators[]{
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ == args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ != args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ <= args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ >= args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ <  args.CxRaw_->CondPri_), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS,        (args.AfMdBS_->Sell_.Qty_ > 0  &&  args.AfMdBS_->Sell_.Pri_ >  args.CxRaw_->CondPri_), ),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_TotalQty_Creators[]{
   nullptr, // 總成交量只會越來越大, 所以僅支援 > 及 >= 判斷.
   nullptr,
   nullptr,
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->TotalQty_ >= args.CxRaw_->CondQty_), ),
   nullptr,
   DEFINE_OmsCxBaseCondFn_Creator(MdLastPrice, (args.AfMdLastPrice_->TotalQty_ >  args.CxRaw_->CondQty_), ),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_LastQty_Creators[]{
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_  == args.CxRaw_->CondQty_), ),
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_  != args.CxRaw_->CondQty_), ),
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_  <= args.CxRaw_->CondQty_), ),
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_  >= args.CxRaw_->CondQty_), ),
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_  <  args.CxRaw_->CondQty_), ),
   DEFINE_OmsCxBaseCondFn_NoFireNow_Creator(MdLastPrice, (args.AfMdLastPrice_->LastQty_  >  args.CxRaw_->CondQty_), ),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_BuyQty_Creators[]{
   nullptr, // 買賣數量判斷,必須配合買賣報價,否則若僅判斷買賣數量,沒有任何意義.
   nullptr,
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Buy_, true,  <, =), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Buy_, false, >, =), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Buy_, true,  <,  ), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Buy_, false, >,  ), ),
};
static OmsCxBaseCondFn_Creator OmsCxBaseCondFn_SellQty_Creators[]{
   nullptr, // 買賣數量判斷,必須配合買賣報價,否則若僅判斷買賣數量,沒有任何意義.
   nullptr,
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Sell_, false, <, =), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Sell_, true,  >, =), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Sell_, false, <,  ), ),
   DEFINE_OmsCxBaseCondFn_Creator(MdBS, CONDOP_BS_Qty(Sell_, true,  >,  ), ),
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
      case 'P': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_LastPrice_Creators;          goto __CHECK_CondPri;
      case 'Q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_LastQty_Creators;            break;
      case 'p': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_LastPrice_Creators_OddLot;   goto __CHECK_CondPri;
      case 'q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_LastQty_Creators_OddLot;     break;
      default:  return OmsErrCode_CondSc_BadCondName;
      }
      if (this->CondQty_ == 0) {
         switch (opcomp) {
         default:
         case f9oms_CondOp_Compare_Unknown:
         case f9oms_CondOp_Compare_NE:  // LastQty != 0 : 只要有成交就成立;
         case f9oms_CondOp_Compare_Grea:// LastQty >  0 : 只要有成交就成立;
         case f9oms_CondOp_Compare_GEQ: // LastQty >= 0 : 只要有成交就成立;
            break;
         case f9oms_CondOp_Compare_EQ:  // LastQty == 0 : 永遠不會成立;
         case f9oms_CondOp_Compare_LEQ: // LastQty <= 0 : 永遠不會成立;
         case f9oms_CondOp_Compare_Less:// LastQty <  0 : 永遠不會成立;
            return OmsErrCode_CondSc_BadQty;
         }
      }
      break;
   case 'T':
      switch (this->CondName_.Chars_[1]) {
      case 'Q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_TotalQty_Creators;        break;
      case 'q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_TotalQty_Creators_OddLot; break;
      default:  return OmsErrCode_CondSc_BadCondName;
      }
      if (opcomp == f9oms_CondOp_Compare_GEQ && this->CondQty_ == 0) // 因為 TotalQty 僅支援 > 及 >= 所以這裡僅需要考慮 >= 即可;
         return OmsErrCode_CondSc_BadQty;                            // TotalQty >= 0, 則一定會立即觸發, 所以無意義;
      break;
   case 'B':
      switch (this->CondName_.Chars_[1]) {
      case 'P': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_BuyPri_Creators;          break;
      case 'Q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_BuyQty_Creators;          break;
      case 'p': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_BuyPri_Creators_OddLot;   break;
      case 'q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_BuyQty_Creators_OddLot;   break;
      default:  return OmsErrCode_CondSc_BadCondName;
      }
      goto __CHECK_CondPri;
   case 'S':
      switch (this->CondName_.Chars_[1]) {
      case 'P': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_SellPri_Creators;         break;
      case 'Q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_SellQty_Creators;         break;
      case 'p': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_SellPri_Creators_OddLot;  break;
      case 'q': OmsCxBaseCondFn_Creators = OmsCxBaseCondFn_SellQty_Creators_OddLot;  break;
      default:  return OmsErrCode_CondSc_BadCondName;
      }
      goto __CHECK_CondPri;
   default:
      return OmsErrCode_CondSc_BadCondName;

   __CHECK_CondPri:;
      if (this->CondPri_.IsNull())
         return OmsErrCode_CondSc_BadPri;
      break;
   }
   auto fn = OmsCxBaseCondFn_Creators[opcomp];
   if (fn == nullptr)
      return OmsErrCode_CondSc_BadCondOp;
   this->CondFn_.reset(fn(*this));
   return OmsErrCode_Null;
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
