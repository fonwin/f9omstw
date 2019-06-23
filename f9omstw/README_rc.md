fon9 rc protocol for OMS
========================

## f9oms API: Function code = 0x08: f9oms api
* `unsigned tableId`
  * tableId == 0 各類查詢、操作: [`enum f9_OmsRcOpKind`](OmsRc.h)

### tableId != 0
#### client 下單要求
* 參考下單要求欄位: Layouts 的 "REQ."
* tableId 之後緊接著 `下單要求字串`

#### server 回覆: 拒絕下單要求 tableId == ~0u
* `unsigned    reqTableId` 原始下單要求的 tableId.
* `OmsErrCode  errc`       錯誤原因.
* `std::string errmsg`     錯誤訊息.
* `std::string reqstr`     原始下單訊息.

#### server 回報: tableId = "RPT." 回報表格序號

---------------------------------------

### Query config: f9_OmsRcOpKind_Config
#### client 發起
* 在 client 發起 f9_OmsRcOpKind_Config 之前, 禁止其他操作.
* 只能在連線後發起一次, 重複發起將會被斷線.
* 在收到回覆之前, 可能會先收到 TDayChanged, 此時應先將 TDay 保留,
  等收到回覆時若 TDayChanged.TDay > Config.TDay, 則需回覆 TDayConfirm, 等候新的 Config;

#### server 回覆
* `fon9::TimeStamp TDay`
* `std::string     SeedPath`
* `uint32_t        HostId`

* Layouts 使用底下的形式提供:
  * REQ.Names { Fields }
  * RPT.Named { }

* Tables 使用 gv 格式提供
  ```
  fon9_kCSTR_LEAD_TABLE("\x02") + TableNamed\n
  FieldNames\n     使用 fon9_kCSTR_CELLSPL("\x01") 分隔
  FieldValues\n    可能有 0..n 行
  ```
  目前有的 tables:
  * OmsPoIvList, 可用帳號
    * IvKey|Rights
    * IvKey = BrkId-IvacNo-SubacNo
    * [Rights = enum class OmsIvRight](OmsPoIvList.hpp)
  * OmsPoUserRights, 使用者權限(可用櫃號、下單流量、查詢流量)
    * OrdTeams|FcReqCount|FcReqMS|FcQryCount|FcQryMS
    * 如果有提供 OrdTeams, 表示 OMS 會用這些櫃號來填委託書號.
      * 如果第1碼為 '*', 表示客戶端可自訂櫃號或委託書號.
      * 除非第1碼為 '*', 否則 client 不能自訂櫃號, 即使 OmsTeams 有提供, 例如:
        * OrdTeams="A"; 表示 OMS 填入的委託書號為 "Axxxx";
        * 但 client 下新單時 OrdNo 不可填 "A".

### TDay changed
`fon9::TimeStamp TDay`
#### server 發起 f9_OmsRcOpKind_TDayChanged
#### client 回覆 f9_OmsRcOpKind_TDayConfirm
* 當 server 收到 TDayConfirm, 且 TDay 正確, 則會回覆 f9_OmsRcOpKind_Config
* 在 client 回覆 f9_OmsRcOpKind_TDayConfirm 之後, 收到 f9_OmsRcOpKind_Config 之前,
  client 不應有任何的 OMS 操作.

### Report recover/subscribe
### Request: 下單要求(新刪改查)
