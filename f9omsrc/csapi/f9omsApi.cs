using System;
using System.Runtime.InteropServices;

namespace f9oms
{
   using LayoutField = f9sv.Field;
   using TableIndex = UInt16;
   using FieldIndexU = UInt16;
   using FieldIndexS = Int16;
   using SNO = UInt64;

   public static class Api
   {
      // ------------------------------------------------------------------
      /// 返回 f9omsrcapi 的版本資訊.
      public static string VersionInfo()
         => System.Runtime.InteropServices.Marshal.PtrToStringAnsi(f9OmsRc_ApiVersionInfo());
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9OmsRc_ApiVersionInfo", CharSet = CharSet.Ansi)]
      static extern IntPtr f9OmsRc_ApiVersionInfo();

      // ------------------------------------------------------------------
      /// 啟動 f9OmsRc(fon9 OMS Rc client) 函式庫.
      /// - 請使用時注意: 禁止 multi thread 同時呼叫 f9OmsRc_Initialize()/fon9_Finalize();
      /// - 可重覆呼叫 f9OmsRc_Initialize(), 但須有對應的 fon9_Finalize();
      ///   最後一個 fon9_Finalize() 被呼叫時, 結束函式庫.
      /// - 其餘請參考 fon9_Initialize();
      /// - 然後透過 f9rc_CreateClientSession() 建立 Session;
      ///   - 建立 Session 時, 必須提供 f9OmsRc_ClientSessionParams; 參數.
      ///   - 且需要經過 f9OmsRc_InitClientSessionParams() 初始化.
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9OmsRc_Initialize", CharSet = CharSet.Ansi)]
      public static extern int Initialize([MarshalAs(UnmanagedType.LPStr)] string logFileFmt);

      // ------------------------------------------------------------------
      /// 載入錯誤代碼文字翻譯檔.
      /// - cfgfn = 設定檔名稱.
      /// - language = "en"; "zh"; ...
      ///   若指定的翻譯不存在, 則使用 "en"; 如果 "en" 也不存在, 則使用第1個設定.
      /// - 載入失敗則傳回 null, 透過 log 機制記錄原因.
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9omstw_LoadOmsErrMsgTx", CharSet = CharSet.Ansi)]
      public static extern IntPtr LoadOmsErrMsgTx([MarshalAs(UnmanagedType.LPStr)] string cfgfn, [MarshalAs(UnmanagedType.LPStr)] string language);

      /// cfg = "filename:en"; => LoadOmsErrMsgTx("filename", "en");
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9omstw_LoadOmsErrMsgTx1", CharSet = CharSet.Ansi)]
      public static extern IntPtr LoadOmsErrMsgTx1([MarshalAs(UnmanagedType.LPStr)] string cfg);

      /// 釋放「錯誤代碼文字翻譯」資源.
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9omstw_FreeOmsErrMsgTx")]
      public static extern void FreeOmsErrMsgTx(IntPtr txRes);

      /// 建立錯誤訊息.
      /// - 如果 ec 在 txRes 沒設定, 則不改變 outbuf, 直接返回 orgmsg;
      /// - buflen 必須包含 EOS 的空間, 如果空間不足則輸出 orgmsg.
      public static string MakeErrMsg(IntPtr txRes, OmsErrCode ec, fon9.CStrView orgmsg)
      {
         byte[] txbuf = new byte[1024];
         fon9.CStrView txmsg = f9omstw_MakeErrMsg(txRes, ref txbuf[0], (uint)txbuf.Length, ec, orgmsg);
         return txmsg.ToString();
      }
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9omstw_MakeErrMsg")]
      static extern fon9.CStrView f9omstw_MakeErrMsg(IntPtr txRes, ref byte pwbuf, uint buflen, OmsErrCode ec, fon9.CStrView orgmsg);

      // ------------------------------------------------------------------
      /// 訂閱回報, ses 與 cfg 必須是 FnOnConfig_ 事件通知時提供的 ref.
      public static unsafe void SubscribeReport(f9rc.ClientSession ses, ref ClientConfig cfg, SNO from, RptFilter filter)
         => SubscribeReport(ref *ses.RcSes_, ref cfg, from, filter);
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9OmsRc_SubscribeReport", CharSet = CharSet.Ansi)]
      public static extern void SubscribeReport(ref f9rc.RcClientSession ses, ref ClientConfig cfg, SNO from, RptFilter filter);

