// \file f9omstw/OmsOrderFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsOrderFactory_hpp__
#define __f9omstw_OmsOrderFactory_hpp__
#include "f9omstw/OmsBase.hpp"
#include "fon9/seed/Tab.hpp"

namespace f9omstw {

/// Name_   = 委託名稱, 例如: TwsOrd;
/// Fields_ = 委託書會隨著操作、成交而變動的欄位(例如: 剩餘量...),
///           委託的初始內容(例如: 帳號、商品...), 由 OmsRequest 負責提供;
class OmsOrderFactory : public fon9::seed::Tab {
   fon9_NON_COPY_NON_MOVE(OmsOrderFactory);
   using base = fon9::seed::Tab;

   friend class OmsOrder; // MakeOrderRawImpl() 開放給 OmsOrder::BeginUpdate(); 使用;
   virtual OmsOrderRaw* MakeOrderRawImpl() = 0;
   virtual OmsOrder* MakeOrderImpl() = 0;
public:
   using base::base;

   virtual ~OmsOrderFactory();

   /// 建立一個 OmsOrder, 並呼叫 OmsOrder.BeginUpdate() 開始異動.
   /// 若有提供 scRes, 則會將 std::move(*scRes) 用於 OmsOrder 的初始化.
   OmsOrderRaw* MakeOrder(OmsRequestBase& starter, OmsScResource* scRes);
};

} // namespaces
#endif//__f9omstw_OmsOrderFactory_hpp__
