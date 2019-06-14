// \file f9omstw/OmsRcServer.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRcServer_hpp__
#define __f9omstw_OmsRcServer_hpp__
#include "f9omstw/OmsRequestTrade.hpp"
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

fon9_WARN_DISABLE_PADDING;
struct ApiSesCfg : public fon9::intrusive_ref_counter<ApiSesCfg> {
   fon9_NON_COPY_NON_MOVE(ApiSesCfg);
   ApiSesCfg(OmsCoreMgrSP coreMgr) : CoreMgr_{std::move(coreMgr)} {
   }

   const OmsCoreMgrSP   CoreMgr_;

   /// 給使用這組設定的 Session 取的名字, 可填入 Request.SesName_ 欄位.
   /// - 設定方式: 在設定檔裡面提供變數 `$SessionName = name`.
   std::string SessionName_{"OmsRcApi"};

   /// ApiSession 登入成功, 回覆給 Client 的下單表格結構訊息.
   std::string ApiSesCfgStr_;

   /// Request map, index = Client 端填入的 form id.
   ApiReqCfgs  ApiReqCfgs_;

   /// Report(Order) map: 回報給 API 的格式.
   // 可考慮在此訂閱 OmsCore 事件, 收到事件後編製回報訊息, 然後再通知 ApiSession;
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
