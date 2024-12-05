// \file f9omstw/OmsMdEvent.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsMdEvent_hpp__
#define __f9omstw_OmsMdEvent_hpp__
#include "f9omstw/OmsBase.hpp"
#include "fon9/Subr.hpp"
#include "fon9/seed/RawWr.hpp"

namespace f9omstw {

using MdQty = fon9::fmkt::Qty;
using MdPri = fon9::fmkt::Pri;
constexpr MdPri   kMdPriBuyMarket = MdPri::max();
constexpr MdPri   kMdPriSellMarket = MdPri::min();

static inline MdPri CondPriToBuy(MdPri pri) {
   return pri.IsNull() ? kMdPriBuyMarket : pri;
}
static inline MdPri CondPriToSell(MdPri pri) {
   return pri.IsNull() ? kMdPriSellMarket : pri;
}
//--------------------------------------------------------------------------//
class OmsMdLastPriceEv;
class OmsMdBSEv;
/// fon9::RevPrint(rbuf, "|LastPrice=", dat.LastPrice_, "|LastQty=", dat.LastQty_, "|TotalQty=", dat.TotalQty_);
void RevPrint(fon9::RevBuffer& rbuf, const OmsMdLastPriceEv& dat);
void RevPrint(fon9::RevBuffer& rbuf, const OmsMdBSEv& dat);
//--------------------------------------------------------------------------//
struct OmsMdLastPrice {
   MdPri LastPrice_;
   bool operator==(const OmsMdLastPrice& rhs) const {
      return this->LastPrice_ == rhs.LastPrice_;
   }
   bool operator!=(const OmsMdLastPrice& rhs) const {
      return !this->operator==(rhs);
   }
};
class OmsMdLastPriceEv : public OmsMdLastPrice {
   fon9_NON_COPY_NON_MOVE(OmsMdLastPriceEv);
protected:
   bool IsNeedsOnMdLastPriceEv_{false};
   char Padding_____[7];
   void SetIsNeedsOnMdLastPriceEv(bool value) {
      this->IsNeedsOnMdLastPriceEv_ = value;
   }
public:
   fon9::fmkt::Qty   LastQty_{};
   fon9::fmkt::Qty   TotalQty_{};

   OmsMdLastPriceEv() = default;
   virtual ~OmsMdLastPriceEv();
   virtual void OnMdLastPriceEv(const OmsMdLastPrice& bf, OmsCore&) = 0;
   /// 是否需要觸發 this->OnMdLastPriceEv()?
   bool IsNeedsOnMdLastPriceEv() const {
      return this->IsNeedsOnMdLastPriceEv_;
   }
};

class OmsMdLastPriceSubject : public OmsMdLastPriceEv {
   fon9_NON_COPY_NON_MOVE(OmsMdLastPriceSubject);
public:
   using Handler = std::function<void(const OmsMdLastPriceSubject& sender, const OmsMdLastPrice& bf, OmsCore&)>;
   using Subject = fon9::Subject<Handler>;

   OmsMdLastPriceSubject() = default;
   ~OmsMdLastPriceSubject();
   void OnMdLastPriceEv(const OmsMdLastPrice& bf, OmsCore&) override;

   void MdLastPrice_Subscribe(fon9::SubConn* subConn, Handler&& handler) {
      this->Subject_.Subscribe(subConn, std::move(handler));
      this->IsNeedsOnMdLastPriceEv_ = true;
   }
   void MdLastPrice_Unsubscribe(fon9::SubConn* subConn) {
      if (this->Subject_.Unsubscribe(subConn)) {
         auto subj = this->Subject_.Lock();
         this->IsNeedsOnMdLastPriceEv_ = !subj->empty();
      }
   }

protected:
   Subject  Subject_;
};
//--------------------------------------------------------------------------//
#define f9omstw_kMdBSCount    5
fon9_MSC_WARN_DISABLE(4582); // constructor is not implicitly called
struct OmsMdBS {
   union {
      fon9::fmkt::PriQty   Buy_;
      fon9::fmkt::PriQty   Buys_[f9omstw_kMdBSCount];
   };
   union {
      fon9::fmkt::PriQty   Sell_;
      fon9::fmkt::PriQty   Sells_[f9omstw_kMdBSCount];
   };
   OmsMdBS() {
      fon9::ForceZeroNonTrivial(this);
   }
};
fon9_MSC_WARN_POP;

class OmsMdBSEv : public OmsMdBS {
   fon9_NON_COPY_NON_MOVE(OmsMdBSEv);
protected:
   bool IsNeedsOnMdBSEv_{false};
   char Padding_____[7];
   void SetIsNeedsOnMdBSEv(bool value) {
      this->IsNeedsOnMdBSEv_ = value;
   }
public:
   OmsMdBSEv() = default;
   virtual ~OmsMdBSEv();
   virtual void OnMdBSEv(const OmsMdBS& bf, OmsCore&) = 0;
   /// 是否需要觸發 this->OnMdBSEv()?
   bool IsNeedsOnMdBSEv() const {
      return this->IsNeedsOnMdBSEv_;
   }
};
class OmsMdBSSubject : public OmsMdBSEv {
   fon9_NON_COPY_NON_MOVE(OmsMdBSSubject);
public:
   using Handler = std::function<void(const OmsMdBSSubject& sender, const OmsMdBS& bf, OmsCore&)>;
   using Subject = fon9::Subject<Handler>;

