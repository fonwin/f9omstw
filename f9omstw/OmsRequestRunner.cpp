// \file f9omstw/OmsRequestRunner.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsRequestRunner.hpp"
#include "f9omstw/OmsCore.hpp"
#include <new>

namespace f9omstw {

OmsRequestRunnerInCore::~OmsRequestRunnerInCore() {
   assert(this->Resource_.Core_.IsThisThread());
   assert(this->OrderRaw_.Owner_->Last() == &this->OrderRaw_);
   this->OrderRaw_.Request_->LastUpdated_ = &this->OrderRaw_;
   // 把 this->OrderRaw_ 丟到 Reporter/Recorder 處理.
}

OmsRequestRunStep::~OmsRequestRunStep() {
}

} // namespaces
