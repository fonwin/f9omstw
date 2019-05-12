fon9 Order management system for tw
===================================

* TODO:
  * OmsBackend
    * OmsReporter
      * Client: OmsReportSubscriber / OmsReportRecover
    * OmsRecorder / OmsLoader
    * 各類事件(開收盤、斷線...)通知, 也可使用 Request Report 機制處理.

libf9omstw: 台灣環境的委託管理系統.
* 採用 static library:
  * 執行時的速度可加快一點點。
  * 僅需提供單一執行檔(減少額外的 dll), 可讓執行環境更單純。

## 下單要求、委託書內容
* 需要使用設定檔設定欄位嗎? 或直接使用 struct 配合 fon9_MakeField()?
  * 使用「動態設定欄位」:
    * 在每次使用欄位時, 都要判斷欄位是否有設定.
    * 可在初始化階段就先判斷必要欄位是否存在, 降低使用時的額外判斷.
    * 每次使用時需要透過額外間接的方式取得欄位內容, 例如使用 fon9::seed::Field
    * 雖然可增加彈性, 但程式會變得非常繁雜.
  * 可使用底下的程式碼達到類似「動態欄位」的效果, 但 dynamic_cast<> 需要付出一些成本, 請斟酌使用.
```c++
struct Req {
   virtual ~Req() {
   }
};
struct ReqEx {
   int Ex_;
};
struct ReqTwsNew : public Req, public ReqEx {
};

bool CheckReqEx(const Req* req) {
   if (const ReqEx* p = dynamic_cast<const ReqEx*>(req)) {
      // 直接取用 p->Ex_;
      return true;
   }
   return false;
}
```

* 一些特殊狀況的思考:
  * 回報順序問題:
    * 沒有新單資料, 先收到其他回報(刪、改、成交)。
    * 新單尚未成功, 先收到其他回報(刪、改、成交):   
      由於台灣證交所可能會主動減少新單的委託量, 所以新單尚未成功前, 不能確定新單成功數量。
    * 刪單成功回報, 但刪減數量無法讓 LeavesQty 為 0, 可能有「在途成交」或「遺漏減量回報」。
    * 可考慮在 OmsOrder 增加一個容器儲存「等待處理的回報」。
  * 委託書號對照表異常
    * 相同委託書號對應到不同的委託書?
    * 發生原因: 外部回報異常(例如: 送出昨天的回報檔), OMS 沒有清檔...
    * 如何決定哪筆委託才是正確的?

### Request/Order Recorder
* format
  * 底下的 '|' = '\x1'; 每一行的尾端使用 '\n' 結束.
  * 下單要求欄位列表, 首碼='R'
    * `R|ReqName|ReqFields(TypeId Name)\n`
  * 委託異動欄位列表, 首碼='O'
    * `O|OrdName|OrdFields(TypeId Name)\n`
  * 不成案的下單要求, 首碼='e'
    * `e|ReqSNO|ReqName|ReqFields|RejectReason\n`
    * 如果此時 ReqSNO==0, 則拋棄此下單要求, 僅記錄在 log, 回報不補送.
    * 如果此時 ReqSNO!=0, 則將此下單要求加入 RequestMgr, 回報可回補.
  * 新單(初成案)要求, 首碼=數字, 且 ReqFields 裡面 **不含** 有效的 IniSNO(Initiator ReqSNO).
    * `ReqSNO|ReqName|ReqFields(不含有效的 IniSNO)\n`
  * 一般要求(刪、改、查、成交...), 首碼=數字, 且 ReqFields 裡面 **包含** 有效的 IniSNO(Initiator ReqSNO).
    * `ReqSNO|ReqName|ReqFields(包含有效的 IniSNO)\n`
  * 委託異動, 首碼='o'
    * `o|ReqSNO|OrdName|OrdFields\n`
  * 額外資訊, 沒有首碼, 第一碼就是 '|' = '\x1'
    ```
    |exsz\n|............ (exsz-2) any bytes ............\n
           \_ 包含開頭的 '|' 及尾端的 '\n' 共 exsz bytes _/
    ```
    any bytes: 可包含任意 binary 資料, 須注意: 其中可能含有 '\n'、'\x1' 之類的控制字元。

## 風控資料表
底下是「f9omstw:台灣環境OMS」所需的資料表, 不同的OMS實作、不同的風控需求, 所需的表格、及表格元素所需的欄位不盡相同。
* Symb
  * 1張的股數, 警示, 平盤下可券賣...
  * SymbRef 今日參考價
* Brk
  * 券商代號: AA-BB 或 AAA-B; 共4碼文數字, BB 或 B 為分公司代號。
  * 期貨商代號: X999BBB; 後3碼為數字序號, 代表分公司碼。
  * 表格使用 std::vector<OmsBrkSP>
  * Brk-Ivac-Subac
