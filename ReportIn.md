f9oms 回報匯入機制
=================

## 亂序:回報匯入機制的困難

### 亂序的起因
* 回報來源可能有多個管道: 下單線路, 其他 f9oms 的回報, 券商主機的回報...
* 可能會從不同來源收到同一筆回報.
* 如果某一管道的回報稍慢、瞬斷...
* 很多原因都可能造成回報亂序, 雖然發生機率可能不高, 但確實會發生.

### 亂序的機率
* 發生機率與「網路配置、機器效率、線路品質...」相關.
* 一般情況, 也許數天一次;
* 環境複雜或配置不良, 也許一天數次;
* 環境單純或配置良好, 也許永遠遇不到;

### 處理亂序的困難
* 如何判斷回報是否亂序?
* 亂序的回報要如何處理? 暫時保留? 立即處理? 拋棄?
  * 哪些必須保留? 例如: 刪減回報 Bf=3,Af=0, 成功 -3;
    * 回報前 LeavesQty=10, 回報後 LeavesQty=7; (或此時根本沒有此筆委託資料:遺漏新單回報?)
    * 此時有 7 不明: 其他主機減量? 在途成交?
    * 此時應將此刪單回報保留, 等遺失的 7 補足後, 再處理「Bf=3,Af=0的刪減回報」.
  * 哪些必須立即處理? 例如, 已收到新單回報之後:
    * 減量回報 Bf=10,Af=9, 成功 -1;
    * 成交沒有 Bf,Af, 所以只要 MatchKey 沒重複, 可直接處理.
  * 哪些是過時的回報(stale)? 例如:
    * 09:01 改價 P1, 沒收到回報;
      09:02 改價 P2 成功, Order 已改成 P2;
      09:03 收到 P1 的改價回報, 此回報應拋棄(但須標註P1改價要求已完成).
    * 09:01 查單, 沒收到回報;
      09:02 刪單成功;
      09:03 收到查單回報剩餘3, 此回報應拋棄(但須標註查單要求已完成, 或用訊息顯示 "HH:MM:SS $Pri*Qty(stale)").
  * 「重複的回報」不應異動委託, 應拋棄.
    * 但仍需在 log 裡面使用 ExInfo 格式留下紀錄.
* 因亂序而暫時保留的回報, 如何記錄? 如何在重啟後能繼續保留?
  * 如果一直沒收到亂序前的回報, 要怎麼辦?
* 亂序保留的資料如何送給回報訂閱者? 重新調整順序後? 還是要立即回報呢?

---------------------------------

## 回報來源
* f9oms多主機互傳回報.
* 線路調撥後收到的回報.
  * 應在設定時告知此線路原本是否為 f9oms 使用.
  * 如果原本是 f9oms 使用, 且 f9oms有互傳回報: 則可用 OrdKey 找到委託, 再循序尋找 ReqUID=ClOrdID 的 Req.
  * 如果原本非 f9oms 使用, 或 f9oms沒互傳回報: 則使用 OrdKey + AfterQty 尋找是否為重複回報.
* 券商主機回報.
  * 需排除「f9oms => 券商主機 => f9oms」這種回報.
  * 如果無法判斷過濾, 則 f9oms多主機「不可以」互傳回報.
  * 思考底下的階段, 如果有亂序, 要怎麼處理呢?
    * f9oms.X.Request(SNO=A), Queuing(SNO=B), Sending(SNO=C).
    * 「SNO=A 的券商回報(SNO=D) 結果(SNO=E)」一般回報只有最後結果.
    * f9oms.X.Result(SNO=F).
    * 如果 f9oms.Y 沒收到 A、B、C, 但先收到了「券商主機送來的 D & E」, 那 f9oms.Y 要怎麼處理呢?
      * 如果將 D&E 當作一般回報匯入, 那之後收到 A、B、C 時, 怎麼對應到 D&E?
  * 所以:
    * 如果券商主機的回報格式, 可以辨別下單要求來源是否為 f9oms, 則可將其過濾, 只處理「非 f9oms」的回報.
    * 如果無法辨別, 則 f9oms 之間「不可以」互傳回報.

