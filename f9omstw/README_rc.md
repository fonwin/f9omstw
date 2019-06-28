fon9 rc protocol for OMS
========================

* TODO: 重複登入時間及來源(即時通知?)、上次(及目前)連線時間及來源...


## f9oms API: Function code = 0x08: f9oms api
* `unsigned tableId`
  * tableId == 0 各類查詢、操作: [`enum f9OmsRc_OpKind`](OmsRc.h)
  * tableId != 0
    * C -> S 下單要求.
    * C <- S 回報.
  * 若 tableId 不正確, 則會立即強制登出.

---------------------------------------

### client 下單要求(C -> S): tableId = "REQ." 下單表格序號
* `tableId != 0`
* `下單要求字串` 參考下單要求欄位: Layouts 的 "REQ."

### server 回報(C <- S): tableId = "RPT." 回報表格序號
* `tableId != 0`
* `OmsRxSNO reportSNO`  從1開始依序編號, 不一定連續.
  * 如果為 0, 則表示該筆為 request abandon, 是沒有進入系統的拒絕, 無法回補.
    * 此時沒有 `OmsRxSNO referenceSNO`
  * 下次連線後的回補(訂閱)要求, 應從「TDay + (最後收到的序號+1)」開始.
    * 回補時 server 端會檢查: 若 TDay 不正確, 則拒絕回補.
* `OmsRxSNO referenceSNO`
  * 初始下單要求、事件回報: 此值為 0
  * 接續的下單要求(刪改查): 此值為 IniSNO = 初始下單要求的 reportSNO
  * 委託異動回報: 此值為來源下單要求的 reportSNO
  * Client 端必須自行使用 SNO 配對「下單要求」與「委託異動」
* `RptTable 的回報字串`

---------------------------------------

### QueryConfig: f9OmsRc_OpKind_Config
#### client 發起
* `tableId = 0`
* `f9OmsRc_OpKind_Config`

* 在 client 發起 f9OmsRc_OpKind_Config 之前, 禁止其他操作.
* 只能在連線後發起一次, 重複發起將會被斷線.
* 在收到回覆之前, 可能會先收到 TDayChanged, 此時應先將 TDay 保留,
  等收到回覆時若 TDayChanged.TDay > Config.TDay, 則需回覆 TDayConfirm, 等候新的 Config;

#### server 回覆
* `tableId = 0`
* `f9OmsRc_OpKind_Config`
* `fon9::TimeStamp TDay`
* `std::string     SeedPath`
* `uint32_t        HostId`
* `std::string     Layouts`
* `std::string     Tables`

* Layouts 使用底下的形式提供:
  若為空白, 則表示使用上次設定.
  * Request: Client 使用此格式送單
    ```
    REQ.Named {\n
      TypeId FieldNamed\n
    }\n
    ```
  * Request: Server 使用此格式送回報
    ```
    RPT.Named [:ex] {\n
      TypeId FieldNamed\n
    }\n
    ```
    * 回報名稱可能重複, 表示應歸入同一種回報項目, 但回報的欄位不同.
      例如: 對於 TwsOrd 可能有數種回報格式, 針對同一筆回報,
           如果回報格式無某些欄位, 則表示那些欄位不變動.
    * 下單要求回報, 可能會有額外參數: `abandon`、`ini`
      * `RPT.TwsNew : abandon`   表示此格式用來回報「無對應 order 的下單要求失敗」
      * `RPT.TwsNew : ini`       表示 TwsNew 可當作「初始下單要求」
      * `RPT.TwsChg : abandon`   表示此格式用來回報「無對應 order 的下單要求失敗」
      * `RPT.TwsChg`             沒有額外參數的下單要求回報
    * 委託異動回報, 額外參數 = 下單要求來源格式, 表示是因為該類的下單要求造成的異動
      * `RPT.TwsOrd : TwsNew`
      * `RPT.TwsOrd : TwsChg`
      * `RPT.TwsOrd : TwsFilled`
    * 事件回報, 額外參數 = `event`
      * `RPT.MktSes : event`
      * `RPT.LineSt : event`

* Tables 使用 gv 格式提供
  若為空白, 則表示使用上次設定.
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

---------------------------------------

### TDayChanged: f9OmsRc_OpKind_TDayChanged, f9OmsRc_OpKind_TDayConfirm
#### server 發起 f9OmsRc_OpKind_TDayChanged
* `tableId = 0`
* `f9OmsRc_OpKind_TDayChanged`
* `fon9::TimeStamp TDay`

#### client 回覆 f9OmsRc_OpKind_TDayConfirm
* `tableId = 0`
* `f9OmsRc_OpKind_TDayConfirm`
* `fon9::TimeStamp TDay`

* 當 server 收到 TDayConfirm, 且 TDay 正確,
  則會回覆 f9OmsRc_OpKind_Config (此時的 Layouts, Tables 可能回空字串),
  若 TDay 不正確(可能TDay又變了?!), 則拋棄此次 TDayConfirm, 等候正確的 TDayConfirm.
* 在 client 回覆 f9OmsRc_OpKind_TDayConfirm 之後, 收到 f9OmsRc_OpKind_Config 之前,
  client 不應有任何的 OMS 操作.

---------------------------------------

### Report Recover & Subscribe: f9OmsRc_OpKind_ReportSubscribe
#### client 發起 f9OmsRc_OpKind_ReportSubscribe
* `tableId = 0`
* `f9OmsRc_OpKind_ReportSubscribe`
* `fon9::TimeStamp TDay`
* `OmsRxSNO        From`
* `f9OmsRc_RptFilter`

* 不理會相同 TDay 的重覆要求.
* 不理會不同 TDay 的要求.

#### server 回覆 f9OmsRc_OpKind_ReportSubscribe
* `tableId = 0`
* `f9OmsRc_OpKind_ReportSubscribe`
* `fon9::TimeStamp TDay`
* `OmsRxSNO        PublishedSNO`

* 表示回補結束, 接下來從 PublishedSNO+1 傳送即時回報.
