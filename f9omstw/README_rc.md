fon9 rc protocol for OMS
========================

## f9oms API: Function code = 0x08: f9oms api
* `uint32_t reqTableId`
  * reqTableId == 0 各類查詢、操作.
    * `enum OmsRcOpKind`
  * reqTableId != 0 下單要求.
    * 參考下單要求欄位.

### Query config: OmsRcOpKind::Config
#### client 發起
* 在 client 發起 OmsRcOpKind::Config 之前, 禁止其他操作.
* 只能在連線後發起一次, 重複發起將會被斷線.
* 在收到回覆之前, 可能會先收到 TDayChanged, 此時應先將 TDay 保留,
  等收到回覆時若 TDayChanged.TDay > Config.TDay, 則需回覆 TDayConfirm, 等候新的 Config;

#### server 回覆
* `std::string SeedPath`
* `fon9::TimeStamp TDay`
* `uint32_t HostId`
* 其他使用 ConfigLoader 形式提供:
  * Layouts
    * REQ.Names {}
    * RPT.Named {}
    * Table.Named {}
      * Table.IvList { C26L IvKey\n U1 Rights\n }
  * Values(Variables)
    * 可用帳號: `$IvList = { IvKey|Rights\n ... }`
    * 可用櫃號: `$OrdTeams = 使用「,」分隔櫃號`
    * 下單流量: `$FcRequest = count/ms`
    * 查詢流量: `$FcQuery = count/ms`

### TDay changed
`fon9::TimeStamp TDay`
#### server 發起 OmsRcOpKind::TDayChanged
#### client 回覆 OmsRcOpKind::TDayConfirm
* 當 server 收到 TDayConfirm, 且 TDay 正確, 則會回覆 OmsRcOpKind::Config
* 在 client 回覆 OmsRcOpKind::TDayConfirm 之後, 收到 OmsRcOpKind::Config 之前,
  client 不應有任何的 OMS 操作.

### Report recover/subscribe
### Request: 下單要求(新刪改查)
