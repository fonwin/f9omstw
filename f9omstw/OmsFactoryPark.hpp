// \file f9omstw/OmsFactoryPark.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsFactoryPark_hpp__
#define __f9omstw_OmsFactoryPark_hpp__
#include "fon9/seed/Tab.hpp"

namespace f9omstw {

template <class FactoryT>
class OmsFactoryPark : public fon9::seed::LayoutN {
   fon9_NON_COPY_NON_MOVE(OmsFactoryPark);
   using base = fon9::seed::LayoutN;

protected:
#ifndef NDEBUG
   bool IsFactory() const {
      for (size_t idx = this->GetTabCount(); idx > 0;) {
         if (dynamic_cast<FactoryT*>(this->GetTab(--idx)) == nullptr)
            return false;
      }
      return true;
   }
#endif

public:
   using base::base;

   FactoryT* GetFactory(size_t index) const {
      return static_cast<FactoryT*>(this->GetTab(index));
   }
   FactoryT* GetFactory(fon9::StrView name) const {
      return static_cast<FactoryT*>(this->GetTab(name));
   }
};
//--------------------------------------------------------------------------//
template <class FactoryT, fon9::seed::FieldSP (*FnKeyMaker)(), fon9::seed::TreeFlag treeFlags>
class OmsFactoryPark_WithKeyMaker : public OmsFactoryPark<FactoryT> {
   fon9_NON_COPY_NON_MOVE(OmsFactoryPark_WithKeyMaker);
   using base = OmsFactoryPark<FactoryT>;
public:
   template <class... ArgsT>
   OmsFactoryPark_WithKeyMaker(ArgsT&&... args)
      : base(FnKeyMaker(), treeFlags, std::forward<ArgsT>(args)...) {
      assert(this->IsFactory());
   }
};
template <class FactoryT, fon9::seed::TreeFlag treeFlags>
class OmsFactoryPark_NoKey : public OmsFactoryPark<FactoryT> {
   fon9_NON_COPY_NON_MOVE(OmsFactoryPark_NoKey);
   using base = OmsFactoryPark<FactoryT>;
public:
   template <class... ArgsT>
   OmsFactoryPark_NoKey(ArgsT&&... args)
      : base(fon9::seed::FieldSP{}, treeFlags, std::forward<ArgsT>(args)...) {
      assert(this->IsFactory());
   }
};

} // namespaces
#endif//__f9omstw_OmsFactoryPark_hpp__