### 台灣證券(台灣證交所、台灣櫃買中心)
* 為證交所有可能接受委託, 但刪減數量, 且成交可能從其他管道回報
  * 狀態碼=31/QUANTITY WAS CUT/外資買進或借券賣委託數量被刪減(或IOC委託部分成功，其餘數量被刪減)
  * 狀態碼=定價:7031,零股:2032/QUANTITY WAS CUT/外資買進或借券賣委託數量被刪減
  * 狀態碼=51/QUANTITY WAS CUT/委託觸及價格穩定措施上、下限價格，市價、IOC委託部分成功，其餘數量被刪減.
* 因收到新單回報前, 不能確定新單成功數量:
  * 所以若「刪、改、查、成交」比新單回報早到, 應先保留回報.
  * 等收到新單回報後再處理.

### 台灣期權
* 只有 IOC/FOK 會有期交所刪單
  * 有「新增併成交」的回報.
  * 在成交回報「之後」(或如果沒有任何成交), 期交所會透過一筆「成交=0」來處理「刪除剩餘量」.
* 因期交所若有部分刪除, 會在最後回報「刪除剩餘量」
  * 所以只要能確定期交所有收到新單(例: 先收到成交, 刪改成功...),
    * 就可以先設定新單 ExchangeAccepted.
    * 但如果是 Bid/Offer 報價單, 期交所會有 2 次回報, 要如何因應呢?
  * 所以台灣期權, 沒有「等新單回報」的需求.

---------------------------------

## f9oms 回報流程

* 底下用「Rpt」統稱「回報物件」.
* 收到回報時, 先建立 Rpt, 然後丟到 OmsCore 進行回報流程.
  * 證券新單回報 Rpt 必須是 OmsTwsRequestIni 衍生類(其他市場類推), 因為:
    後續的操作(刪改查), 會用 `static_cast<const OmsTwsRequestIni*>(Order.Initiator_);` 取得必要欄位.
* 一般而言, 極少部分會亂序, 因此亂序時不要求速度, 但求正確, 避免爭議造成客訴.

### 一般情況
絕大部分是這種情況, 所以這裡的處理必須追求速度.
* 下單線路(或其他 f9oms)的依序回報
  * 用 Rpt 找到當初的下單要求, 更新委託內容, 然後刪除 Rpt.
  * 只有這種情況, 不用記錄 Rpt.
  * 其餘的 Rpt 都會: 編號(RxSNO)存入 RxHistory, 並記錄 log.
* 外部依序回報: 使用 Rpt 更新委託內容.

### 有些即使亂序也必須立即處理
* 先決條件: 已有該筆委託, 且如果是證券則必須已收到交易所新單回報.
* 成交回報: 因為成交回報通常沒有 BeforeQty、AfterQty, 無法判斷中間是否有遺漏.
* 改量後 AfterQty!=0
  * 如果 AfterQty!=0 則可能因亂序關係, 永遠無法滿足 BeforeQty, 因此不能等候, 必須直接處理, 例如:
    * 正常 N:10 => F:1 => F:2 => C:-3(Bf:7,Af:4) => F:4
    * 亂序 N:10 => F:1 => F:4 => F:2 => C:-3(Bf:7,Af:4) 這種順序 LeavesQty 從沒變成 7;
  * 如果 AfterQty==0 則應為 UserCanceled, 此時應等遺漏的「在途成交 or 其他改量」, 等滿足 BeforeQty 之後再處理.
* OrderSt 的因應之道
  * 正常: 新單10:Accepted => 成交1:PartFilled => 減量6(Bf=9,Af=3):PartFilled => 成交3:FullFilled
  * 亂序: 新單10:Accepted => 成交1:PartFilled => 成交3:PartFilled => 減量6(Bf=9,Af=3):AsCanceled

### 過時的回報(stale)
* 多次改價, 只需保留最後一次的結果, 例如:
  * 改價要求順序 P1 => P2 => P3, 但收到回報的順序 P3 => P2 => P1, 則 P2, P1 為過時的回報.
* 查單後刪改(或成交), 查單結果比刪改結果晚收到, 則查單結果為過時的回報.
  * 正常 N:10 => Q => F:1 => L:9 => F:2 => C:-3(Bf:7,Af:4)
  * 亂序 N:10 => Q => C:-3(Bf:7,Af:4) => L:9:此為過時的回報
  * 亂序 N:10 => Q => F:1 => F:2 => L:9:此為過時的回報
