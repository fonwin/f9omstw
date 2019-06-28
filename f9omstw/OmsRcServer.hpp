// \file f9omstw/OmsRcServer.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRcServer_hpp__
#define __f9omstw_OmsRcServer_hpp__
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/SimpleFactory.hpp"
#include "fon9/rc/RcSession.hpp"

namespace f9omstw {

using ApiSession = fon9::rc::RcSession;

class ApiReqFieldArg : public fon9::seed::SimpleRawWr {
   fon9_NON_COPY_NON_MOVE(ApiReqFieldArg);
   using base = fon9::seed::SimpleRawWr;
public:
   OmsRequestTrade&  Request_;
   ApiSession&       ApiSession_;
   fon9::StrView     ClientFieldValue_;

   ApiReqFieldArg(OmsRequestTrade& req, ApiSession& ses)
      : base{req}
      , Request_(req)
      , ApiSession_(ses) {
   }
};

struct ApiReqFieldCfg;
typedef void (*FnPutApiField)(const ApiReqFieldCfg& cfg, const ApiReqFieldArg& arg);

/// 如果欄位內容為 IsNull(), 則填入 cfg.ExtParam_;
/// ApiFieldName/SysFieldName = ExtParam, Title, Description
void FnPutApiField_FixedValue(const ApiReqFieldCfg& cfg, const ApiReqFieldArg& arg);

/// 透過註冊擴充自訂 FnPutApiField.
class FnPutApiField_Register : public fon9::SimpleFactoryPark<FnPutApiField_Register> {
   using base = fon9::SimpleFactoryPark<FnPutApiField_Register>;
public:
   using base::base;
   FnPutApiField_Register() = delete;

   /// - 若 fnName 重複, 則註冊失敗, 傳回先註冊的 fnPutApiField.
   /// - 若 fnFieldMaker==nullptr, 則傳回已註冊的 fnPutApiField;
   static FnPutApiField Register(fon9::StrView fnName, FnPutApiField fnFieldMaker);
};

//--------------------------------------------------------------------------//

struct ApiReqFieldCfg {
   ApiReqFieldCfg(const fon9::seed::Field* fld, fon9::Named&& apiNamed)
      : Field_(fld)
      , ApiNamed_(std::move(apiNamed)) {
   }
   ApiReqFieldCfg(const fon9::seed::Field* fld)
      : Field_(fld) {
   }
   ApiReqFieldCfg(const ApiReqFieldCfg&) = default;
   ApiReqFieldCfg(ApiReqFieldCfg&&) = default;
   ApiReqFieldCfg& operator=(const ApiReqFieldCfg&) = delete;

   const fon9::seed::Field* const  Field_;
   fon9::Named       ApiNamed_;
   fon9::CharVector  ExtParam_;
   FnPutApiField     FnPut_{nullptr};
};
struct ApiReqCfg {
   ApiReqCfg(OmsRequestFactory& fac, fon9::Named&& apiName)
      : Factory_{&fac}
      , ApiNamed_{std::move(apiName)} {
   }
   ApiReqCfg(const ApiReqCfg&) = default;
   ApiReqCfg(ApiReqCfg&&) = default;
   ApiReqCfg& operator=(const ApiReqCfg&) = delete;

   OmsRequestFactory* const Factory_;

   /// API端看到的下單要求名稱及描述.
   fon9::Named    ApiNamed_;

   using ApiReqFields = std::vector<ApiReqFieldCfg>;
   /// 由 API 依序填入的欄位.
   ApiReqFields   ApiFields_;

   /// 由系統處理的欄位, 不包含在API封包裡面.
   ApiReqFields   SysFields_;
};
using ApiReqCfgs = std::vector<ApiReqCfg>;

//--------------------------------------------------------------------------//

class ApiRptFieldArg : public fon9::seed::SimpleRawRd {
   fon9_NON_COPY_NON_MOVE(ApiRptFieldArg);
   using base = fon9::seed::SimpleRawRd;
public:
   const OmsRxItem&  Item_;
   ApiRptFieldArg(const OmsRxItem& item)
      : base{item}
      , Item_(item) {
   }
};

struct ApiRptFieldCfg;
/// 如果 arg == nullptr 則在 rbuf 填入 TypeId.
typedef void (*FnRptApiField)(fon9::RevBuffer& rbuf, const ApiRptFieldCfg& cfg, const ApiRptFieldArg* arg);

/// 透過註冊擴充自訂 FnRptApiField.
class FnRptApiField_Register : public fon9::SimpleFactoryPark<FnRptApiField_Register> {
   using base = fon9::SimpleFactoryPark<FnRptApiField_Register>;
public:
   using base::base;
   FnRptApiField_Register() = delete;

