// \file f9omstw/OmsIvacTree.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsIvacTree.hpp"
#include "f9omstw/OmsBrk.hpp"

namespace f9omstw {

inline IvacNo GetIteratorForPod(const OmsIvacMap&, const fon9::StrView& strKeyText) {
   return StrToIvacNo(strKeyText);
}

struct OmsIvacTree::TreeOp : public fon9::seed::TreeOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
   using base = fon9::seed::TreeOp;
   using base::base;

   void GridView(const fon9::seed::GridViewRequest& req, fon9::seed::FnGridViewOp fnCallback) override {
      assert(OmsIvacTree::IsInOmsThread(&this->Tree_));
      fon9::seed::GridViewResult res{this->Tree_, req.Tab_};
      fon9::seed::MakeGridViewUnordered(*static_cast<const OmsIvacMap*>(nullptr), static_cast<IvacNo>(IvacNC::End) * 10, req, res,
                                        [this](IvacNo ivacNo, fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) {
         if (auto* ivac = static_cast<OmsIvacTree*>(&this->Tree_)->Brk_->GetIvac(ivacNo)) {
            ivac->MakeGridRow(tab, rbuf);
            return true;
         }
         return false;
      });
      fnCallback(res);
   }

   void OnPodOp(const fon9::StrView& strKeyText, fon9::seed::FnPodOp&& fnCallback, OmsIvac* ivac) {
      assert(OmsIvacTree::IsInOmsThread(&this->Tree_));
      if (ivac)
         ivac->OnPodOp(*static_cast<OmsIvacTree*>(&this->Tree_), std::move(fnCallback), strKeyText);
      else
         fnCallback(fon9::seed::PodOpResult{this->Tree_, fon9::seed::OpResult::not_found_key, strKeyText}, nullptr);
   }
   void Get(fon9::StrView strKeyText, fon9::seed::FnPodOp fnCallback) override {
      this->OnPodOp(strKeyText, std::move(fnCallback),
                    static_cast<OmsIvacTree*>(&this->Tree_)->Brk_->GetIvac(StrToIvacNo(strKeyText)));
   }
   void Add(fon9::StrView strKeyText, fon9::seed::FnPodOp fnCallback) override {
      this->OnPodOp(strKeyText, std::move(fnCallback),
                    static_cast<OmsIvacTree*>(&this->Tree_)->Brk_->FetchIvac(StrToIvacNo(strKeyText)));
   }
   void Remove(fon9::StrView strKeyText, fon9::seed::Tab* tab, fon9::seed::FnPodRemoved fnCallback) override {
      fon9::seed::PodRemoveResult res{this->Tree_, fon9::seed::OpResult::removed_pod, strKeyText, tab};
      static_cast<OmsIvacTree*>(&this->Tree_)->Brk_->RemoveIvac(StrToIvacNo(strKeyText));
      fnCallback(res);
   }
};
void OmsIvacTree::InThr_OnTreeOp(fon9::seed::FnTreeOp fnCallback) {
   TreeOp op{*this};
   fnCallback(fon9::seed::TreeOpResult{this, fon9::seed::OpResult::no_error}, &op);
}

} // namespaces