      /// 用指定名稱取得「下單表單格式」.
      /// - cfg 必須是在 FnOnConfig_ 事件裡面提供的那個;
      /// - 這裡取得的 layout 將在下次 FnOnConfig_ 事件之後銷毀.
      /// - 建議您在處理 FnOnConfig_ 事件時, 取得需要的 request layout.
      /// - 可自行設定 retval->IdxUserFields_[], 不可更改 retval 的其他 members;
      /// - 您也可以透過 cfg->RequestLayoutArray_[] 循序搜尋 layout.
      public static unsafe Layout* GetRequestLayout(f9rc.ClientSession ses, ref ClientConfig cfg, string reqName)
         => GetRequestLayout(ref *ses.RcSes_, ref cfg, reqName);
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9OmsRc_GetRequestLayout", CharSet = CharSet.Ansi)]
      public static unsafe extern Layout* GetRequestLayout(ref f9rc.RcClientSession ses, ref ClientConfig cfg, [MarshalAs(UnmanagedType.LPStr)] string reqName);

      /// 用 rptTableId(從1開始) 取得「回報表單格式」, 回報表單可能會有相同名稱.
      /// - cfg 必須是在 FnOnConfig_ 事件裡面提供的那個;
      /// - 這裡取得的 layout 將在下次 FnOnConfig_ 事件之後銷毀.
      /// - 可自行設定 retval->IdxUserFields_[], 不可更改 retval 的其他 members;
      public static unsafe Layout* GetReportLayout(ref f9rc.ClientSession ses, ref ClientConfig cfg, uint rptTableId)
         => GetReportLayout(ref *ses.RcSes_, ref cfg, rptTableId);
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9OmsRc_GetReportLayout", CharSet = CharSet.Ansi)]
      public static unsafe extern Layout* GetReportLayout(ref f9rc.RcClientSession ses, ref ClientConfig cfg, uint rptTableId);

      /// 傳送下單要求: 已組好的字串形式.
      /// \retval 1=true  下單要求已送出.
      /// \retval 0=false 無法下單: 沒有呼叫過 f9OmsRc_Initialize();
      ///                 或建立 ses 時, 沒有提供 f9OmsRc_ClientSessionParams 參數.
      public static int SendRequestString(ref f9rc.RcClientSession ses, ref Layout reqLayout, string reqStr)
         => SendRequestString(ref ses, ref reqLayout, System.Text.Encoding.UTF8.GetBytes(reqStr));
      public static int SendRequestString(ref f9rc.RcClientSession ses, ref Layout reqLayout, byte[] reqBytes)
         => SendRequestString(ref ses, ref reqLayout, new fon9.CStrView(reqBytes));
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9OmsRc_SendRequestString", CharSet = CharSet.Ansi)]
      public static unsafe extern int SendRequestString(ref f9rc.RcClientSession ses, ref Layout reqLayout, fon9.CStrView reqStr);
      public static unsafe int SendRequestString(f9rc.ClientSession ses, ref Layout reqLayout, string reqStr)
         => SendRequestString(ref *ses.RcSes_, ref reqLayout, reqStr);
      public static unsafe int SendRequestString(f9rc.ClientSession ses, ref Layout reqLayout, byte[] reqBytes)
         => SendRequestString(ref *ses.RcSes_, ref reqLayout, reqBytes);
      public static unsafe int SendRequestString(f9rc.ClientSession ses, ref Layout reqLayout, fon9.CStrView reqStr)
         => SendRequestString(ref *ses.RcSes_, ref reqLayout, reqStr);