* Ivac
  * IvacNo: 999999C 依序編號(6碼序號, C為1數字檢查碼), 但可能會用前置碼表示特殊身份, 例如: 9700000
  * enum IvacNC: 999999 移除檢查碼後的序號。
  * 帳號有分段連續的特性, 因此表格使用 OmsIvacMap = fon9::LevelArray<IvacNC, OmsIvacSP, 3>;
  * 帳號商品表: 庫存、委託累計、成交累計; 用於風控檢查。
* Subac
  * SubacId = 字串。
  * 表格使用 OmsSubacMap = fon9::SortedVectorSet<OmsSubacSP, OmsSubacSP_Comper>;
* OrdNo
  * Brk-Market-Session-OrdNo(fon9::Trie)
* Request/Order
  * RequestId = 依序編號, 所以使用 std::deque

## Thread safety
因為 OMS 牽涉到的資料表很多, 如果每個表有自己的 mutex, 是不切實際的作法。
目前可以考慮的方式有:
* 共用一個 mutex
* 共用一個 fon9::AQueue
* 所有工作集中到單一 thread

## 每日清檔作業
* 方法:
  * 每日結束然後重啟?
  * 設計 DailyClear 程序?
    * 清除 OrdNoMap
    * 帳號資料:
      * 移除後重新匯入? 如果有註冊「帳號回報」, 是否可以不要重新註冊?
      * 匯入時處理 IsDailyClear?
      * 先呼叫全部帳號的 OnDailyClear(); ?

## Events
* Report Recover
* Order Report
* Market event
  * Preopen/Open/Close
  * Line broken

## 下單流程
### user thread
當 user(session) 收到下單訊息時, 建立 req, 填入 req 內容。
此階段仍允許修改 req 內容, 還在 user thread, 尚未進入 OmsCore.

1. 下單要求填妥後檢查:
   * 檢查欄位是否正確
   * 沒填的欄位是否可以由程式補填 (例: RequestChg 根據 Qty 決定 RequestKind: 刪 or 改...)
2. 檢查 OmsRequestPolicy 權限: RequestKind = 新、刪、改、查...
   => 可用帳號在進入 OmsCore 之後檢查, 因為帳號資料表是「受保護」的資料。
3. 若有失敗則 req 進入 Abandon 狀態,
   由 user thread 直接處理失敗, 不進入 OmsCore.
4. req 確定後, 交給 OmsCore 執行下單步驟。

### 進入 OmsCore 執行下單步驟.
* 可將 OmsCore 視為一個 MPSC(多發行者+單一消費者) Queue.
* 此階段在 OmsCore 保護下執行。
* 來到此處之後, 一律透過「回報機制」告知結果。
* 在安全的取得 OmsResource 之後(可能在 OmsCore thread, 或透過 locker), 才能進行底下步驟。

#### 成交
  * 透過 OrdKey 找回委託, 然後填入 req.IniSNO_;
  * 檢查 MatchKey 是否重複.
  * 直接處理成交回報作業, 然後結束異動.

#### 下單(交易)要求
1. 前置作業
   * 此時仍允許修改 req 內容.
   * 若有失敗則 req 進入 Abandon 狀態:
     * req.ReqSNO_ = 0; 不加入 RequestMgr;
     * 透過 ReportMgr: 「寫入 log」及「回報通知」, 但 Abandon 的 req 無法回補.
   * 聯繫 OmsOrder.
     * 新單要求: 建立新委託
       * 檢查是否為可用帳號? 透過 OmsRequestPolicy;
       * 建立新的 OmsOrder: 設定 OmsOrder.ScResource_;
       * 分配委託書號, 此動作也可能在其他地方處理, 例如: 送出前 or 風控成功後 or...
         分配好之後, 應立即加入委託書對照表.
     * 改單(或查單)要求: 取得舊委託;
       * 如果有提供 IniSNO: 透過 IniSNO 找回委託, 如果有提供 OrdKey(BrkId-Market-SessionId-OrdNo), 則檢查是否正確.
       * 如果沒提供 IniSNO: 透過 OrdKey 找回委託, 然後填入 req.IniSNO_;
       * 取得舊委託後, 檢查是否為可用帳號? 透過 OmsOrder.ScResource_.Ivr_ 及 OmsRequestPolicy;
   * 結束 req 的異動, 建立新的 OmsOrderRaw, 進入委託異動狀態.

2. 風控檢查.
   - 若有失敗則 req 進入 Reject 狀態.

3. 排隊、送出、或 NoReadyLine.
   - 若有失敗則 req 進入 Reject 狀態.

#### 異動結束
* 將 OmsRequest 及 委託異動資料(OmsOrderRaw) 丟到 OmsReportMgr.
  * OmsReportMgr 是一個 SPSC(單一發行者(來自OmsCore)+單一消費者) Queue.
* 透過 OmsRecorder 寫 log.
* 回報通知.

