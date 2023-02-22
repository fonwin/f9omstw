// \file f9omstw/OmsErrCodeAct.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsErrCodeAct.hpp"
#include "f9omstw/OmsOrder.hpp"

namespace f9omstw {

#define kCSTR_During          "During"
#define kCSTR_NewSrc          "NewSrc"
#define kCSTR_Src             "Src"
#define kCSTR_OType           "OType"
#define kCSTR_UseNewLine      "UseNewLine"
#define kCSTR_AtNewDone       "AtNewDone"
#define kCSTR_NewSending      "NewSending"
#define kCSTR_St              "St"
#define kCSTR_Rerun           "Rerun"
#define kCSTR_ErrCode         "ErrCode"
#define kCSTR_ResetOkErrCode  "ResetOkErrCode"
#define kCSTR_Memo            "Memo"

static void AssignSrc(fon9::CharVector& dst, fon9::StrView src) {
   dst.clear();
   while (!src.empty()) {
      fon9::StrView val = fon9::StrFetchTrim(src, ',');
      if (val.empty())
         continue;
      if (!dst.empty())
         dst.push_back(',');
      dst.append(val);
   }
}

fon9_WARN_DISABLE_PADDING;
struct RequestStDef {
   f9fmkt_TradingRequestSt St_;
   fon9::StrView           StName_;
};
fon9_WARN_POP;
#define DEF_RequestStDef(st) { f9fmkt_TradingRequestSt_##st, fon9::StrView{#st} }
static const RequestStDef StrRequestSt[]{
   DEF_RequestStDef(ExchangeNoLeavesQty),
   DEF_RequestStDef(ExchangeCanceling),
   DEF_RequestStDef(ExchangeCanceled),
};
f9fmkt_TradingRequestSt StrToRequestSt(fon9::StrView value) {
   for (const auto& st : StrRequestSt) {
      if (st.StName_ == value)
         return st.St_;
   }
   return StrTo(value, f9fmkt_TradingRequestSt{});
}
//--------------------------------------------------------------------------//
const char* OmsErrCodeAct::ParseFrom(fon9::StrView cfgstr, std::string* msg) {
   fon9::StrView tag, value;
   while (fon9::StrFetchTagValue(cfgstr, tag, value)) {
      if (tag == kCSTR_During) {
         this->BeginTime_ = StrTo(&value, fon9::DayTime::Null());
         if (fon9::StrTrim(&value).Get1st() != '-')
            this->EndTime_.AssignNull();
         else {
            fon9::StrTrimHead(&value, value.begin() + 1);
            this->EndTime_ = StrTo(&value, fon9::DayTime::Null());
         }
      }
      else if (tag == kCSTR_NewSrc) {
         AssignSrc(this->NewSrcs_, value);
      }
      else if (tag == kCSTR_Src) {
         AssignSrc(this->Srcs_, value);
      }
      else if (tag == kCSTR_OType) {
         this->OTypes_.clear();
         for (char ch : value) {
            if (ch != ',' && !fon9::isspace(static_cast<unsigned char>(ch)))
               this->OTypes_.push_back(ch);
         }
      }
      else if (tag == kCSTR_NewSending) {
         this->NewSending_ = fon9::StrTo(value, this->NewSending_);
      }
      else if (tag == kCSTR_AtNewDone) {
         this->IsAtNewDone_ = (fon9::toupper(value.Get1st()) == 'Y');
      }
      else if (tag == kCSTR_UseNewLine) {
         this->IsUseNewLine_ = (fon9::toupper(value.Get1st()) == 'Y');
      }
      else if (tag == kCSTR_St) {
         if ((this->ReqSt_ = StrToRequestSt(value)) == f9fmkt_TradingRequestSt{}) {
            if (msg)
               *msg = value.ToString("Unknown St: ");
            return value.begin();
         }
      }
      else if (tag == kCSTR_Rerun) {
         this->RerunTimes_ = fon9::StrTo(value, this->RerunTimes_);
      }
      else if (tag == kCSTR_ErrCode) {
         this->ReErrCode_ = static_cast<OmsErrCode>(fon9::StrTo(value, fon9::cast_to_underlying(this->ReErrCode_)));
         this->IsResetOkErrCode_ = false;
      }
      else if (tag == kCSTR_ResetOkErrCode) {
         this->ReErrCode_ = OmsErrCode_MaxV;
         this->IsResetOkErrCode_ = (fon9::toupper(value.Get1st()) == 'Y');
      }
      else if (tag == kCSTR_Memo) {
         this->Memo_ = value.ToString();
      }
      else {
         if (msg)
            *msg = tag.ToString("Unknown tag: ");
         return tag.begin();
      }
   }
   this->ConfigStr_ = fon9::RevPrintTo<std::string>(*this);
   return nullptr;
}

void RevPrint(fon9::RevBuffer& rbuf, const OmsErrCodeAct& act) {
   struct SplAux {
      bool  NeedsSpl_{false};
      void operator()(fon9::RevBuffer& rbuf) {
         if (this->NeedsSpl_)
            fon9::RevPrint(rbuf, '|');
         else
            this->NeedsSpl_ = true;
      }
   };
   SplAux   splaux;
   if (!act.BeginTime_.IsNull() || !act.EndTime_.IsNull()) {
      splaux(rbuf);
      fon9::RevPrint(rbuf, kCSTR_During "=", act.BeginTime_, '-', act.EndTime_);
   }
   if (!act.NewSrcs_.empty()) {
      splaux(rbuf);
      fon9::RevPrint(rbuf, kCSTR_NewSrc "=", act.NewSrcs_);
   }
   if (!act.Srcs_.empty()) {
      splaux(rbuf);
      fon9::RevPrint(rbuf, kCSTR_Src "=", act.Srcs_);
   }
   if (!act.OTypes_.empty()) {
      splaux(rbuf);
      fon9::RevPrint(rbuf, kCSTR_OType "=", act.OTypes_);
   }
   if (act.NewSending_.GetOrigValue() > 0) {
      splaux(rbuf);
      fon9::RevPrint(rbuf, kCSTR_NewSending "=", act.NewSending_);
   }
   if (act.IsAtNewDone_) {
      splaux(rbuf);
      fon9::RevPrint(rbuf, kCSTR_AtNewDone "=Y");
   }
   if (act.IsUseNewLine_) {
      splaux(rbuf);
      fon9::RevPrint(rbuf, kCSTR_UseNewLine "=Y");
   }
   if (act.ReqSt_) {
      splaux(rbuf);
      for (const auto& st : StrRequestSt) {
         if (act.ReqSt_ == st.St_) {
            fon9::RevPrint(rbuf, ':', st.StName_);
            break;
         }
      }
      fon9::RevPrint(rbuf, kCSTR_St "=", act.ReqSt_);
   }
   if (act.RerunTimes_) {
      splaux(rbuf);
      fon9::RevPrint(rbuf, kCSTR_Rerun "=", act.RerunTimes_);
   }
   if (act.ReErrCode_ != OmsErrCode_MaxV) {
      splaux(rbuf);
      fon9::RevPrint(rbuf, kCSTR_ErrCode "=", act.ReErrCode_);
   }
   if (act.IsResetOkErrCode_) {
      splaux(rbuf);
      fon9::RevPrint(rbuf, kCSTR_ResetOkErrCode "=Y");
   }
}
bool OmsErrCodeAct::CheckTime(fon9::DayTime now) const {
   if (this->BeginTime_.IsNull()) {
      if (this->EndTime_.IsNull())
         return true;
      return now <= this->EndTime_;
   }
   if (this->EndTime_.IsNull())
      return this->BeginTime_ <= now;
   if (this->BeginTime_ < this->EndTime_)
      return this->BeginTime_ <= now && now <= this->EndTime_;
   return this->BeginTime_ <= now || now <= this->EndTime_;
}

} // namespaces