   /// - 若 fnName 重複, 則註冊失敗, 傳回先註冊的 fnRptApiField.
   /// - 若 fnFieldMaker==nullptr, 則傳回已註冊的 fnRptApiField;
   static FnRptApiField Register(fon9::StrView fnName, FnRptApiField fnFieldMaker);
};

struct ApiRptFieldCfg {
   ApiRptFieldCfg(const fon9::seed::Field* fld, fon9::Named&& apiNamed)
      : Field_(fld)
      , ApiNamed_(std::move(apiNamed)) {
   }
   ApiRptFieldCfg(const fon9::seed::Field* fld)
      : Field_(fld) {
   }
   ApiRptFieldCfg(const ApiRptFieldCfg&) = default;
   ApiRptFieldCfg(ApiRptFieldCfg&&) = default;
   ApiRptFieldCfg& operator=(const ApiRptFieldCfg&) = delete;

   const fon9::seed::Field* const  Field_;
   fon9::Named       ApiNamed_;
   fon9::CharVector  ExtParam_;
   FnRptApiField     FnRpt_{nullptr};
};
struct ApiRptCfg {
   ApiRptCfg(fon9::seed::Tab& fac, fon9::Named&& apiName)
      : Factory_{&fac}
      , ApiNamed_{std::move(apiName)} {
   }
   ApiRptCfg(const ApiRptCfg&) = default;
   ApiRptCfg(ApiRptCfg&&) = default;
   ApiRptCfg& operator=(const ApiRptCfg&) = delete;

   fon9::seed::Tab* const Factory_;

   /// API端看到的回報名稱及描述.
   fon9::Named ApiNamed_;

   using RptFields = std::vector<ApiRptFieldCfg>;
   /// 回報時依序填入的欄位.
   RptFields   RptFields_;
};

fon9_WARN_DISABLE_PADDING;
class ApiRptCfgs {
   fon9_NON_COPY_NON_MOVE(ApiRptCfgs);
   using Configs = std::vector<ApiRptCfg>;
   using IndexMap = std::vector<unsigned>;
   Configs  Configs_;
   IndexMap IndexMap_;

   const ApiRptCfg* GetRptCfg(unsigned idx) const {
      if (idx >= this->IndexMap_.size())
         return nullptr;
      if ((idx = this->IndexMap_[idx] - 1) < this->Configs_.size())
         return &this->Configs_[idx];
      return nullptr;
   }
public:
   enum RequestE : unsigned {
      /// 一般下單要求(有對應的 order) 回報格式設定.
      RequestE_Normal = 0,
      /// abandon 下單要求(無對應的 order) 回報格式設定.
      RequestE_Abandon = 1,
      RequestE_Max = 1,
      RequestE_Size = RequestE_Max + 1,
   };
   struct FactoryToIndex {
      fon9_NON_COPY_NON_MOVE(FactoryToIndex);
      const unsigned RequestFactoryCount_;
      const unsigned OrderFactoryCount_;
      const unsigned EventFactoryCount_;
      const unsigned OrderTableIdOffset_;
      const unsigned EventTableIdOffset_;

      FactoryToIndex(const OmsCoreMgr& coreMgr)
         : RequestFactoryCount_{static_cast<unsigned>(coreMgr.RequestFactoryPark().GetTabCount())}
         , OrderFactoryCount_{static_cast<unsigned>(coreMgr.OrderFactoryPark().GetTabCount())}
         , EventFactoryCount_{static_cast<unsigned>(coreMgr.EventFactoryPark().GetTabCount())}
         , OrderTableIdOffset_{RequestFactoryCount_ * RequestE_Size}
         , EventTableIdOffset_{OrderTableIdOffset_ + OrderFactoryCount_ * RequestFactoryCount_} {
      }
      unsigned RequestIndex(OmsRequestFactory& fac, RequestE reqe) const {
         const unsigned idx = static_cast<unsigned>(fac.GetIndex());
         if(idx < this->RequestFactoryCount_ && static_cast<unsigned>(reqe) < RequestE_Size)
            return idx * RequestE_Size + reqe;
         return static_cast<unsigned>(-1);
      }
      unsigned OrderIndex(OmsOrderFactory& ordfac, OmsRequestFactory& reqfac) const {
         const unsigned ordidx = static_cast<unsigned>(ordfac.GetIndex());
         const unsigned reqidx = static_cast<unsigned>(reqfac.GetIndex());
         if (ordidx < this->OrderFactoryCount_ && reqidx < this->RequestFactoryCount_)
            return ordidx * this->RequestFactoryCount_ + reqidx + this->OrderTableIdOffset_;
         return static_cast<unsigned>(-1);
      }
      unsigned EventIndex(OmsEventFactory& fac) const {
         const unsigned idx = static_cast<unsigned>(fac.GetIndex());
         if (idx < this->EventFactoryCount_)
            return idx + this->EventTableIdOffset_;
         return static_cast<unsigned>(-1);
      }
   };