* 過時的回報, 不會改變 LastOrderSt, 但仍會有一筆委託異動設定 RequestSt, 標記該筆下單要求的結果.
* 可能的誤判:
  * 依序收到「改價P1」、「改價P2」, 透過2條線路送出;
  * 依序收到回報「改價P1成功」、「改價P2成功」且交易所 Timestamp 相同.
  * 交易所最後是接受哪一個價格呢? 依照本機收到的順序不一定是正確的.
  * 目前無解, 除非交易所提供「交易所端的異動計數」並隨著回報返回.
  * 如果真的發生, 且想知道最後的價格及剩餘量, 可透過查詢得知.

### 必須先保留的亂序回報
* 新單不明:
  * (委託不存在的「刪、改、查、成交」回報) 或 (證券且尚未收到交易所新單回報).
* 無法結單:
  * 刪改結果 AfterQty==0, 但刪減數量不足以使 Order.LeavesQty==0,
    此時應等遺漏的「在途成交 or 其他改量」, 等滿足 BeforeQty 時再處理: OrderSt_Canceled.
  * 刪改失敗(剩餘量為0), 但此時 Order.LeavesQty != 0,
    此時應等遺漏的「在途成交 or 其他改量」, 等 Order.LeavesQty==0 時再處理: RequestSt_ExchangeRejected.

### 保留、繼續、程式重啟
#### 重複或不正確的 Rpt
* 若 Rpt 為重複回報: 如果沒有 f9oms 的 ReqUID, 則使用「OrdKey + 交易所的 BeforeQty、AfterQty」
* 若 Rpt 與新單的基本欄位不符, 例: IvacNo,Symbol,Side...
* 使用 ExInfo 方式寫入 log
* 然後刪除 Rpt.
* 不會記錄到 RxHistory, 也不會送給回報訂閱者

#### 若 Rpt 為 f9oms Req 的回報
* 若 Rpt 可直接進行回報更新(包含過時的回報)
  * 直接使用 Req 異動 OrderRaw
  * 刪除 Rpt
* 若 Rpt 必須先保留
  * 直接使用 Req 異動 OrderRaw
    * OrderSt = f9fmkt_OrderSt_ReportPending
    * 設定 BeforeQty, AfterQty, LastExgTime, Pri, PriType, LastPriTime, ErrCode...
    * 不改變 LeavesQty
    * 刪除 Rpt
  * 等後續回報造成 LeavesQty == BeforeQty 時, 再異動一次 OrderRaw.
    * RequestSt: ExchangeRejected、ExchangeNoLeavesQty、ExchangeAccepted...
    * 重設 BeforeQty, AfterQty, LastExgTime, ErrCode...
    * 計算 LeavesQty

#### 若 Rpt 非 f9oms Req 的回報
* Rpt 加到 RxHistory 的尾端
  * 依照一般流程寫入 log.
  * 依照一般流程送給回報訂閱者.
* 如果委託不存在
  * 建立一個「暫時的特殊 Order」保留 Rpt
  * 等收到新單回報時, 再依底下方法重新處理此處保留的 Rpt.
  * 有可能到程式結束前(或 TDayChanged 之前), 都沒有收到新單回報.
    * 但因為一開始就已送給回報訂閱者, 所以訂閱者還是會收到.
    * 也有可能在收到新單時, 基本欄位不符(例: IvacNo), 則這裡的 Rpt 就不會有任何後續了.
* 如果委託有存在
  * 若 Rpt 可直接進行回報更新(包含過時的回報)
    * 直接使用 Req 異動 OrderRaw
  * 若 Rpt 必須保留
    * 使用 Rpt 異動 OrderRaw
      * OrderSt = f9fmkt_OrderSt_ReportPending
      * 設定 BeforeQty, AfterQty, LastExgTime, ErrCode...
      * 不改變 LeavesQty
    * 等後續回報造成 LeavesQty == BeforeQty 時, 再異動一次 OrderRaw.
      * RequestSt: ExchangeRejected、ExchangeNoLeavesQty、ExchangeAccepted...
      * 重設 BeforeQty, AfterQty, LastExgTime, ErrCode...
      * 計算 LeavesQty