      // C# 不提供此功能, 請自行組裝好傳送字串後呼叫: SendRequestString();
      // /// 傳送下單要求: 已填妥下單欄位字串陣列: reqFieldArray[0 .. reqLayout->FieldCount_);
      // /// \retval 1=true  下單要求已送出.
      // /// \retval 0=false 無法下單: 沒有呼叫過 f9OmsRc_Initialize();
      // ///                 或建立 ses 時, 沒有提供 f9OmsRc_ClientSessionParams 參數.
      // [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9OmsRc_SendRequestFields", CharSet = CharSet.Ansi)]
      // unsafe static extern int f9OmsRc_SendRequestFields(ref f9rc.RcClientSession ses, ref Layout reqLayout, fon9.CStrView* reqFieldArray);

      /// 傳送一批下單要求.
      /// 若遇到流量管制, 則已打包的會先送出, 然後用 f9OmsRc_ClientSessionParams.FnOnFlowControl_ 機制處理.
      /// \retval 1=true  下單要求已送出.
      /// \retval 0=false 無法下單: 沒有呼叫過 f9OmsRc_Initialize();
      ///                 或建立 ses 時, 沒有提供 f9OmsRc_ClientSessionParams 參數.
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9OmsRc_SendRequestBatch")]
      public static unsafe extern int SendRequestBatch(ref f9rc.RcClientSession ses, RequestBatch[] reqBatch, uint reqCount);
      public static unsafe int SendRequestBatch(f9rc.ClientSession ses, RequestBatch[] reqBatch)
         => SendRequestBatch(ref *ses.RcSes_, reqBatch, (uint)reqBatch.Length);
      /// 取得 SendRequestBatch() 最大打包資料量.
      /// 因 TCP/Ethernet 封包的特性, 若資料量超過 MSS, 則在網路線上傳送時, 需要拆包送出,
      /// 接收端需要等候併包後才能處理, 反而造成更大的延遲, 所以系統供一個最大打包資料量.
      /// 一旦批次下單打包資料量超過此值, 就先送出, 然後繼續打包後續的下單要求.
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9OmsRc_GetRequestBatchMSS")]
      public static extern uint GetRequestBatchMSS();
      /// 設定新的 SendRequestBatch() 最大打包資料量.
      /// 返回設定前的值.
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9OmsRc_SetRequestBatchMSS")]
      public static extern uint SetRequestBatchMSS(uint value);

      /// 傳送下單要求之前, 可以自行檢查流量.
      /// 傳回需要等候的 microseconds. 傳回 0 表示不需管制.
      public static unsafe uint CheckFcRequest(f9rc.ClientSession ses)
         => CheckFcRequest(ref *ses.RcSes_);
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9OmsRc_CheckFcRequest")]
      public static extern uint CheckFcRequest(ref f9rc.RcClientSession ses);
   }

   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   public struct RequestBatch
   {
      public unsafe Layout* Layout_;
      /// 在 ReqStr_ 的使用期間, 參照的記憶體必須保持有效.
      public fon9.CStrView ReqStr_;
   }

   [Flags]
   public enum RptFilter : byte
   {
      RptAll = 0,
      /// 回補階段, 回補最後狀態, 例如:
      /// - 若某筆要求已收到交易所回報, 則中間過程的 Queuing, Sending 不回補.
      /// - 若某筆要求正在 Sending, 則中間過程的 Queuing 不回補.
      RecoverLastSt = 0x01,
      /// 僅回補 WorkingOrder, 即時回報不考慮此旗標.
      RecoverWorkingOrder = 0x02,

      /// Queuing 不回報(也不回補).
      NoQueuing = 0x10,
      /// Sending 不回報(也不回補).
      NoSending = 0x20,
      /// 僅回補(回報)成交;
      MatchOnly = 0x40,
      /// 排除外部回報, 僅本地下單要求的回報.
      /// 成交回報: 僅回報新單為本地所下的成交.
      NoExternal = 0x80,
   }

   public enum OmsLayoutKind : byte
   {
      Request = 0,
      ReportRequest = 1,
      ReportRequestIni = 2,
      ReportRequestAbandon = 3,
      ReportOrder = 4,
      ReportEvent = 5,
   }

