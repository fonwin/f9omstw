// \file f9utw/UtwSpCmdTws.cpp
// \author fonwinz@gmail.com
#include "f9utw/UtwSpCmdTws.hpp"
#include "f9omstws/OmsTwsRequest.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsCoreMgr.hpp"

namespace f9omstw {

struct SpCmdDordTws : public SpCmdDord {
   fon9_NON_COPY_NON_MOVE(SpCmdDordTws);
   using base = SpCmdDord;
   using base::base;
   OmsRequestFactory* ChgTwsN_Factory_{};

   /// 例: 檢查 SymbId 是否符合預期.
   /// \retval false 表示此筆要求不用處理.
   bool CheckForDord_ArgsFilter(const OmsRequestIni& iniReq) override {
      auto* ini = dynamic_cast<const OmsTwsRequestIni*>(&iniReq);
      if (ini == nullptr)
         return false;
      fon9::StrView symbid{ToStrView(this->Args_.SymbId_)};
      return(symbid.Get1st() == '*' || symbid == ToStrView(static_cast<const OmsTwsRequestIni*>(&iniReq)->Symbol_));
   }
   fon9::intrusive_ptr<OmsRequestUpd> MakeDeleteRequest(const OmsRequestIni& iniReq) {
      (void)iniReq;
      return fon9::dynamic_pointer_cast<OmsRequestUpd>(this->ChgTwsN_Factory_->MakeRequest());
   }
};

//--------------------------------------------------------------------------//

bool SpCmdMgrTws::ParseSpCmdArg(SpCmdArgs& args, fon9::StrView tag, fon9::StrView value) {
   (void)args; (void)tag; (void)value;
   return false;
}
SpCmdItemSP SpCmdMgrTws::MakeSpCmdDord_Impl(fon9::StrView& strResult, SpCmdArgs&& args, fon9::Named&& itemName) {
   fon9::intrusive_ptr<SpCmdDordTws> retval{new SpCmdDordTws(*this, std::move(args), std::move(itemName))};
   if ((retval->ChgTwsN_Factory_ = this->OmsCore_->Owner_->RequestFactoryPark().GetFactory("TwsChg")) == nullptr) {
      strResult = "dord: Not found TwsChg factory.";
      return nullptr;
   }
   return retval;
}

} // namespaces
