// \file f9omstw/OmsRequestRunner.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRequestRunner_hpp__
#define __f9omstw_OmsRequestRunner_hpp__
#include "f9omstw/OmsRequestTrade.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "fon9/RevPrint.hpp"

namespace f9omstw {

/// user(session) thread 的下單要求物件
class OmsRequestRunner {
   fon9_NON_COPYABLE(OmsRequestRunner);
public:
   OmsRequestTradeSP    Request_;
   fon9::RevBufferList  ExLog_;

   OmsRequestRunner() : ExLog_{128} {
   }
   OmsRequestRunner(fon9::StrView exlog) : ExLog_{static_cast<fon9::BufferNodeSize>(exlog.size() + 128)} {
      fon9::RevPrint(this->ExLog_, exlog, '\n');
   }

   OmsRequestRunner(OmsRequestRunner&&) = default;
   OmsRequestRunner& operator=(OmsRequestRunner&&) = default;

   /// \copydoc bool OmsRequestTrade::ValidateInUser(OmsRequestRunner&);
   bool ValidateInUser() {
      return this->Request_->ValidateInUser(*this);
   }
   OmsOrderRaw* BeforeRunInCore(OmsResource& res) {
      return this->Request_->BeforeRunInCore(*this, res);
   }

   void RequestAbandon(OmsResource* res, OmsErrCode errCode);
   void RequestAbandon(OmsResource* res, OmsErrCode errCode, std::string reason);
};

/// - 禁止使用 new 的方式建立 OmsRequestRunnerInCore;
///   只能用「堆疊變數」的方式, 並在解構時自動結束更新.
/// - 變動中的委託: this->OrderRaw_
///   - this->OrderRaw_.Order_->Last() == &this->OrderRaw_;
///   - this->OrderRaw_.Request_->LastUpdated_ 尚未設定成 &this->OrderRaw_;
/// - 解構時: ~OmsRequestRunnerInCore()
///   - this->Resource_.Backend_.OnAfterOrderUpdated(*this);
///     - 設定 this->OrderRaw_.Request_->LastUpdated_ = &this->OrderRaw_;
///     - 將 req, this->OrderRaw_ 加入 backend;
class OmsRequestRunnerInCore {
   fon9_NON_COPY_NON_MOVE(OmsRequestRunnerInCore);
public:
   OmsResource&         Resource_;
   OmsOrderRaw&         OrderRaw_;
   /// 例如: 儲存送給交易所的封包.
   fon9::RevBufferList  ExLogForUpd_;
   /// 例如: 儲存下單要求的原始封包.
   fon9::RevBufferList  ExLogForReq_;

   /// 收到下單要求: 準備進行下單流程.
   OmsRequestRunnerInCore(OmsResource& resource, OmsOrderRaw& ordRaw, fon9::RevBufferList&& exLogForReq, fon9::BufferNodeSize updExLogSize)
      : Resource_(resource)
      , OrderRaw_(ordRaw)
      , ExLogForUpd_{updExLogSize}
      , ExLogForReq_(std::move(exLogForReq)) {
   }

   /// 收到交易所回報時: 找回下單要求, 建立委託更新物件.
   template <class ExLogForUpdArg>
   OmsRequestRunnerInCore(OmsResource& resource, OmsOrderRaw& ordRaw, ExLogForUpdArg&& exLogForUpdArg)
      : Resource_(resource)
      , OrderRaw_(ordRaw)
      , ExLogForUpd_{std::forward<ExLogForUpdArg>(exLogForUpdArg)}
      , ExLogForReq_{0} {
   }

   ~OmsRequestRunnerInCore();

   /// 使用 this->OrderRaw_.Order_->Initiator_->Policy()->OrdTeamGroupId(); 的櫃號設定.
   bool AllocOrdNo(OmsOrdNo reqOrdNo);
   /// 使用 tgId 的櫃號設定, 不考慮 Request policy.
   bool AllocOrdNo(OmsOrdTeamGroupId tgId);

   /// 在 this->OrderRaw_.OrdNo_.empty1st() 時, 分配委託書號.
   /// - 如果 !this->OrderRaw_.Order_->Initiator_->OrdNo_.empty1st()
   ///     || this->OrderRaw_.Order_->Initiator_->Policy()->OrdTeamGroupId() != 0;
   ///   也就是下單時有指定櫃號, 或 Policy 有指定櫃號群組.
   ///   則使用 this->AllocOrdNo(this->OrderRaw_.Order_->Initiator_->OrdNo_).
   /// - 否則使用 this->AllocOrdNo(tgId);
   bool AllocOrdNo_IniOrTgid(OmsOrdTeamGroupId tgId) {
      if (!this->OrderRaw_.OrdNo_.empty1st()) // 已編號.
         return true;
      assert(this->OrderRaw_.Request_->RxKind() == f9fmkt_RxKind_RequestNew);
      assert(this->OrderRaw_.Order_->Initiator_ == this->OrderRaw_.Request_);
      auto iniReq = this->OrderRaw_.Order_->Initiator_;
      if (!iniReq->OrdNo_.empty1st() || iniReq->Policy()->OrdTeamGroupId() != 0)
         return this->AllocOrdNo(iniReq->OrdNo_);
      return this->AllocOrdNo(tgId);
   }
};

class OmsRequestRunStep {
   fon9_NON_COPY_NON_MOVE(OmsRequestRunStep);
   OmsRequestRunStepSP  NextStep_;
protected:
   void ToNextStep(OmsRequestRunnerInCore&& runner) {
      if (this->NextStep_)
         this->NextStep_->RunRequest(std::move(runner));
   }

public:
   OmsCore& Core_;
   OmsRequestRunStep(OmsCore& core) : Core_(core) {
   }
   OmsRequestRunStep(OmsCore& core, OmsRequestRunStepSP next)
      : NextStep_{std::move(next)}
      , Core_(core) {
   }
   virtual ~OmsRequestRunStep();

   virtual void RunRequest(OmsRequestRunnerInCore&&) = 0;
};

} // namespaces
#endif//__f9omstw_OmsRequestRunner_hpp__
