// \file f9omstw/OmsTreeSvctSet.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsTreeSvctSet_hpp__
#define __f9omstw_OmsTreeSvctSet_hpp__
#include "f9omstw/OmsTree.hpp"
#include "f9omstw/OmsTools.hpp"

namespace f9omstw {

/// 提供給 fon9::SortedVectorSet<ValueSP, ValueSP_Comper> 使用的 tree.
/// - 例如: OmsIvSymbs; OmsSubacMap;
template <class SvctSet, class OwnerSP>
class OmsTreeSvctSet : public OmsTree {
   fon9_NON_COPY_NON_MOVE(OmsTreeSvctSet);
   using base = OmsTree;

public:
   using value_type = typename SvctSet::value_type;
   using OwnerT = typename OwnerSP::element_type;
   typedef value_type (*FnValueMaker)(const fon9::StrView& idstr, OwnerT* owner);
   const FnValueMaker   FnValueMaker_;
   const OwnerSP        ContainerOwner_;
   SvctSet&             Container_;

   OmsTreeSvctSet(OmsCore& omsCore, fon9::seed::LayoutSP layout, OwnerSP owner, SvctSet& container, FnValueMaker fnValueMaker)
      : base(omsCore, std::move(layout))
      , FnValueMaker_{fnValueMaker}
      , ContainerOwner_{std::move(owner)}
      , Container_(container) {
   }

protected:
   struct TreeOp : public fon9::seed::TreeOp {
      fon9_NON_COPY_NON_MOVE(TreeOp);
      using base = fon9::seed::TreeOp;
      using base::base;

      void GridView(const fon9::seed::GridViewRequest& req, fon9::seed::FnGridViewOp fnCallback) override {
         assert(OmsTreeSvctSet::IsInOmsThread(&this->Tree_));
         fon9::seed::GridViewResult res{this->Tree_, req.Tab_};
         auto&                      container = static_cast<OmsTreeSvctSet*>(&this->Tree_)->Container_;
         const auto                 ikey = fon9::seed::GetIteratorForGv(container, req.OrigKey_);
         const size_t               istart = static_cast<size_t>(ikey - container.begin());
         res.ContainerSize_ = container.size();
         fon9::seed::MakeGridViewArrayRange(istart, res.ContainerSize_, req, res,
                                            [&res, &container](size_t ivalue, fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) {
            if (ivalue >= res.ContainerSize_)
               return false;
            container.sindex(ivalue)->MakeGridRow(tab, rbuf);
            return true;
         });
         fnCallback(res);
      }

      void OnPodOp(const fon9::StrView& strKeyText, fon9::seed::FnPodOp&& fnCallback, value_type val) {
         assert(OmsTreeSvctSet::IsInOmsThread(&this->Tree_));
         if (val)
            val->OnPodOp(*static_cast<OmsTreeSvctSet*>(&this->Tree_), std::move(fnCallback), strKeyText);
         else
            fnCallback(fon9::seed::PodOpResult{this->Tree_, fon9::seed::OpResult::not_found_key, strKeyText}, nullptr);
      }
      void Get(fon9::StrView strKeyText, fon9::seed::FnPodOp fnCallback) override {
         const auto  ifind = static_cast<OmsTreeSvctSet*>(&this->Tree_)->Container_.find(strKeyText);
         this->OnPodOp(strKeyText, std::move(fnCallback),
                       ifind == static_cast<OmsTreeSvctSet*>(&this->Tree_)->Container_.end() ? nullptr : ifind->get());
      }
      void Add(fon9::StrView strKeyText, fon9::seed::FnPodOp fnCallback) override {
         auto  owner = static_cast<OmsTreeSvctSet*>(&this->Tree_);
         if (owner->FnValueMaker_ == nullptr)
            base::Add(strKeyText, std::move(fnCallback));
         else {
            auto  ifind = owner->Container_.find(strKeyText);
            if (ifind == owner->Container_.end())
               ifind = owner->Container_.insert(owner->FnValueMaker_(strKeyText, owner->ContainerOwner_.get())).first;
            this->OnPodOp(strKeyText, std::move(fnCallback), ifind->get());
         }
      }
      void Remove(fon9::StrView strKeyText, fon9::seed::Tab* tab, fon9::seed::FnPodRemoved fnCallback) override {
         assert(OmsTreeSvctSet::IsInOmsThread(&this->Tree_));
         fon9::seed::PodRemoveResult res{this->Tree_, fon9::seed::OpResult::removed_pod, strKeyText, tab};
         if (!ContainerRemove(static_cast<OmsTreeSvctSet*>(&this->Tree_)->Container_, strKeyText))
            res.OpResult_ = fon9::seed::OpResult::not_found_key;
         fnCallback(res);
      }
   };

   void InThr_OnTreeOp(fon9::seed::FnTreeOp fnCallback) {
      TreeOp op{*this};
      fnCallback(fon9::seed::TreeOpResult{this, fon9::seed::OpResult::no_error}, &op);
   }
};

} // namespaces
#endif//__f9omstw_OmsTreeSvctSet_hpp__