   OmsMdBSSubject() = default;
   ~OmsMdBSSubject();
   void OnMdBSEv(const OmsMdBS& bf, OmsCore&) override;

   void MdBS_Subscribe(fon9::SubConn* subConn, Handler&& handler) {
      this->Subject_.Subscribe(subConn, std::move(handler));
      this->IsNeedsOnMdBSEv_ = true;
   }
   void MdBS_Unsubscribe(fon9::SubConn* subConn) {
      if (this->Subject_.Unsubscribe(subConn)) {
         auto subj = this->Subject_.Lock();
         this->IsNeedsOnMdBSEv_ = !subj->empty();
      }
   }

protected:
   Subject  Subject_;
};

} // namespaces

// ======================================================================== //

namespace fon9 { namespace seed {

class FieldMdPQ : public fon9::seed::Field {
   fon9_NON_COPY_NON_MOVE(FieldMdPQ);
   FieldMdPQ() = delete;
   using base = fon9::seed::Field;
public:
   using ValueT = fon9::fmkt::PriQty;

   FieldMdPQ(Named&& named, int32_t ofs)
      : base{std::move(named), f9sv_FieldType_Unknown, FieldSource::DataMember, ofs, sizeof(ValueT), 0} {
   }
   ~FieldMdPQ();

   static void ValueSetNull(ValueT& val) {
      val.Pri_.AssignNull();
      val.Qty_ = 0;
   }

   StrView GetTypeId(NumOutBuf& buf) const override;

   const ValueT* GetValuePtr(const RawRd& rd) const {
      return rd.GetCellPtr<ValueT>(*this);
   }
   void PutValue(const RawWr& wr, const ValueT& value) const {
      wr.PutCellValue(*this, value);
   }

   void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override;
   OpResult StrToCell(const RawWr& wr, StrView value) const override;

   void CellToBitv(const RawRd& rd, RevBuffer& out) const override;
   OpResult BitvToCell(const RawWr& wr, DcQueue& buf) const override;

   FieldNumberT GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const override;
   OpResult PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const override;

   OpResult SetNull(const RawWr& wr) const override;
   bool IsNull(const RawRd& rd) const override;

   OpResult Copy(const RawWr& wr, const RawRd& rd) const override;

   int CompareValue(const RawRd& lhs, const ValueT& rhs) const {
      const ValueT* plhs = this->GetValuePtr(lhs);
      int retval = plhs->Pri_.Compare(rhs.Pri_);
      if (retval != 0)
         return retval;
      return ( plhs->Qty_ == rhs.Qty_ ? 0
             : plhs->Qty_ <  rhs.Qty_ ? -1
             :                          1);
   }
   int Compare(const RawRd& lhs, const RawRd& rhs) const override;
   size_t AppendRawBytes(const RawRd& rd, ByteVector& dst) const override;
   int CompareRawBytes(const RawRd& rd, const void* rhs, size_t rsz) const override;
};

inline fon9::seed::FieldSPT<FieldMdPQ> MakeField(fon9::Named&& named, int32_t ofs, fon9::fmkt::PriQty&) {
   return fon9::seed::FieldSPT<FieldMdPQ>{new FieldMdPQ{std::move(named), ofs}};
}
inline fon9::seed::FieldSPT<FieldMdPQ> MakeField(fon9::Named&& named, int32_t ofs, const fon9::fmkt::PriQty&) {
   return fon9::seed::FieldSPT<FieldMdPQ>{new FieldConst<FieldMdPQ>{std::move(named), ofs}};
}

} } // namespaces
#endif//__f9omstw_OmsMdEvent_hpp__
