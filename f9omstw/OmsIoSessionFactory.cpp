// \file f9omstw/OmsIoSessionFactory.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsIoSessionFactory.hpp"

namespace f9omstw {
using namespace fon9;
using namespace fon9::seed;
using namespace fon9::io;

OmsIoSessionFactory::OmsIoSessionFactory(std::string name, OmsCoreMgrSP&& omsCoreMgr)
   : base(std::move(name))
   , OmsCoreMgr_{std::move(omsCoreMgr)} {
}
OmsIoSessionFactory::~OmsIoSessionFactory() {
   this->OmsCoreMgr_->TDayChangedEvent_.Unsubscribe(&this->SubConnTDayChanged_);
}
void OmsIoSessionFactory::OnAfterCtor() {
   assert(this->SubConnTDayChanged_ == nullptr);
   this->OmsCoreMgr_->TDayChangedEvent_.Subscribe(&this->SubConnTDayChanged_,
      std::bind(&OmsIoSessionFactory::OnTDayChanged, this, std::placeholders::_1));
}
void OmsIoSessionFactory::OnTDayChanged(OmsCore& omsCore) {
   (void)omsCore;
}
//--------------------------------------------------------------------------//
OmsIoSessionFactoryConfigParser::~OmsIoSessionFactoryConfigParser() {
}
bool OmsIoSessionFactoryConfigParser::OnUnknownTag(PluginsHolder& holder, StrView tag, StrView value) {
   if (tag == "OmsCore") {
      this->OmsCoreName_ = value;
      return true;
   }
   return base::OnUnknownTag(holder, tag, value);
}
OmsCoreMgrSP OmsIoSessionFactoryConfigParser::GetOmsCoreMgr() {
   if (auto omsCoreMgr = this->PluginsHolder_.Root_->GetSapling<OmsCoreMgr>(this->OmsCoreName_))
      return omsCoreMgr;
   this->ErrMsg_ += "|err=Unknown OmsCore";
   if (!this->OmsCoreName_.empty()) {
      this->ErrMsg_.push_back('=');
      this->OmsCoreName_.AppendTo(this->ErrMsg_);
   }
   return nullptr;
}

} // namespaces
