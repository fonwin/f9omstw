// \file f9omstw/OmsResource.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsResource_hpp__
#define __f9omstw_OmsResource_hpp__
#include "f9omstw/OmsSymbTree.hpp"
#include "f9omstw/OmsBrkTree.hpp"
#include "f9omstw/OmsBackend.hpp"
#include "f9omstw/OmsRequestRunner.hpp"
#include "f9omstw/OmsFactoryPark.hpp"
#include "fon9/seed/MaTree.hpp"

namespace f9omstw {

using OmsRequestFactoryPark = OmsFactoryPark_WithKeyMaker<OmsRequestFactory, &OmsRxItem::MakeField_RxSNO, fon9::seed::TreeFlag::Unordered>;
using OmsRequestFactoryParkSP = fon9::intrusive_ptr<OmsRequestFactoryPark>;

using OmsOrderFactoryPark = OmsFactoryPark_NoKey<OmsOrderFactory, fon9::seed::TreeFlag::Unordered>;
using OmsOrderFactoryParkSP = fon9::intrusive_ptr<OmsOrderFactoryPark>;

/// OMS 所需的資源, 集中在此處理.
/// - 這裡的資源都 **不是** thread safe!
class OmsResource : public fon9::seed::NamedSapling {
   fon9_NON_COPY_NON_MOVE(OmsResource);

public:
   OmsCore&    Core_;

   using SymbTreeSP = fon9::intrusive_ptr<OmsSymbTree>;
   SymbTreeSP  Symbs_;

   using BrkTreeSP = fon9::intrusive_ptr<OmsBrkTree>;
   BrkTreeSP   Brks_;

   OmsOrderFactoryParkSP   OrderFacPark_;
   OmsRequestFactoryParkSP RequestFacPark_;
   /// Backend_ 死亡時需要用到 Factories 做最後的存檔,
   /// 所以必須放在 Factories 之後建構, 然後比 Factories 早死.
   OmsBackend              Backend_;

   void PutRequestId(OmsRequestBase& req) {
      this->ReqUID_Builder_.MakeReqUID(req, this->Backend_.FetchSNO(req));
   }
   const OmsRequestBase* GetRequest(OmsRxSNO sno) const {
      if (auto item = this->Backend_.GetItem(sno))
         return item->CastToRequest();
      return nullptr;
   }
   fon9::TimeStamp TDay() const {
      return this->TDay_;
   }

protected:
   fon9::TimeStamp   TDay_;
   OmsReqUID_Builder ReqUID_Builder_;

   template <class... NamedArgsT>
   OmsResource(OmsCore& core, NamedArgsT&&... namedargs)
      : fon9::seed::NamedSapling(new fon9::seed::MaTree("Tables"), std::forward<NamedArgsT>(namedargs)...)
      , Core_(core) {
   }
   ~OmsResource();

   void AddNamedSapling(fon9::seed::TreeSP sapling, fon9::Named&& named) {
      static_cast<fon9::seed::MaTree*>(this->Sapling_.get())->
         Add(new fon9::seed::NamedSapling(std::move(sapling), std::move(named)));
   }

   /// 將 this->Symbs_; this->Brks_; 加入 this->Sapling.
   void Plant();
};

//--------------------------------------------------------------------------//

inline OmsRequestRunnerInCore::~OmsRequestRunnerInCore() {
   this->Resource_.Backend_.OnAfterOrderUpdated(*this);
}

} // namespaces
#endif//__f9omstw_OmsResource_hpp__
