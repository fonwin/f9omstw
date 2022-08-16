// \file f9omstw/OmsReportRunner.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsReportRunner_hpp__
#define __f9omstw_OmsReportRunner_hpp__
#include "f9omstw/OmsRequestRunner.hpp"
#include "f9omstw/OmsErrCodeAct.hpp"

namespace f9omstw {

fon9_WARN_DISABLE_PADDING;
enum class OmsReportCheckerSt {
   Checking,
   /// OmsReportChecker::ReportAbandon(); 之後的狀態.
   Abandoned,
   /// 已收過的重複回報, e.g. 改量結果的 BeforeQty 相同.
   Duplicated,
   /// e.g. 已收到 Sending 回報, 再收到 Queuing 回報; 此時的 Queuing 回報狀態.
   Obsolete,
   /// e.g. 改價1(成功於09:01:01) => 改價2(成功於09:01:02);
   /// 先收到改價2回報, 再收到改價1回報; 此時的改價1回報狀態.
   Expired,
   /// 確定沒收過的回報(已經不可能是 Duplicated), 但仍需檢查是否為 Expired?
   /// 這類回報必定會在 Order 留下記錄.
   NotReceived,
};

/// 在 OmsCore 裡面收到 ReportIn 要求, 但尚未決定是否需要更新 Order.
class OmsReportChecker {
   fon9_NON_COPYABLE(OmsReportChecker);
public:
   OmsResource&         Resource_;
   OmsRequestSP         Report_;
   fon9::RevBufferList  ExLog_;
   OmsReportCheckerSt   CheckerSt_{};

   OmsReportChecker(OmsResource& res, OmsRequestRunner&& src)
      : Resource_(res)
      , Report_{std::move(src.Request_)}
      , ExLog_{std::move(src.ExLog_)} {
   }

   /// 如果傳回 nullptr, 則必定已經呼叫過 this->ReportAbandon();
   OmsOrdNoMap* GetOrdNoMap();

   /// 用 this->Report_->ReqUID_ 尋找原始下單要求.
   /// \retval !nullptr 表示找到了原本的 Request;
   ///   - this->CheckerSt_ == OmsReportCheckerSt::NotReceived; 找到了 Req, 但此筆 Rpt 確定沒收到過.
   ///   - this->CheckerSt_ 與呼叫前相同(預設為 OmsReportCheckerSt::Checking)
   /// \retval nullptr  表示沒找到原本的 Request;
   ///   - this->CheckerSt_ == OmsReportCheckerSt::Abandoned;
   ///   - this->CheckerSt_ 與呼叫前相同(預設為 OmsReportCheckerSt::Checking)
   const OmsRequestBase* SearchOrigRequestId();

   /// 拋棄此次回報, 僅使用 ExInfo 方式記錄 log.
   void ReportAbandon(fon9::StrView reason);
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

/// 若 rpt 有 Message_ 欄位, 則將 rpt->Message_ 複製 or 移動到 ordraw->Message_;
template <class ReportT>
inline auto OmsAssignReportMessage(OmsOrderRaw* ordraw, ReportT* rpt)
->decltype(rpt->Message_, void()) {
   if (&ordraw->Request() == rpt)
      ordraw->Message_ = rpt->Message_;
   else { // ordraw 異動的是已存在的 request, this 即將被刪除, 所以使用 std::move(this->Message_)
      ordraw->Message_ = std::move(rpt->Message_);
   }
}
inline void OmsAssignReportMessage(...) {
}

fon9_WARN_DISABLE_PADDING;
/// - 協助處理 ErrCodeAct.
/// - 建構時設定:
///   - this->ErrCodeAct_;
///   - this->OrderRaw().ErrCode_;
///   - this->OrderRaw().RequestSt_;
///     填入 this->ErrCodeAct_->ReqSt_ 或 rpt->ReportSt();
///   - 若條件成立, 但需等 NewDone 才 Rerun, 則應將此次異動放到 ReportPending:
///     this->OrderRaw().UpdateOrderSt_ = f9fmkt_OrderSt_ReportPending;
class OmsReportRunnerInCore : public OmsRequestRunnerInCore {
   fon9_NON_COPY_NON_MOVE(OmsReportRunnerInCore);
   using base = OmsRequestRunnerInCore;
   int  RequestRunTimes_{-1};
   bool HasDeleteRequest_{false};
   bool IsReportPending_{false};

