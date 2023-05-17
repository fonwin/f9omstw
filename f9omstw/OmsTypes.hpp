// \file f9omstw/OmsTypes.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsTypes_hpp__
#define __f9omstw_OmsTypes_hpp__

namespace f9omstw {

/// 用於風控檢查時, 用 idx 取得 `Qty BS_[idx];` [買or賣] 的資料.
enum OmsBSIdx : unsigned char /* uint8_t */ {
   OmsBSIdx_Buy = 0,
   OmsBSIdx_Sell = 1,
};
static inline OmsBSIdx RevOmsBSIdx(OmsBSIdx val) {
   return static_cast<OmsBSIdx>(!val);
}

enum OmsOTypeIdx : unsigned char /* uint8_t */ {
   OmsOTypeIdx_Gn = 0,
   OmsOTypeIdx_Cr = 1,
   OmsOTypeIdx_Db = 2,
   OmsOTypeIdx_SBL = 3,
};

} // namespaces
#endif//__f9omstw_OmsTypes_hpp__
