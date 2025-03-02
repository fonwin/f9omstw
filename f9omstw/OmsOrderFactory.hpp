// \file f9omstw/OmsOrderFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsOrderFactory_hpp__
#define __f9omstw_OmsOrderFactory_hpp__
#include "f9omstw/OmsTools.hpp"
#include "fon9/ObjSupplier.hpp"

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
   OmsRequestFactory* const DeleteRequestFactory_{nullptr};

   template <class... ArgsT>
   OmsOrderFactory(ArgsT&&... args) : base(std::forward<ArgsT>(args)...) {
   }
   template <class... ArgsT>
   OmsOrderFactory(OmsRequestFactory* deleteRequestFactory, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , DeleteRequestFactory_{deleteRequestFactory} {
   }

   virtual ~OmsOrderFactory();

   /// 建立一個 OmsOrder, 並呼叫 OmsOrder.BeginUpdate() 開始異動.
   /// 若有提供 scRes, 則會將 std::move(*scRes) 用於 OmsOrder 的初始化.
   OmsOrderRaw* MakeOrder(OmsRequestBase& starter, OmsScResource* scRes);
};

template <class OrderT, class OrderRawT>
class OmsOrderFactoryBaseT : public OmsOrderFactory {
   fon9_NON_COPY_NON_MOVE(OmsOrderFactoryBaseT);
   using base = OmsOrderFactory;

   OrderRawT* MakeOrderRawImpl() override {
      return new OrderRawT;
   }

   OrderT* MakeOrderImpl() override {
      return new OrderT;
   }

public:
   OmsOrderFactoryBaseT(std::string name, OmsRequestFactory* deleteRequestFactory = nullptr)
      : base(deleteRequestFactory, fon9::Named(std::move(name)), f9omstw::MakeFieldsT<OrderRawT>()) {
   }
};

template <class OrderT, class OrderRawT, unsigned kPoolObjCount>
class OmsOrderFactoryT : public OmsOrderFactoryBaseT<OrderT, OrderRawT> {
   fon9_NON_COPY_NON_MOVE(OmsOrderFactoryT);
   using base = OmsOrderFactoryBaseT<OrderT, OrderRawT>;

   using RawSupplier = fon9::ObjSupplier<OrderRawT, kPoolObjCount>;
   typename RawSupplier::ThisSP RawSupplier_{RawSupplier::Make()};
   OrderRawT* MakeOrderRawImpl() override {
      return this->RawSupplier_->Alloc();
   }

   using OrderSupplier = fon9::ObjSupplier<OrderT, kPoolObjCount>;
   typename OrderSupplier::ThisSP OrderSupplier_{OrderSupplier::Make()};
   OrderT* MakeOrderImpl() override {
      return this->OrderSupplier_->Alloc();
   }

public:
   using base::base;
};

template <class OrderT, class OrderRawT>
class OmsOrderFactoryT<OrderT, OrderRawT, 0> : public OmsOrderFactoryBaseT<OrderT, OrderRawT> {
   fon9_NON_COPY_NON_MOVE(OmsOrderFactoryT);
   using base = OmsOrderFactoryBaseT<OrderT, OrderRawT>;

public:
   using base::base;
};

} // namespaces
#endif//__f9omstw_OmsOrderFactory_hpp__
