f9utw 台灣期權交易 / Startup
============================

## 初次啟動
### 啟動前準備事項
* 建立 fon9cfg/fon9common.cfg
```
$LogFileFmt=./logs/{0:f+'L'}/fon9sys-{1:04}.log
$MemLock = Y
```

* 建立 fon9cfg/fon9local.cfg
```
$HostId = 33
```

* 將 OmsErrCodeAct.cfg 複製到 fon9cfg/.
  根據交易所回覆的錯誤碼, 執行指定的工作, 例如:「40010 : St=ExchangeNoLeavesQty」

* 建立 API 表格, 從 `~/devel/f9omstw/f9omsrc/forms` 複製, 或建立連結.
  * `ln -s ~/devel/f9omstw/f9omsrc/forms forms`

### 期貨交易所資料
* 預設存放路徑: 執行路徑下的相對路徑 ./tmpsave
* 設定位置: `gv /omstw/TwfExgConfig/TwfExgImporter`
* P06: 期貨商代號BrkId - 編號FcmId 對照
  * 請建立連結: `ln -s ./tmpsave/P06.20  ./tmpsave/P06`
  * 或修改設定, 更改載入的檔名:
    `ss,FileName=./tmpsave/P06.20   /omstw/TwfExgConfig/TwfExgImporter/0_P06`
* P07: 連線設定的 dn 設定, 需移除尾端的期貨商代號: P07.10, P07.11, P07.20, P07.21
  * 或建立連結: `ln -s ./tmpsave/P07.10.F906  ./tmpsave/P07.10`
* P08: 短Id商品基本資料, 取得價格小數位, 商品序號
* PA8: 長Id商品基本資料, 取得價格小數位, 商品序號
* P09: 契約基本資料

### 使用「管理模式」啟動程式
* `./f9utw --admin`
初次啟動執行底下指令, 載入必要的模組, 底下的設定都是立即生效:
```
# 建立必要的 Device(TcpServer,TcpClient,FileIO) factory, 放在 DeviceFactoryPark=FpDevice
ss,Enabled=Y,EntryName=TcpServer,Args='Name=TcpServer|AddTo=FpDevice' /MaPlugins/DevTcps
ss,Enabled=Y,EntryName=TcpClient,Args='Name=TcpClient|AddTo=FpDevice' /MaPlugins/DevTcpc
ss,Enabled=Y,EntryName=FileIO,Args='Name=FileIO|AddTo=FpDevice'       /MaPlugins/DevFileIO

# 建立 io 管理員, DeviceFactoryPark=FpDevice; SessionFactoryPark=FpSession; 設定儲存在 fon9cfg/MaIo.f9gv
ss,Enabled=Y,EntryName=NamedIoManager,Args='Name=MaIo|DevFp=FpDevice|SesFp=FpSession|Cfg=MaIo.f9gv|SvcCfg="ThreadCount=2|Capacity=100"' /MaPlugins/MaIo

# 啟動 OMS, 設定支援的期貨商 (CmId=結算會員編號, 如果 BrkCount!=1 全部期貨商的結算會員都相同).
# 可使用 IoFut="期貨日盤線路IO服務參數"|IoOpt="選擇權日盤線路IO服務參數"|
#        IoFutA="期貨夜盤線路IO服務參數"|IoOptA="選擇權夜盤線路IO服務參數"
# 參數範例: "ThreadCount=1|Cpus=12|Capacity=20"
ss,Enabled=Y,EntryName=UtwOmsCore,Args='BrkId=F906999|BrkCount=1|CmId=610'  /MaPlugins/iOmsCore

# 啟動 Rc下單協定
ss,Enabled=Y,EntryName=RcSessionServer,Args='Name=OmsRcSvr|Desp=f9OmsRc Server|AuthMgr=MaAuth|AddTo=FpSession'                      /MaPlugins/iOmsRc
ss,Enabled=Y,EntryName=OmsRcServerAgent,Args='OmsCore=omstw|Cfg=$TxLang={zh} $include:forms/ApiTwfAll.cfg|AddTo=FpSession/OmsRcSvr' /MaPlugins/iOmsRcAgent
ss,Enabled=Y,EntryName=RcSvServerAgent,Args='AddTo=FpSession/OmsRcSvr'                                                              /MaPlugins/iRcSvAgent
```

### 設定測試及管理員帳號(fonwin)、密碼、權限
```
ss,RoleId=admin,Flags=0 /MaAuth/UserSha256/fonwin
/MaAuth/UserSha256/fonwin repw Password(You can use a sentence.)

# 設定管理權限.
ss,HomePath=/   /MaAuth/PoAcl/admin
ss,Rights=xff   /MaAuth/PoAcl/admin/'/'
ss,Rights=xff   /MaAuth/PoAcl/admin/'/..'

# 設定交易權限: 可用櫃號=A, 不限流量.
ss,OrdTeams=A   /MaAuth/OmsPoUserRights/admin

# 設定交易帳號: 管理員, 任意帳號, AllowAddReport(x10)
ss              /MaAuth/OmsPoIvList/admin
ss,Rights=x10   /MaAuth/OmsPoIvList/admin/*
```

### 建立 Rc下單 Server
```
# ----- 設定 -----
ss,Enabled=Y,Session=OmsRcSvr,Device=TcpServer,DeviceArgs=6601 /MaIo/^edit:Config/iOmsRcSv

# 查看設定及套用時的 SubmitId
gv /MaIo/^apply:Config

# 套用設定, 假設上一行指令的結果提示 SubmitId=1
/MaIo/^apply:Config submit 1
# ----- MaIo 套用後生效 -----
# ---------------------------
```

### 建立 OMS 的台灣期交所連線
底下以「期貨日盤:UtwFutNormal_io」為例,「期貨夜盤:UtwFutAfterHour_io、選擇權日盤:UtwOptNormal_io、選擇權夜盤:UtwOptAfterHour_io」類推.
若連線目的地不是從 P07 取得, 可自行增加 DeviceArgs='ReuseAddr=Y|dn=XSimIp:port'
```
# 設定台灣期交所連線, 實際連線的 dn 會參考 P07 及此處的 SessionId 來決定
ss,Enabled=Y,Session=TWF,SessionArgs='FcmId=610|SessionId=412|Pass=9999|ApCode=4|IsUseSymNum=N',Device=TcpClient,DeviceArgs='ReuseAddr=Y' /omstw/TwfLineMgr/UtwFutNormal_io/^edit:Config/L412

# 查看設定及套用時的 SubmitId
gv /omstw/TwfLineMgr/UtwFutNormal_io/^apply:Config

# 套用設定, 假設上一行指令的結果提示 SubmitId=2
/omstw/TwfLineMgr/UtwFutNormal_io/^apply:Config submit 2
```
