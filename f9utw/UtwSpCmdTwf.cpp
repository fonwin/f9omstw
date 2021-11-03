// \file f9utw/UtwSpCmdTwf.cpp
// \author fonwinz@gmail.com
#include "f9utw/UtwSpCmdTwf.hpp"
#include "f9omstwf/OmsTwfRequest0.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsCoreMgr.hpp"

namespace f9omstw {

enum SpCmdDordTwfFilter {
   SpCmdDordTwfFilter_No_TwfN = 0x01,
   SpCmdDordTwfFilter_No_TwfQ = 0x02,
};

struct SpCmdDordTwf : public SpCmdDord {
   fon9_NON_COPY_NON_MOVE(SpCmdDordTwf);
   using base = SpCmdDord;
   using base::base;
   OmsRequestFactory* ChgTwfN_Factory_{};
   OmsRequestFactory* ChgTwfQ_Factory_{};

   /// 例: 檢查 SymbId 是否符合預期.
   /// \retval false 表示此筆要求不用處理.
   bool CheckForDord_ArgsFilter(const OmsRequestIni& iniReq) override {
      auto* ini = dynamic_cast<const OmsTwfRequestIni0*>(&iniReq);
      if (ini == nullptr)
         return false;
      switch (ini->RequestType_) {
      default:
      case RequestType::QuoteR:
         return false;
      case RequestType::Normal:
         if (this->Args_.Filter_ & SpCmdDordTwfFilter_No_TwfN)
            return false;
         break;
      case RequestType::Quote:
         if (this->Args_.Filter_ & SpCmdDordTwfFilter_No_TwfQ)
            return false;
         break;
      }
      fon9::StrView symbid{ToStrView(this->Args_.SymbId_)};
      return(symbid.Get1st() == '*' || symbid == ToStrView(static_cast<const OmsTwfRequestIni0*>(&iniReq)->Symbol_));
   }
   fon9::intrusive_ptr<OmsRequestUpd> MakeDeleteRequest(const OmsRequestIni& iniReq) {
      OmsRequestFactory* fac;
      switch (static_cast<const OmsTwfRequestIni0*>(&iniReq)->RequestType_) {
      default:
      case RequestType::QuoteR: return nullptr;
      case RequestType::Normal: fac = this->ChgTwfN_Factory_;  break;
      case RequestType::Quote:  fac = this->ChgTwfQ_Factory_;  break;
      }
      if (!fac)
         return nullptr;
      return fon9::dynamic_pointer_cast<OmsRequestUpd>(fac->MakeRequest());
   }
};

//--------------------------------------------------------------------------//

bool SpCmdMgrTwf::ParseSpCmdArg(SpCmdArgs& args, fon9::StrView tag, fon9::StrView value) {
   SpCmdDordTwfFilter filter;
   if (tag == "TwfQ")      // value = "Y" or "N" 是否包含 [報價單 TwfNewQ]?
      filter = SpCmdDordTwfFilter_No_TwfQ;
   else if (tag == "TwfN") // value = "Y" or "N" 是否包含 [一般單 TwfNew]?
      filter = SpCmdDordTwfFilter_No_TwfN;
   else
      return false;
   switch (fon9::toupper(static_cast<unsigned char>(value.Get1st()))) {
   default:    return false;
   case 'Y':   args.Filter_ -= filter; break;
   case 'N':   args.Filter_ += filter; break;
   }
   return true;
}
SpCmdItemSP SpCmdMgrTwf::MakeSpCmdDord_Impl(fon9::StrView& strResult, SpCmdArgs&& args, fon9::Named&& itemName) {
   fon9::intrusive_ptr<SpCmdDordTwf> retval{new SpCmdDordTwf(*this, std::move(args), std::move(itemName))};
   if (!(args.Filter_ & SpCmdDordTwfFilter_No_TwfN)) {
      if ((retval->ChgTwfN_Factory_ = this->OmsCore_->Owner_->RequestFactoryPark().GetFactory("TwfChg")) == nullptr) {
         strResult = "dord: Not found TwfChg factory.";
         return nullptr;
      }
   }
   if (!(args.Filter_ & SpCmdDordTwfFilter_No_TwfQ)) {
      if ((retval->ChgTwfQ_Factory_ = this->OmsCore_->Owner_->RequestFactoryPark().GetFactory("TwfChgQ")) == nullptr) {
         strResult = "dord: Not found TwfChgQ factory.";
         return nullptr;
      }
   }
   if (retval->ChgTwfN_Factory_ == nullptr && retval->ChgTwfQ_Factory_ == nullptr) {
      strResult = "dord: Unnecessary.";
      return nullptr;
   }
   return retval;
}

} // namespaces
