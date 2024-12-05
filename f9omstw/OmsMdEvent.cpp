// \file f9omstw/OmsMdEvent.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsMdEvent.hpp"
#include "fon9/RevPrint.hpp"
#include "fon9/BitvArchive.hpp"

namespace f9omstw {

void RevPrint(fon9::RevBuffer& rbuf, const OmsMdLastPriceEv& dat) {
   fon9::RevPrint(rbuf, "|LastPrice=", dat.LastPrice_, "|LastQty=", dat.LastQty_, "|TotalQty=", dat.TotalQty_);
}
void RevPrint(fon9::RevBuffer& rbuf, const OmsMdBSEv& dat) {
   for (;;) {
      if (fon9_LIKELY(dat.Buy_.Qty_)) {
         fon9::RevPrint(rbuf, '*', dat.Buy_.Qty_);
         if (fon9_UNLIKELY(dat.Buy_.Pri_ == kMdPriBuyMarket)) {
            fon9::RevPrint(rbuf, "|Buy=BM");
            break;
         }
         fon9::RevPrint(rbuf, dat.Buy_.Pri_);
      }
      fon9::RevPrint(rbuf, "|Buy=");
      break;
   }
   // -----
   for (;;) {
      if (fon9_LIKELY(dat.Sell_.Qty_)) {
         fon9::RevPrint(rbuf, '*', dat.Sell_.Qty_);
         if (fon9_UNLIKELY(dat.Sell_.Pri_ == kMdPriSellMarket)) {
            fon9::RevPrint(rbuf, "|Sell=SM");
            break;
         }
         fon9::RevPrint(rbuf, dat.Sell_.Pri_);
      }
      fon9::RevPrint(rbuf, "|Sell=");
      break;
   }
}
//--------------------------------------------------------------------------//
OmsMdLastPriceEv::~OmsMdLastPriceEv() {
}
OmsMdLastPriceSubject::~OmsMdLastPriceSubject() {
}
void OmsMdLastPriceSubject::OnMdLastPriceEv(const OmsMdLastPrice& bf, OmsCore& omsCore) {
   this->Subject_.Publish(*this, bf, omsCore);
}
//--------------------------------------------------------------------------//
OmsMdBSEv::~OmsMdBSEv() {
}
OmsMdBSSubject::~OmsMdBSSubject() {
}
void OmsMdBSSubject::OnMdBSEv(const OmsMdBS& bf, OmsCore& omsCore) {
   this->Subject_.Publish(*this, bf, omsCore);
}

} // namespaces

// ======================================================================== //

namespace fon9 { namespace seed {

FieldMdPQ::~FieldMdPQ() {
}
StrView FieldMdPQ::GetTypeId(NumOutBuf& buf) const {
   (void)buf;
   return StrView{"C0"};
}
void FieldMdPQ::CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const {
   const ValueT* value = this->GetValuePtr(rd);
   if (fon9_LIKELY(value->Qty_ != 0)) {
      RevPrint(out, " * ", value->Qty_);
      if (value->Pri_ == f9omstw::kMdPriBuyMarket)
         RevPrint(out, "BM");
      else if (value->Pri_ == f9omstw::kMdPriSellMarket)
         RevPrint(out, "SM");
      else
         FmtRevPrint(fmt, out, value->Pri_);
   }
}
OpResult FieldMdPQ::StrToCell(const RawWr& wr, StrView value) const {
   ValueT res = *this->GetValuePtr(wr);
   if (fon9_UNLIKELY(fon9::StrTrimHead(&value).empty())) {
      res.Pri_.AssignNull();
      res.Qty_ = 0;
   }
   else {
      res.Pri_ = StrTo(&value, res.Pri_.Null());
      if (res.Pri_.IsNull()) {
         switch (fon9::toupper(value.Get1st())) {
         case 'B':   res.Pri_ = f9omstw::kMdPriBuyMarket;   break;
         case 'S':   res.Pri_ = f9omstw::kMdPriSellMarket;  break;
         default:    return OpResult::value_format_error;
         }
         if (fon9::toupper(value.begin()[1]) != 'M')
            return OpResult::value_format_error;
         value.IncBegin(2);
      }
      switch (StrTrimHead(&value).Get1st()) {
      case '*':
      case ',':
         value.IncBegin(1);
         break;
      }
      res.Qty_ = StrTo(&value, res.Qty_);
   }
   this->PutValue(wr, res);
   return OpResult::no_error;
}
void FieldMdPQ::CellToBitv(const RawRd& rd, RevBuffer& out) const {
   const ValueT* ptr = this->GetValuePtr(rd);
   ToBitv(out, ptr->Pri_);
   ToBitv(out, ptr->Qty_);
}
OpResult FieldMdPQ::BitvToCell(const RawWr& wr, DcQueue& buf) const {
   ValueT val;
   ValueSetNull(val);
   BitvTo(buf, val.Pri_);
   BitvTo(buf, val.Qty_);
   this->PutValue(wr, val);
   return OpResult::no_error;
}
FieldNumberT FieldMdPQ::GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const {
   (void)rd; (void)outDecScale;
   return nullValue;
}
OpResult FieldMdPQ::PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const {
   (void)wr; (void)num; (void)decScale;
   return OpResult::not_supported_number;
}

OpResult FieldMdPQ::SetNull(const RawWr& wr) const {
   ValueT val;
   ValueSetNull(val);
   this->PutValue(wr, val);
   return OpResult::no_error;
}
bool FieldMdPQ::IsNull(const RawRd& rd) const {
   return this->GetValuePtr(rd)->Qty_ == 0;
}

OpResult FieldMdPQ::Copy(const RawWr& wr, const RawRd& rd) const {
   this->PutValue(wr, *this->GetValuePtr(rd));
   return OpResult::no_error;
}
int FieldMdPQ::Compare(const RawRd& lhs, const RawRd& rhs) const {
   return this->CompareValue(lhs, *this->GetValuePtr(rhs));
}
size_t FieldMdPQ::AppendRawBytes(const RawRd& rd, ByteVector& dst) const {
   dst.append(this->GetValuePtr(rd), sizeof(ValueT));
   return sizeof(ValueT);
}
int FieldMdPQ::CompareRawBytes(const RawRd& rd, const void* rhs, size_t rsz) const {
   if (rsz == sizeof(ValueT))
      return this->CompareValue(rd, *static_cast<const ValueT*>(rhs));
   ValueT rval;
   ValueSetNull(rval);
   return this->CompareValue(rd, rval);
}

} } // namespaces fon::seed