   /// 下單格式、回報格式.
   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   public struct Layout {
      public fon9.CStrView LayoutName_;

   /// 如果是委託回報格式, 則會額外提供是哪種回報造成的委託異動.
   /// 例如: "abandon", "event", "TwsNew", "TwsChg", "TwsFil";
      public fon9.CStrView ExParam_;

      public unsafe LayoutField* FieldArray_;

      public FieldIndexU FieldCount_;

      /// 從 1 開始計算, ReqTableId 或 RptTableId.
      public TableIndex LayoutId_;

      /// 如果是 "REQ" 的下單設定, 則此處為 OmsLayoutKind.Request;
      /// 如果是 "RPT" 的回報設定, 則根據設定內容.
      public OmsLayoutKind LayoutKind_;
      byte Reserved___;

      /// FieldArray_[IdxKind_] = 下單要求的種類: f9fmkt_RxKind 新刪改查、委託、系統事件...;
      public FieldIndexS IdxKind_;
      /// FieldArray_[IdxMarket_] = 下單要求的市場別: f9fmkt_TradingMarket;
      public FieldIndexS IdxMarket_;
      /// FieldArray_[IdxSessionId_] = 交易時段: f9fmkt_TradingSessionId;
      public FieldIndexS IdxSessionId_;
      /// FieldArray_[IdxBrkId_] = 勸商代號;
      public FieldIndexS IdxBrkId_;
      /// FieldArray_[IdxOrdNo_] = 委託書號;
      /// 對於一筆委託書: 市場唯一編號 = OrdKey = Market + SessionId + BrkId + OrdNo;
      public FieldIndexS IdxOrdNo_;

      public FieldIndexS IdxIvacNo_;
      public FieldIndexS IdxSubacNo_;
      public FieldIndexS IdxIvacNoFlag_;
      public FieldIndexS IdxUsrDef_;
      public FieldIndexS IdxClOrdId_;
      public FieldIndexS IdxSide_;
      public FieldIndexS IdxOType_;
      public FieldIndexS IdxSymbol_;
      public FieldIndexS IdxPriType_;
      public FieldIndexS IdxPri_;
      public FieldIndexS IdxQty_;
      public FieldIndexS IdxPosEff_;
      public FieldIndexS IdxTIF_;
      public FieldIndexS IdxPri2_;

      // 底下的欄位為回報專用.
      public FieldIndexS IdxReqUID_;
      public FieldIndexS IdxCrTime_;
      public FieldIndexS IdxUpdateTime_;
      public FieldIndexS IdxOrdSt_;
      public FieldIndexS IdxReqSt_;
      public FieldIndexS IdxUserId_;
      public FieldIndexS IdxFromIp_;
      public FieldIndexS IdxSrc_;
      public FieldIndexS IdxSesName_;
      public FieldIndexS IdxIniQty_;
      public FieldIndexS IdxIniOType_;
      public FieldIndexS IdxIniTIF_;

      public FieldIndexS IdxChgQty_;
      public FieldIndexS IdxErrCode_;
      public FieldIndexS IdxMessage_;
      public FieldIndexS IdxBeforeQty_;
      public FieldIndexS IdxAfterQty_;
      public FieldIndexS IdxLeavesQty_;
      public FieldIndexS IdxLastExgTime_;

      public FieldIndexS IdxLastFilledTime_;
      public FieldIndexS IdxCumQty_;
      public FieldIndexS IdxCumAmt_;
      public FieldIndexS IdxMatchQty_;
      public FieldIndexS IdxMatchPri_;
      public FieldIndexS IdxMatchKey_;
      public FieldIndexS IdxMatchTime_;

      // 第1腳成交.
      public FieldIndexS IdxCumQty1_;
      public FieldIndexS IdxCumAmt1_;
      public FieldIndexS IdxMatchQty1_;
      public FieldIndexS IdxMatchPri1_;
      // 第2腳成交.
      public FieldIndexS IdxCumQty2_;
      public FieldIndexS IdxCumAmt2_;
      public FieldIndexS IdxMatchQty2_;
      public FieldIndexS IdxMatchPri2_;

      /// 券商提供的特殊欄位.
      /// 可在收到 FnOnConfig_ 事件時透過:
      /// `GetRequestLayout();`
      /// `GetReportLayout();`
      /// 取得 layout 之後設定這裡的值.
      public unsafe fixed FieldIndexS IdxUserFields_[16];
   }

   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   public struct ClientReport
   {
      public SNO ReportSNO_;
      public SNO ReferenceSNO_;

      /// 回報事件若此值為 null, 則表示回補完畢通知, 之後開始接收即時回報.
      public unsafe Layout* Layout_;

      /// 不能保留這裡的指標, 回報通知完畢後, 字串會被銷毀.
      /// 欄位數量可從 Layout_->FieldCount_ 得到.
      public unsafe fon9.CStrView* FieldArray_;
   }

   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   public struct CoreTDay
   {
      public UInt32 HostId_;
      public UInt32 YYYYMMDD_;
      /// 相同 TDay 的啟動次數.
      public UInt32 UpdatedCount_;

      bool Equals(CoreTDay r)
      {
         return this.HostId_ == r.HostId_
            && this.YYYYMMDD_ == r.YYYYMMDD_
            && this.UpdatedCount_ == r.UpdatedCount_;
      }
   }

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   public struct ClientConfig {
      public CoreTDay CoreTDay_;

      /// 下單要求的表單數量.
      public TableIndex RequestLayoutCount_;
      UInt16 Padding1__;

      /// 下單要求的表單指標陣列.
      public unsafe Layout** RequestLayoutArray_;

      /// OmsApi 登入成功後, 收到的相關權限資料.
      public f9sv.GvTables RightsTables_;

      /// 下單流量管制.
      public UInt16 FcReqCount_;
      public UInt16 FcReqMS_;
      UInt32 Padding4__;

      /// 可用櫃號列表.
      public fon9.CStrView OrdTeams_;
   }

