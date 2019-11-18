// \file f9omstwf/OmsTwfExgSymbBasic.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfExgSymbBasic_hpp__
#define __f9omstwf_OmsTwfExgSymbBasic_hpp__
#include "fon9/CharVector.hpp"
#include "fon9/TimeStamp.hpp"

namespace f9omstw {

/// 這裡提供一個 Twf 商品基本資料.
/// TwfExgMapMgr 會將 P08 的內容填入此處.
class TwfExgSymbBasic {
public:
   /// 期交所PA8的商品Id;
   /// this->SymbId_ = P08的商品Id;
   /// P08/PA8 共用同一個 UtwsSymb;
   fon9::CharVector  LongId_;
   fon9::TimeStamp   P08UpdatedTime_;
};

} // namespaces
#endif//__f9omstwf_OmsTwfExgSymbBasic_hpp__
