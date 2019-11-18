fon9 Order management system for tw
===================================

[benchmark](https://github.com/fonwin/f9omstw.benchmark)

libf9omstw: 台灣環境的委託管理系統.
* 採用 static library:
  * 執行時的速度可加快一點點。
  * 僅需提供單一執行檔(減少額外的 dll), 可讓執行環境更單純。
* TODO:
  * OmsBackend
    * 各類事件(開收盤、斷線...)通知.
    * 重新載入後, 重算風控資料.
    * 回報訂閱(回補) Filter: 排除「非本機收單」的回報.
* 一些特殊狀況的思考:
  * 委託書號對照表異常
    * 相同委託書號對應到不同的委託書?
    * 發生原因: 外部回報異常(例如: 送出昨天的回報檔), OMS 沒有清檔...
    * 如何決定哪筆委託才是正確的?

## 預設路徑結構
```
~/devel/                我的開發環境根目錄
  |-- external/         third-party libraries
  |
  |-- output/fon9/
  |-- output/f9omstw/
  |   |-- 64/Release/      預設 Windows 的 fon9.sln 輸出路徑
  |   \-- release/f9omstw/ 預設 Linux 輸出路徑
  |
  |-- fon9/             libfon9 基礎建設
  |
  \-- f9omstw/          適用於台灣市場的 OMS, 為了便於開發, 底下所有的 namespace 都使用 f9omstw
      |-- f9omstw/      header(.hpp & .h) & source(.cpp & .c) 放在一起，更容易找到所需的檔案。
      |-- f9omstws/     適用於台灣證券市場 OMS 擴充, 例如: OmsTwsRequest.cpp
      |-- f9omstwf/     適用於台灣期貨市場 OMS 擴充, 例如: OmsTwfRequest.cpp
      |-- f9omsrc/      使用 fon9 rc protocol 的「client 端 C API」及「server 端: RcServerAgent, RcServerNote」
      |   \-- forms/    API 表單 Layout 設定範例.
      |
      |-- f9utw/        台「證券、期貨」市場 OMS 範例程式.
      |
      \-- build/
          |-- cmake/    build shell for cmake
          \-- vs2015/   VS2015 project & solution files
```

---------------------------------------

## 回報匯入(包含下單回報)
依序回報是理想的世界, 但現實是殘酷的,
所以「回報匯入」亂序, 是 OMS 必須解決的難題.
請參閱專章說明: [回報匯入機制](ReportIn.md)

## 根據錯誤碼決定後續行為
請參閱: [錯誤碼設定](OmsErrCode/OmsErrCodeAct.cfg)

---------------------------------------

## 下單要求、委託書內容
### 如何決定需要那些欄位
* 需要使用設定檔設定欄位嗎? 或直接使用 struct 配合 fon9_MakeField()?
  * 使用「動態設定欄位」:
    * 在每次使用欄位時, 都要判斷欄位是否有設定.
    * 雖然可在初始化階段, 就先判斷必要欄位是否存在, 降低使用時的額外判斷.
    * 每次使用時需要透過額外間接的方式取得欄位內容, 例如使用 fon9::seed::Field
    * 雖然可增加彈性, 但程式會變得非常繁瑣.
  * 可使用底下的程式碼達到類似「動態欄位」的效果, 但 dynamic_cast<> 需要付出一些成本, 請斟酌使用.
    ```c++
    struct Req { // 基底類別需要有 virtual function.
       virtual ~Req() {
       }
    };
    struct ReqEx { // 要檢查的類別可以沒有 virtual function.
       int Ex_;
    };
    struct ReqTwsNew : public Req, public ReqEx {
    };
    
    bool CheckReqEx(const Req* req) {
       if (const ReqEx* p = dynamic_cast<const ReqEx*>(req)) {
          // ReqEx 存在, 可直接取用 p->Ex_;
          return true;
       }
       return false;
    }
    ```
* OrdNo 綜合底下原因, OrdNo 會在 OmsRequestBase, OmsOrderRaw 裡面都有提供.
  * Client 如果有 IsAllowAnyOrdNo 權限:
    * Client 可自訂新單的委託號(例: OrdNo=A1234), 系統需檢查是否重複.
    * Client 可自訂新單的委託櫃號(例: OrdNo=B1), 然後由後續的步驟編序號.
  * OrdNo 在一個台灣券商交易系統內, 是有限資源,
    可能讓某系統專用1碼或2碼櫃號, 因此可用序號範圍: (62^4=14,776,336 或 62^3=238,328).
  * 所以券商可能要求: 送單前才編製委託書號.

### OmsRequestBase
* BrkId, OrdNo 放在 OmsRequestBase; 由 OmsRequestBase 提供市場唯一Key,
  如此大部分的要求, 就可以有一致的處理方法.
  * 新單: 如果有 IsAllowAnyOrdNo 權限, 可讓使用者可自訂櫃號或委託書號.
  * 刪改查、成交: 可填入市場唯一序號(OrdKey), 用來尋找委託.
* Market, SessionId 應可透過下單內容(商品、數量、價格)自動判斷.
* RequestKind 若為改單, 可透過下單欄位(數量、價格、TIF)自動判斷.
* ReqUID 由系統自行編製.

### OmsRequestIni
* 當需要提供全部欄位時(例:新單要求、回報匯入), 使用此基底.
* 也可用於「提供全部欄位」的「刪改查」, 但此時委託必須有 Initiator();
  執行下單步驟前, 必須檢查基本欄位(IvacNo,Symbol,Side...)是否正確.

---------------------------------------

## Backend
* 負責:
  * 依序(RxSNO)記錄「回報(所有歷史資料及事件)」包含:
    * OmsRequestBase: 進入 OmsCore 的 Request
    * OmsOrderRaw: Order 異動
    * OmsCoreEvent: 各類與 OmsCore 有關的事件
  * 回報訂閱
    * 只會在 Backend thread 裡面通知.
    * 回補回報
      * using RxRecover = std::function<OmsRxSNO(OmsCore&, const OmsRxItem* item)>;
      * ReportRecover(OmsRxSNO fromSNO, RxRecover&& consumer);
      * 可在收到回補結束訊息時訂閱 ReportSubject.
    * 增加訂閱、取消訂閱
      * ReportSubject_.Subscribe()、ReportSubject_.Unubscribe()
      * core.ReportSubject().Subscribe()、core.ReportSubject().Unubscribe()
* RxSNO
  * 每個 OMS 自行獨立編號(各編各的號, 也就是OmsA.RxSNO=1, 與OmsB.RxSNO=1, 可能是不同的 OmsRxItem).
  * 從 1 開始, 依序編號.
* log format
  * 底下的 '|' = '\x1'; 每行的尾端使用 '\n' 結束.

  * 首碼=英文字母=FactoryName, 此行表示 OmsRxItemFactory 欄位列表(包含欄位型別及名稱), 例如:
    * 下單要求欄位列表: `ReqName|Fields(TypeId Name)\n`
    * 委託異動欄位列表: `OrdName|Fields(TypeId Name)\n`
    * 事件名稱欄位列表: `EvtName|Fields(TypeId Name)\n`

  * 首碼=數字=RxSNO, 此行表示 OmsRxItem(OmsRequest, OmsOrderRaw, OmsEvent...)
    * 不成案的下單要求:
      * `RxSNO|ReqName|Fields|E:OmsErrCode:AbandonReason\n`
    * 新單(初成案)要求
      * `RxSNO|ReqName|Fields\n`
    * 一般要求(刪、改、查、成交...)
      * `RxSNO|ReqName|Fields(包含有效的 IniSNO)\n`
      * IniSNO = Initiator request RxSNO.
    * 委託異動
      * `RxSNO|OrdName|Fields|FromSNO\n`
      * FromSNO = From request RxSNO.

  * 額外資訊, 沒有首碼, 第一碼就是 '|' = '\x1'
    ```
    |exsz|............ (exsz-2) any bytes ............\n
         \_ 包含開頭的 '|' 及尾端的 '\n' 共 exsz bytes _/
    ```
    any bytes: 可包含任意 binary 資料, 須注意: 其中可能含有 '\n'、'\x1' 之類的控制字元。

---------------------------------------

## Report output(subscribe)

由 Backend 提供回報「回補/訂閱」的服務.

* Report Recover
* Report Subscribe:
  * (Request/Order/Event) Report
  * Market event Report
    * Preopen/Open/Close
    * Line broken

---------------------------------------

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
* OrdNoMap 僅適用於「由本地端自編委託序號」的交易情境, 例如: 台灣證券、台灣期權。
  * Brk-Market-Session-OrdNo(fon9::Trie)
  * 目前台灣券商環境 OmsOrdNo 長度是 5
  * 若交易其他市場(例如:國外期貨), OrdNo 通常由對方編製,
    此時不應使用 OrdNo, 應使用其他欄位處理, 例如: ExgOrdId;
    * 此時不要使用 OrdNoMap, 應考慮使用 std::unordered_map;
    * OrdNoMap 適用於台灣環境: 由 OMS 端編製「櫃號+序號」的使用情境。
    * 櫃號為券商資源, 分配給券商端個交易系統, 各自獨立編製序號。
* Request/Order

---------------------------------------

## Thread safety
因為 OMS 牽涉到的資料表很多, 如果每個表有自己的 mutex, 是不切實際的作法。
目前可以考慮的方式有:
* 共用一個 mutex
* 共用一個 fon9::AQueue
* OmsCoreByThread: 所有工作集中到單一 thread

---------------------------------------

## 每日清檔作業
* 當 TDay 開始時, 先建立當日的 OmsCore, 備妥後加入 OmsCoreMgr;
  * 加入前, 匯入: 商品、投資人基本資料、庫存...
* OmsCoreMgr 觸發新交易日開始事件.
  * 由於每個 OmsCore 各自擁有: 商品資料、投資人基本資料、庫存...
  * 所以收到新交易日開始事件時, 應重新取得相關資料.
* 舊的 OmsCore 則在適當時機刪除.
* 這樣就沒有 OmsCore 每日清檔的問題!

---------------------------------------

## 下單流程
### 可用櫃號
* 使用者權限:
  * 新單任意委託書號權限, 完全信任對方提供的委託書號(或櫃號), 除非委託書號重複。
    * 在 user thread 如果是新單, 則會先檢查: 若有自訂櫃號, 則必須有 IsAllowAnyOrdNo 權限.
  * 可用櫃號列表, 依序使用, 不讓使用者填選。
  * 不提供可用櫃號, 由系統決定委託櫃號。
* 若使用者沒提供可用櫃號, 則可能由下列設定提供「可用櫃號列表」
  * 來源別。
  * 線路群組。
  * 特定下單步驟。

### user thread 收單
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

### 進入 OmsCore 執行下單步驟
* 可將 OmsCore 視為一個 MPSC(多發行者+單一消費者) Queue.
* 此階段在 OmsCore 保護下執行。
* 來到此處之後, 一律透過「回報機制」告知結果。
* 在安全的取得 OmsResource 之後(可能在 OmsCore thread, 或透過 locker), 才能進行底下步驟。

#### 下單(交易)要求
1. 前置作業: req->BeforeReqInCore();
   * 此時仍允許修改 req 內容.
   * 若有失敗則 req 進入 Abandon 狀態: 此時透過 OmsBackend 回報機制, 回報給 user(session)
   * 先將下單要求編號: OmsResource.FetchRequestId(req);
   * 聯繫 OmsOrder;
     * 新單要求: 建立新委託
       * BeforeReq_CheckOrdKey():  檢查 OrdKey 是否正確, 若沒設定, 則嘗試填入正確值.
       * BeforeReq_CheckIvRight(): 檢查是否為可用帳號? 透過 OmsRequestPolicy;
       * 建立新的 OmsOrder: 設定 OmsOrder.ScResource_;
       * 分配委託書號, 此動作也可能在其他地方處理, 例如: 送出前 or 風控成功後 or...
         分配好之後, 應立即加入委託書對照表.
       * 結束 req 的異動, 進入委託異動狀態.
     * 改單(或查單)要求: 取得舊委託;
       * BeforeReq_GetInitiator()
         * 如果有提供 IniSNO: 透過 IniSNO 找回委託, 如果有提供 OrdKey(BrkId-Market-SessionId-OrdNo), 則檢查是否正確.
         * 如果沒提供 IniSNO: 透過 OrdKey 找回委託, 然後填入 req.IniSNO_;
       * 取得舊委託後, 檢查是否為可用帳號? 透過 inireq->LastUpdated()->Order().Initiator()->BeforeReq_CheckIvRight(reqr, res);
       * 結束 req 的異動, 建立新的 OmsOrderRaw, 進入委託異動狀態.

2. 風控檢查.
   * 若有失敗則 req 進入 Reject 狀態.

3. 排隊、送出、或 NoReadyLine.
   * 若有失敗則 req 進入 Reject 狀態.

#### 異動結束
* 將 OmsRequest 或 委託異動資料(OmsOrderRaw) 丟到 OmsBackend.
* 定時(例:1 ms), 或有空時通知, 或資料累積太多: 通知有新增的 OmsRxItem.
  * OmsBackend 將新增的 OmsRxItem 寫入檔案, 及回報通知.