   public delegate void FnOnConfig(ref f9rc.RcClientSession ses, ref ClientConfig cfg);
   public delegate void FnOnReport(ref f9rc.RcClientSession ses, ref ClientReport rpt);
   public delegate void FnOnFlowControl(ref f9rc.RcClientSession ses, uint usWait);

   /// OmsRc API 客戶端用戶的事件處理程序及必要參數.
   [StructLayout(LayoutKind.Sequential)]
   public class ClientSessionParams : f9rc.FunctionNoteParams
   {
      public ClientSessionParams() : base(f9rc.FunctionCode.OmsApi)
      {
      }

      /// ErrCode 訊息翻譯服務.
      /// 載入翻譯檔: LoadOmsErrMsgTx(); 或 LoadOmsErrMsgTx1();
      /// 釋放翻譯檔: FreeOmsErrMsgTx();
      public IntPtr ErrCodeTx_;

      /// 通知時機:
      /// - 當登入成功後, Server 回覆現在交易日, 使用者設定.
      /// - 當 Server 切換到新的交易日.
      /// - 您應該在收到此事件時「回補 & 訂閱」回報.
      public FnOnConfig FnOnConfig_;

      /// 當收到回報時通知.
      ///
      /// \code
      /// // ref f9oms.ClientReport rpt;
      /// f9oms.Layout* layout = rpt.Layout_;
      /// \endcode
      ///
      /// 取得特定欄位的回報名稱及內容(以「IdxKind_:下單要求的種類」為例):
      /// \code
      /// if (layout->IdxKind_ >= 0) {
      ///   layout->FieldArray_[layout->IdxKind_]; // 欄位名稱及描述.
      ///   rpt.FieldArray_[layout->IdxKind_];     // 欄位回報內容.
      /// }
      /// \endcode
      ///
      /// 依序取得欄位的回報名稱及內容:
      /// \code
      /// for (unsigned L = 0; L < layout->FieldCount_; ++L) {
      ///   layout->FieldArray_[L]; // 欄位名稱及描述.
      ///   rpt.FieldArray_[L];     // 欄位回報內容.
      /// }
      /// \endcode
      public FnOnReport FnOnReport_;

      /// 當下單遇到流量管制時通知.
      /// 如果這裡為 null, 則由 API 「等候流量解除」然後「送單」, 之後才返回.
      /// 您也可以在送單前自主呼叫 f9oms.CheckFcRequest() 取得流量管制需要等候的時間.
      public FnOnFlowControl FnOnFlowControl_;
   }
}

