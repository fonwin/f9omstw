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

   virtual OmsOrderRaw* MakeOrderRawImpl() = 0;
   virtual OmsOrder* MakeOrderImpl() = 0;
public:
   using base::base;

   virtual ~OmsOrderFactory();

   /// 新單要求, 建立一個新的委託與其對應, 然後返回 OmsOrderRaw 開始首次更新.
   /// 若有提供 scRes, 則會將 std::move(*scRes) 用於 Order 的初始化.
   OmsOrderRaw* MakeOrder(OmsRequestIni& initiator, OmsScResource* scRes);

   /// 建立時須注意, 若此時 order.Tail()==nullptr
   /// - 表示要建立的是 order 第一次異動.
   /// - 包含 order.Head_ 及之後的 data members、衍生類別, 都處在尚未初始化的狀態.
   OmsOrderRaw* MakeOrderRaw(OmsOrder& order, const OmsRequestBase& req);
};

} // namespaces
#endif//__f9omstw_OmsOrderFactory_hpp__