   void CalcRequestRunTimes();
   void UpdateReportImpl(OmsRequestBase& rpt);
   using base::Update;

   static OmsErrCodeActSP GetErrCodeAct(OmsReportRunnerInCore& runner, const OmsRequestBase* rpt);
   static OmsErrCodeActSP CheckErrCodeAct(OmsReportRunnerInCore& runner, const OmsRequestBase* rpt);

public:
   const OmsErrCodeActSP   ErrCodeAct_;

   fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list
   OmsReportRunnerInCore(OmsReportChecker&& checker, OmsOrderRaw& ordRaw)
      : base{checker.Resource_, ordRaw, std::move(checker.ExLog_)}
      , ErrCodeAct_{CheckErrCodeAct(*this, checker.Report_.get())} {
      if (checker.Report_.get() == &ordRaw.Request())
         this->ExLogForReq_ = std::move(this->ExLogForUpd_);
   }
   OmsReportRunnerInCore(OmsResource& resource, OmsOrderRaw& ordRaw)
      : base{resource, ordRaw}
      , ErrCodeAct_{CheckErrCodeAct(*this, nullptr)} {
   }
   /// 接續前一次執行完畢後, 的繼續執行.
   /// prevRunner 可能是同一個 order, 也可能是 child 或 parent;
   OmsReportRunnerInCore(const OmsRequestRunnerInCore& prevRunner, OmsOrderRaw& ordRaw)
      : base{prevRunner, ordRaw}
      , ErrCodeAct_{CheckErrCodeAct(*this, nullptr)} {
   }
   OmsReportRunnerInCore(OmsResource& resource, OmsOrderRaw& ordRaw, const OmsRequestRunnerInCore* prevRunner)
      : base{resource, ordRaw, prevRunner}
      , ErrCodeAct_{CheckErrCodeAct(*this, nullptr)} {
   }
   fon9_MSC_WARN_POP;

   ~OmsReportRunnerInCore();

   unsigned RequestRunTimes() {
      if (this->RequestRunTimes_ < 0)
         this->CalcRequestRunTimes();
      return static_cast<unsigned>(this->RequestRunTimes_);
   }

   /// 回報最後的更新:
   /// - 若 rpt 有 Message_ 欄位, 則將 rpt.Message_ 複製或移動到 this->OrderRaw().Message_;
   /// - 若 rpt.RequestFlags() 有設定 OmsRequestFlag_ReportNeedsLog; 且 rpt 是現有 request 的回報:
   ///   - 則將 rpt 內容寫入 log.
   /// - 如果 this->OrderRaw().UpdateOrderSt_ == f9fmkt_OrderSt_ReportStale 則:
   ///   - this->OrderRaw().Message_.append("(stale)");
   /// - 最後呼叫 base::Update(this->OrderRaw().RequestSt_);
   template <class ReportT>
   void UpdateReport(ReportT& rpt) {
      OmsAssignReportMessage(&this->OrderRaw(), &rpt);
      this->UpdateReportImpl(rpt);
   }
   void UpdateSt(f9fmkt_OrderSt ordst, f9fmkt_TradingRequestSt reqst) {
      this->OrderRaw().UpdateOrderSt_ = ordst;
      this->Update(reqst);
   }
   void UpdateFilled(f9fmkt_OrderSt ordst, const OmsReportFilled& rptFilled) {
      this->UpdateSt(ordst, rptFilled.ReportSt());
   }
};
fon9_WARN_POP;

} // namespaces
#endif//__f9omstw_OmsReportRunner_hpp__
