// \file f9omstw/OmsResource.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsResource_hpp__
#define __f9omstw_OmsResource_hpp__
#include "f9omstw/OmsSymbTree.hpp"
#include "f9omstw/OmsBrkTree.hpp"
#include "f9omstw/OmsBackend.hpp"
#include "f9omstw/OmsOrdTeam.hpp"
#include "f9omstw/OmsRequestRunner.hpp"
#include "fon9/seed/MaTree.hpp"

namespace f9omstw {

/// OMS 所需的資源, 集中在此處理.
/// - 這裡的資源都 **不是** thread safe!
/// - 初始化在: 
///   OmsCoreMgrSeed::AddCore()
///   → CreateCore
///   → OmsCoreMgrSeed::InitCoreTables() 由衍生者撰寫, 建立自訂物件:
///     Symbs_; Brks_; UsrDef_;
class OmsResource : public fon9::seed::NamedMaTree {
   fon9_NON_COPY_NON_MOVE(OmsResource);

public:
   OmsCore&    Core_;

   using SymbTreeSP = fon9::intrusive_ptr<OmsSymbTree>;
   SymbTreeSP  Symbs_;

   using BrkTreeSP = fon9::intrusive_ptr<OmsBrkTree>;
   BrkTreeSP   Brks_;

   struct UsrDefObj : public fon9::intrusive_ref_counter<UsrDefObj> {
      char  Padding____[4];
      virtual ~UsrDefObj();
   };
   using UsrDefSP = fon9::intrusive_ptr<UsrDefObj>;
   UsrDefSP UsrDef_;

   OmsBackend           Backend_;
   OmsOrdTeamGroupMgr   OrdTeamGroupMgr_;

   void FetchRequestId(OmsRequestBase& req) {
      this->ReqUID_Builder_.MakeReqUID(req, this->Backend_.FetchSNO(req));
   }
   OmsRxSNO ParseRequestId(const OmsRequestId& reqId) const {
      return this->ReqUID_Builder_.ParseRequestId(reqId);
   }

   const OmsRequestBase* GetRequest(OmsRxSNO sno) const {
      if (auto item = this->Backend_.GetItem(sno))
         return static_cast<const OmsRequestBase*>(item->CastToRequest());
      return nullptr;
   }

   fon9::TimeStamp TDay() const {
      return this->TDay_;
   }

   /// 一旦要求回補, 如果想要取消, 就只能透過 consumer 返回 0 來取消.
   /// 否則就會回補到最後一筆為止.
   /// 回補結束時, 可透過 ReportSubject().Subscribe() 來訂閱即時回報.
   void ReportRecover(OmsRxSNO fromSNO, RxRecover&& consumer) {
      return this->Backend_.ReportRecover(fromSNO, std::move(consumer));
   }
   f9omstw::ReportSubject& ReportSubject() {
      return this->Backend_.ReportSubject_;
   }
   void LogAppend(fon9::RevBufferList&& rbuf) {
      this->Backend_.LogAppend(std::move(rbuf));
   }

protected:
   fon9::TimeStamp   TDay_;
   OmsReqUID_Builder ReqUID_Builder_;

   template <class... NamedArgsT>
   OmsResource(OmsCore& core, NamedArgsT&&... namedargs)
      : fon9::seed::NamedMaTree(std::forward<NamedArgsT>(namedargs)...)
      , Core_(core) {
   }
   ~OmsResource();

   /// 將 this->Symbs_; this->Brks_; 加入 this->Sapling.
   void Plant();
};

//--------------------------------------------------------------------------//

inline OmsRequestRunnerInCore::~OmsRequestRunnerInCore() {
   this->Resource_.Backend_.OnAfterOrderUpdated(*this);
   this->OrderRaw_.Order().EndUpdate(this->OrderRaw_, &this->Resource_);
}

template <class SymbId>
inline OmsSymb* OmsOrder::GetSymb(OmsResource& res, const SymbId& symbid) {
   if (OmsSymb* retval = this->ScResource_.Symb_.get())
      return retval;
   return (this->ScResource_.Symb_ = this->FindSymb(res, ToStrView(symbid))).get();
}
inline OmsSymb* OmsOrder::GetSymb(OmsResource& res, const fon9::StrView& symbid) {
   if (OmsSymb* retval = this->ScResource_.Symb_.get())
      return retval;
   return (this->ScResource_.Symb_ = this->FindSymb(res, symbid)).get();
}

inline OmsIvBaseSP GetIvr(OmsResource& res, const OmsRequestIni& inireq) {
   return res.Brks_->GetIvr(ToStrView(inireq.BrkId_), inireq.IvacNo_, ToStrView(inireq.SubacNo_));
}
inline OmsIvBaseSP FetchIvr(OmsResource& res, const OmsRequestIni& inireq) {
   return res.Brks_->FetchIvr(ToStrView(inireq.BrkId_), inireq.IvacNo_, ToStrView(inireq.SubacNo_));
}
/// 因回報訂閱者會用 order.ScResource().Ivr_ 來判斷是否需要回補給 Client:
///    f9omsrc/OmsRcServerFunc.cpp : OmsRcServerNote::Handler::IsNeedReport()
/// 所以在重載(Backend.Reload)或收到回報(ReportIn)時, 應設定好 order.ScResource().Ivr_;
inline OmsIvBase* FetchScResourceIvr(OmsResource& res, OmsOrder& order, const OmsRequestIni* inireq = nullptr) {
   OmsScResource& scResource = order.ScResource();
   if (auto* ivr = scResource.Ivr_.get())
      return ivr;
   if (inireq || (inireq = order.Initiator()) != nullptr)
      return (scResource.Ivr_ = FetchIvr(res, *inireq)).get();
   return nullptr;
}

//--------------------------------------------------------------------------//

/// 凍結 lastSNO 之前的委託剩餘量: 設定 ordraw->IsFrozeScLeaves_ = true;
void FrozeScLeaves(OmsBackend& backend, OmsRxSNO lastSNO, const OmsBackend::Locker* locker);

} // namespaces
#endif//__f9omstw_OmsResource_hpp__
