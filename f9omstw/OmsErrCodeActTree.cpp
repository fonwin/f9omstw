// \file f9omstw/OmsErrCodeActTree.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsErrCodeActTree.hpp"
#include "fon9/ConfigLoader.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {
using namespace fon9;
using namespace fon9::seed;

class ErrCodeActSeed::ErrCodeActTree : public TreeBase {
   fon9_NON_COPY_NON_MOVE(ErrCodeActTree);

   static LayoutSP MakeLayout() {
      Fields fields;
      fields.Add(fon9_MakeField2(OmsErrCodeAct, ConfigStr));
      fields.Add(fon9_MakeField2(OmsErrCodeAct, Memo));
      fields.Add(fon9_MakeField2(OmsErrCodeAct, ConfigAt));
      return new Layout1(fon9_MakeField(ErrCodeActMap::value_type, first, "ErrCode"),
                         new Tab(fon9::Named{"config"}, std::move(fields),
                                 TabFlag::NoSapling | TabFlag::NoSeedCommand));
   }
   using TreeBase::Container_;

   struct PodOp : public PodOpDefault {
      fon9_NON_COPY_NON_MOVE(PodOp);
      using base = PodOpDefault;
      OmsErrCodeActSP  Seed_;
      PodOp(ContainerImpl::value_type& v, Tree& sender, OpResult res, const StrView& key, Locker&)
         : base{sender, res, key}
         , Seed_{v.second} {
      }
      void BeginRead(Tab& tab, FnReadOp fnCallback) override {
         this->BeginRW(tab, std::move(fnCallback), SimpleRawRd{*this->Seed_});
      }
   };

   struct TreeOp : public fon9::seed::TreeOp {
      fon9_NON_COPY_NON_MOVE(TreeOp);
      using base = fon9::seed::TreeOp;
      using base::base;
      TreeOp(ErrCodeActTree& tree) : base(tree) {
      }
      static void MakeSeedView(ContainerImpl::iterator ivalue, Tab* tab, RevBuffer& rbuf) {
         if (tab)
            FieldsCellRevPrint(tab->Fields_, SimpleRawRd{*ivalue->second}, rbuf, GridViewResult::kCellSplitter);
         RevPrint(rbuf, ivalue->first);
      }
      void GridView(const GridViewRequest& req, FnGridViewOp fnCallback) override {
         TreeOp_GridView_MustLock(*this, static_cast<ErrCodeActTree*>(&this->Tree_)->Container_,
                                  req, std::move(fnCallback), &MakeSeedView);
      }
      void Get(StrView strKeyText, FnPodOp fnCallback) override {
         TreeOp_Get_MustLock<PodOp>(*this, static_cast<ErrCodeActTree*>(&this->Tree_)->Container_,
                                    strKeyText, std::move(fnCallback));
      }
   };

public:
   ErrCodeActTree() : TreeBase{MakeLayout()} {
   }
   void OnTreeOp(FnTreeOp fnCallback) {
      TreeOp op{*this};
      fnCallback(TreeOpResult{this, OpResult::no_error}, &op);
   }
};
//--------------------------------------------------------------------------//
ErrCodeActSeed::ErrCodeActSeed(std::string name)
   : base(new ErrCodeActTree, std::move(name)) {
}
//--------------------------------------------------------------------------//
void ErrCodeActSeed::OnSeedCommand(SeedOpResult& res, StrView cmdln,
                                   FnCommandResultHandler resHandler,
                                   MaTree::Locker&& ulk,
                                   SeedVisitor*) {
   StrView cmd = StrFetchTrim(cmdln, &isspace);
   StrTrim(&cmdln);
   if (cmd == "reload") {
      // reload [cfgFileName]
      ulk.lock();
      this->ReloadErrCodeAct(cmdln, std::move(ulk));
      return;
   }
   if (cmd == "?") {
      res.OpResult_ = OpResult::no_error;
      resHandler(res,
                 "reload" fon9_kCSTR_CELLSPL "Reload ErrCode config" fon9_kCSTR_CELLSPL "[Config filename]");
      return;
   }
   res.OpResult_ = OpResult::not_supported_cmd;
   resHandler(res, cmd);
}
void ErrCodeActSeed::ReloadErrCodeAct(fon9::StrView cfgfn, fon9::seed::MaTree::Locker&& lk) {
   assert(lk.owns_lock());
   std::string strfn = cfgfn.empty() ? this->GetTitle() : cfgfn.ToString();
   lk.unlock();

   ConfigLoader      cfgldr{std::string{}};
   OmsErrCodeActAry  acts;
   ErrCodeActMap     view;
   std::string       msg;
   try {
      cfgldr.CheckLoad(&strfn);
      StrView cfgs{&cfgldr.GetCfgStr()};
      while (!fon9::StrTrimHead(&cfgs).empty()) {
         StrView    ecStr = fon9::StrFetchTrim(cfgs, ':');
         StrView    actStr = fon9::StrFetchTrim(cfgs, '\n');
         if (ecStr.empty() || actStr.empty())
            continue;
         auto       from = cfgldr.GetLineFrom(ecStr.begin());
         unsigned   ecn = fon9::StrTo(ecStr, 0u);
         OmsErrCode ec = static_cast<OmsErrCode>(ecn);
         if (ecn != ec || ecn == 0) {
            msg = fon9::RevPrintTo<std::string>("Bad ErrCode=", ecStr, '|', from);
            goto __CONFIG_ERR;
         }
         OmsErrCodeAct*  pact;
         OmsErrCodeActSP actsp{pact = new OmsErrCodeAct{ec}};
         if (const char* errpos = pact->ParseFrom(actStr, &msg)) {
            msg = fon9::RevPrintTo<std::string>("err=", msg, '|', cfgldr.GetLineFrom(errpos));
            goto __CONFIG_ERR;
         }
         pact->ConfigAt_ = fon9::RevPrintTo<std::string>(from);

         ErrCodeActKey    cfgkey = ErrCodeActKey::Make<0>(ecn);
         OmsErrCodeActSP* pactsp = &acts[ec];
         while (pactsp->get()) {
            pactsp = &(const_cast<OmsErrCodeAct*>(pactsp->get()))->Next_;
            cfgkey.SetOrigValue(cfgkey.GetOrigValue() + 1);
         }
         view.kfetch(cfgkey).second = actsp;
         *pactsp = std::move(actsp);
      }
   }
   catch (fon9::ConfigLoader::Err& e) {
      msg = fon9::RevPrintTo<std::string>(e);
      goto __CONFIG_ERR;
   }
   if (!msg.empty()) {
   __CONFIG_ERR:;
      std::replace(msg.begin(), msg.end(), '\n', '|');
      lk.lock();
      if (this->GetTitle().empty())
         this->SetTitle(std::move(strfn));
      this->SetDescription(std::move(msg));
      return;
   }
   else {
      auto saplingLocker{static_cast<TreeBase*>(this->Sapling_.get())->Lock()};
      saplingLocker->swap(view);
      this->ErrCodeActs_.swap(acts);
      // unlock saplingLocker at dtor.
   }
   msg = fon9::RevPrintTo<std::string>("Loaded at ", fon9::LocalNow(), kFmtYMD_HH_MM_SS_us);
   lk.lock();
   if (!cfgfn.empty())
      this->SetTitle(std::move(strfn));
   this->SetDescription(std::move(msg));
}

} // namespaces