   FactoryToIndex ToIndex_;

   struct Parser;

   ApiRptCfgs(const OmsCoreMgr& coreMgr) : ToIndex_{coreMgr} {
   }

   bool empty() const {
      return this->Configs_.empty();
   }
   const ApiRptCfg* GetRptCfgRequest(OmsRequestFactory& fac, RequestE reqe) const {
      return this->GetRptCfg(this->ToIndex_.RequestIndex(fac, reqe));
   }
   const ApiRptCfg* GetRptCfgOrder(OmsOrderFactory& ordfac, OmsRequestFactory& reqfac) const {
      return this->GetRptCfg(this->ToIndex_.OrderIndex(ordfac, reqfac));
   }
   const ApiRptCfg* GetRptCfgEvent(OmsEventFactory& fac) const {
      return this->GetRptCfg(this->ToIndex_.EventIndex(fac));
   }
   unsigned RptTableId(const ApiRptCfg& cfg) const {
      return static_cast<unsigned>(&cfg - &*this->Configs_.begin());
   }
};

//--------------------------------------------------------------------------//

class ApiSesCfg : public fon9::intrusive_ref_counter<ApiSesCfg> {
   fon9_NON_COPY_NON_MOVE(ApiSesCfg);
   OmsCoreSP      CurrentCore_;
   fon9::SubConn  SubrRpt_{};
   fon9::SubConn  SubrTDay_{};
   void OnReport(OmsCore&, const OmsRxItem&);
   void UnsubscribeRpt();
   void SubscribeRpt(OmsCore& core);
public:
   ApiSesCfg(OmsCoreMgrSP coreMgr)
      : CoreMgr_{coreMgr}
      , ApiRptCfgs_(*coreMgr) {
   }
   ~ApiSesCfg();

   const OmsCoreMgrSP   CoreMgr_;

   /// 給使用這組設定的 Session 取的名字, 可填入 Request.SesName_ 欄位.
   /// - 設定方式: 在設定檔裡面提供變數 `$SessionName = name`.
   std::string SessionName_{"OmsRcApi"};

   /// ApiSession 登入成功, 回覆給 Client 的下單表格結構訊息.
   std::string ApiSesCfgStr_;

   /// Request map, index = Client 端填入的 form id.
   ApiReqCfgs  ApiReqCfgs_;

   /// 回報給 API 的格式.
   ApiRptCfgs  ApiRptCfgs_;

   /// 由 MakeApiSesCfg() 呼叫訂閱.
   void SubscribeReport();

   /// 收到 OmsCore 的即時回報後, 編製的回報訊息.
   /// 包含開頭的 rptTableId、reportSNO、referenceSNO (Bitv格式);
   std::string       ReportMessage_;
   const OmsRxItem*  ReportMessageFor_{nullptr};
   /// 建立 item 的回報訊息.
   /// 包含開頭的 rptTableId、reportSNO、referenceSNO (Bitv格式);
   /// \retval false 此 item 不用回報(沒設定 RPT);
   bool MakeReport(fon9::RevBufferList& rbuf, const OmsRxItem& item) const;
};
using ApiSesCfgSP = fon9::intrusive_ptr<const ApiSesCfg>;
fon9_WARN_POP;

//--------------------------------------------------------------------------//

/// - cfgstr
///   - `$` 開頭, 使用 cfgld.Append(cfgstr); 載入設定.
///     - e.g. `$TxLang={zh} $include:forms/ApiAll.cfg`
///   - 非 `$` 開頭 = API表格設定檔, 使用 cfgld.LoadFile(cfgstr); 載入設定.
///     - e.g. `forms/ApiAll.cfg`
/// - 可能會並拋出 ConfigLoader::Err 異常, 若沒拋出異常, 則傳回值必定有效(不會是 nullptr).
ApiSesCfgSP MakeApiSesCfg(OmsCoreMgrSP coreMgr, fon9::StrView cfgstr);

} // namespaces
#endif//__f9omstw_OmsRcServer_hpp__
