// \file f9omstw/OmsChildRequest.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsChildRequest_hpp__
#define __f9omstw_OmsChildRequest_hpp__
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

template <class RequestBase>
class OmsChildRequestT : public RequestBase {
   fon9_NON_COPY_NON_MOVE(OmsChildRequestT);
   using base = RequestBase;
   OmsRxSNO                ParentRequestSNO_{};
   const OmsRequestBase*   ParentRequest_{};

protected:
   /// 不論現在的 ParentRequestSNO 內容, 都要強制重設;
   /// 因為 this 可能為遠端主機送來的, 重設前 ParentRequestSNO 為遠端主機的序號, 此時應重設為本機的 parentRequest.
   void SetParentRequest(const OmsRequestBase& parentRequest, OmsRequestRunnerInCore* parentRunner) override {
      assert(this->ParentRequest_ == nullptr);
      assert(parentRequest.IsParentRequest());
      if (fon9_LIKELY(this->ParentRequest_ == nullptr)) {
         this->ParentRequestSNO_ = parentRequest.RxSNO();
         this->ParentRequest_ = &parentRequest;
         parentRequest.OnAddChildRequest(*this, parentRunner);
      }
   }
   const OmsRequestBase* GetParentRequest() const override {
      return this->ParentRequest_;
   }
   OmsRxSNO GetParentRequestSNO() const override {
      return this->ParentRequestSNO_;
   }
   void ClearParentRequest() override {
      this->ParentRequestSNO_ = 0;
      this->ParentRequest_ = nullptr;
   }

   void SetParentReport(const OmsRequestBase* parentReport) override {
      assert(this->ParentRequest_ == nullptr);
      assert(parentReport == nullptr || parentReport->IsParentRequest());
      this->ParentRequestSNO_ = 0;
      if (parentReport && parentReport->IsParentRequest()) {
         this->ParentRequest_ = parentReport;
         // - 此時 parentReport->RxSNO() 可能為 0,
         //   因為 parentReport 可能尚未 RunInCore();
         // - 此時必定還在回報接收程序的 thread,
         //   所以不可在此通知 parent->OnAddChildRequest(),
         //   因為 parent 無法保證 thread-safe.
         // - 所以必須在 this->RunReportInCore() 時再補上.
      }
      else {
         // 找不到對應回報的母單, 必須清除 ParentRequestSNO_, 以免系統參考到不正確的母單.
         // 因為此時 this->ParentRequestSNO_ 為回報來源端的序號, 與本地序號無關.
      }
   }
   void RunReportInCore(OmsReportChecker&& checker) override {
      if (this->ParentRequestSNO_ == 0 && this->ParentRequest_) {
         if ((this->ParentRequestSNO_ = this->ParentRequest_->RxSNO()) == 0) {
            // 回報的母單沒有進入回報流程, 無法建立子母單的關聯.
            this->ParentRequest_ = nullptr;
         }
         else {
            this->ParentRequest_->OnAddChildRequest(*this, nullptr);
         }
      }
      base::RunReportInCore(std::move(checker));
   }

public:
   using base::base;
   OmsChildRequestT() = default;

   static void MakeFields(fon9::seed::Fields& flds) {
      base::MakeFields(flds);
      flds.Add(fon9_MakeField2(OmsChildRequestT, ParentRequestSNO));
   }
};

/// 提供 ChildId 欄位;
template <class RequestBase>
class OmsChildIdRequestT : public OmsChildRequestT<RequestBase> {
   fon9_NON_COPY_NON_MOVE(OmsChildIdRequestT);
   using base = OmsChildRequestT<RequestBase>;
public:
   using base::base;
   OmsChildIdRequestT() = default;
   static void MakeFields(fon9::seed::Fields& flds) {
      base::MakeFields(flds);
      base::AddChildIdField(flds);
   }
};

} // namespaces
#endif//__f9omstw_OmsChildRequest_hpp__
