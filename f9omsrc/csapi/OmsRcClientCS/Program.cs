using System;
using System.Text;

namespace OmsRcClientCS
{
   class Program
   {
      static void PrintUsage()
      {
         Console.WriteLine(@"Usage:
-n DevName
   default is TcpClient
-a DevArguments
   e.g. -a 127.0.0.1:6601
   e.g. -a dn=localhost:6601
-R RcIosvArgs

-u UserId
-p Password
-t OmsErrCode.All.cfg:zh

-f LogFlags(hex)
   default is ffff = all.
-l FmtLogFileName
   """" = default = ./logs/{0:f+'L'}/fon9cli.log
   time format: {0:?}
     L = YYYYMMDDHHMMSS
     f = YYYYMMDD
     F = YYYY-MM-DD
     K = YYYY/MM/DD
     Y = YYYY
     m = MM = Month
     d = DD = Day
     +'L' = to localtime
");
      }
      static void Main(string[] args)
      {
         Console.InputEncoding = Encoding.UTF8;
         Console.OutputEncoding = Encoding.UTF8;
         Console.WriteLine($"f9omsrcapi library version info: {f9oms.Api.VersionInfo()}");
         // ------------------------------------------------------------------
         ClientHandler cli = new ClientHandler();
         // 設定 rc client session 所需的參數.
         f9rc.ClientSessionParams rcArgs = new f9rc.ClientSessionParams();
         rcArgs.RcFlags_ = f9rc.RcFlag.NoChecksum; // 告知 rc protocol 不須使用 check sum; 需自行負擔風險.
         rcArgs.FnOnLinkEv_ = cli.OnRcLinkEv;
         rcArgs.LogFlags_ = f9rc.ClientLogFlag.All;

         // 設定 SeedVisitor(可用於查詢) 所需的參數.
         f9sv.ClientSessionParams svArgs = new f9sv.ClientSessionParams();
         svArgs.FnOnConfig_ = cli.OnSvConfig;

         // 設定 f9oms rc api 所需的參數.
         f9oms.ClientSessionParams omsRcParams = new f9oms.ClientSessionParams();
         omsRcParams.FnOnConfig_ = cli.OnOmsClientConfig;
         omsRcParams.FnOnReport_ = cli.OnOmsClientReport;
         omsRcParams.FnOnFlowControl_ = cli.OnOmsClientFcReq;

         // ------------------------------------------------------------------
         fon9.IoSessionParams ioParams = new fon9.IoSessionParams();
         string f9rcIosv = string.Empty;
         string logFileFmt = null;

         // ------------------------------------------------------------------
         for (int L = 0; L < args.Length; ++L)
         {
            string str = args[L];
            if (string.IsNullOrEmpty(str))
               continue;
            char chlead = str[0];
            if ((chlead == '-' || chlead == '/') && str.Length == 2)
            {
               if (++L >= args.Length)
               {
                  Console.WriteLine($"Lost argument value: {str}");
                  return;
               }
               string val = args[L];
               switch (str[1])
               {
                  case 'l': logFileFmt = val; continue;
                  case 'u': rcArgs.UserId_ = val; continue;
                  case 'p': rcArgs.Password_ = val; continue;
                  case 'n': rcArgs.DevName_ = val; continue;
                  case 'a': rcArgs.DevParams_ = val; continue;
                  case 'f':
                     try
                     {
                        rcArgs.LogFlags_ = (f9rc.ClientLogFlag)UInt32.Parse(val, System.Globalization.NumberStyles.HexNumber);
                     }
                     catch (SystemException)
                     {
                        Console.WriteLine($"parse LogFlag error: {str} {val}");
                        return;
                     }
                     continue;
                  case 't':
                     f9oms.Api.FreeOmsErrMsgTx(omsRcParams.ErrCodeTx_);
                     omsRcParams.ErrCodeTx_ = f9oms.Api.LoadOmsErrMsgTx1(val);
                     continue;
                  case '?':
                     PrintUsage();
                     return;

                  case 'R': f9rcIosv = val; continue;
                  case 'N': ioParams.DevName_ = val; continue;
               }
            }
            Console.WriteLine($"Unknown argument: {str}");
            PrintUsage();
            return;
         }

         if (string.IsNullOrEmpty(rcArgs.UserId_))
         {
            Console.WriteLine("Require: -u UserId");
            PrintUsage();
            return;
         }
         if (string.IsNullOrEmpty(rcArgs.DevParams_))
         {
            Console.WriteLine("Require: -a DevArguments");
            return;
         }
         if (string.IsNullOrEmpty(rcArgs.DevName_))
            rcArgs.DevName_ = "TcpClient";
         if (string.IsNullOrEmpty(rcArgs.Password_))
            rcArgs.Password_ = fon9.Api.getpass("Password: ");

         // ------------------------------------------------------------------
         // 初始化 fon9 相關函式庫.
         // 首次啟動可以設定 rc session IoManager 的 iosv 參數.
         fon9.Api.RcInitialize(logFileFmt, f9rcIosv);

         f9sv.Api.Initialize(null); // 啟動 rc session 的 sv 功能.
         fon9.Api.Finalize();       // Initialize/Finalize 必須對應, 在此取消一次 Initialize();

         f9oms.Api.Initialize(null);// 啟動 rc session 的 f9oms 功能.
         fon9.Api.Finalize();       // Initialize/Finalize 必須對應, 在此取消一次 Initialize();

         // ------------------------------------------------------------------
         // - 不論您是否需要設定額外的 svArgs 參數(e.g. svArgs.FnOnConfig_),
         //   都需要設定 rcArgs.SetFuncParams(svArgs);
         //   用來告知 ses.Open(rcArgs); 需要與 Server 端協商提供 sv 功能.
         // - 如果 Server 端無法提供 sv 功能, 則無法建立連線,
         //   此時 OnRcLinkEv 會依序收到底下的事件
         //   - Opening => Linking => LinkReady
         //     => Lingering|info=RxLogout:Unknown function code
         //     => Closed|info=RxLogout:Unknown function code
         // - 必須在 svArgs, omsRcParams 的內容都完成後, 才能呼叫 rcArgs.SetFuncParams();
         //   在呼叫 rcArgs.SetFuncParams(); 之後, 若 svArgs, omsRcParams 有變動, 則不會生效.
         rcArgs.SetFuncParams(svArgs);
         rcArgs.SetFuncParams(omsRcParams);
         // 建立 ClientSession 並開啟連線.
         cli.Open(rcArgs);

         // ------------------------------------------------------------------
         fon9.Tools.HandlerRoutine ctrlHandler = fon9.Tools.SetConsoleCtrlHandler();
         f9sv.ReportHandler svCmdHandler = new f9sv.ReportHandler();
         // 在進入主迴圈前, 先進行一次 gc, 可讓因 gc 造成的「物件失效」問題在第一時間出現.
         // 避免在運行一段不確定的時間(因 gc 啟動了) 後才 crash, 會很難尋找問題.
         GC.Collect();
         for (;;)
         {
            Console.Write("> ");
            Console.Out.Flush();
            string cmdln = Console.ReadLine();
            if (cmdln == null)
               break;
            cmdln.Trim();
            if (string.IsNullOrEmpty(cmdln))
               continue;
            if (cmdln == "quit")
               break;
            char[] splitter = { ' ', '\t' };
            string[] cmds = cmdln.Split(splitter, StringSplitOptions.RemoveEmptyEntries);
            if (cmds[0] == "wc")
            {  // wait connect. default=5 secs. 0 = 1 secs;
               Console.WriteLine("Waiting for connection...");
               Sleep(cmds.Length >= 2 ? cmds[1] : string.Empty, cli, 5);
               Console.WriteLine(cli.IsConnected ? "Connection ready." : "Wait connection timeout.");
            }
            else if (cmds[0] == "sleep")
            {
               Sleep(cmds.Length >= 2 ? cmds[1] : string.Empty, null, 1);
            }
            else if (cmds[0] == "lf")
            {
               if (cmds.Length >= 2)
               {
                  UInt32 lf;
                  if (UInt32.TryParse(cmds[1],
                     System.Globalization.NumberStyles.HexNumber,
                     System.Globalization.CultureInfo.CurrentCulture,
                     out lf))
                     cli.LogFlags = (f9rc.ClientLogFlag)lf;
               }
               Console.WriteLine($"LogFlags = {cli.LogFlags:X}\n");
               if (cmds.Length >= 3)
               {
                  fon9.Api.Initialize(cmds[2]);
                  fon9.Api.Finalize();
               }
            }
            else if (cmds[0] == "q")
            {  // query: q treePath key tabName
               svCmdHandler.FnOnReport_ = cli.OnSvQueryReport;
               if (cli.SvCommand3(f9sv.Api.Query, cmds, 1, "q:query", ref svCmdHandler))
               {
                  // 等候查詢結果, 或一小段時間(沒結果則放棄等候).
                  for (uint L = 0; L < 100; ++L)
                  {
                     if (cli.LastQueryReportedUserData_ == svCmdHandler.UserData_)
                        break;
                     System.Threading.Thread.Sleep(10);
                  }
               }
            }
            else if (cmds[0] == "s")
            {  // subscribe: s treePath key tabName
               svCmdHandler.FnOnReport_ = cli.OnSvSubscribeReport;
               cli.SvCommand3(f9sv.Api.Subscribe, cmds, 1, "s:subr", ref svCmdHandler);
            }
            else if (cmds[0] == "u")
            {  // unsubscribe: u treePath key tabName
               svCmdHandler.FnOnReport_ = cli.OnSvUnsubscribeReport;
               cli.SvCommand3(f9sv.Api.Unsubscribe, cmds, 1, "s:subr", ref svCmdHandler);
            }
            else if (cmds[0] == "gv")
            { // GridView: gv treePath key tabName  rowCount
               svCmdHandler.FnOnReport_ = cli.OnSvGridViewReport;
               cli.SvCommand3((f9rc.ClientSession ses, ref f9sv.SeedName seedName, f9sv.ReportHandler regHandler) => {
                  Int16 rowCount = 10;
                  if (cmds.Length > 4)
                     Int16.TryParse(cmds[4], out rowCount);
                  return f9sv.Api.GridView(ses, ref seedName, regHandler, rowCount);
               },
               cmds, 1, "gv:GridView", ref svCmdHandler);
            }
            else if (cmds[0] == "rm")
            { // remove: rm treePath key tabName
               svCmdHandler.FnOnReport_ = cli.OnSvRemoveReport;
               cli.SvCommand3(f9sv.Api.Remove, cmds, 1, "rm:remove", ref svCmdHandler);
            }
            else if (cmds[0] == "w")
            { // write: w treePath key tabName    fieldName=value|fieldName2=value2|...
               svCmdHandler.FnOnReport_ = cli.OnSvWriteReport;
               cli.SvCommand3((f9rc.ClientSession ses, ref f9sv.SeedName seedName, f9sv.ReportHandler regHandler) => {
                  return f9sv.Api.Write(ses, ref seedName, regHandler, cmds.Length > 4 ? cmds[4] : string.Empty);
               },
               cmds, 1, "w:write", ref svCmdHandler);
            }
            else if (cmds[0] == "run")
            { // run: run treePath key tabName    args
               svCmdHandler.FnOnReport_ = cli.OnSvCommandReport;
               cli.SvCommand3((f9rc.ClientSession ses, ref f9sv.SeedName seedName, f9sv.ReportHandler regHandler) => {
                  string cmdArgs = string.Empty;
                  if (cmds.Length > 4)
                  {
                     cmdArgs = cmds[4];
                     for (int L = 5; L < cmds.Length; ++L)
                        cmdArgs = cmdArgs + ' ' + cmds[L];
                  }
                  return f9sv.Api.Command(ses, ref seedName, regHandler, cmdArgs);
               },
               cmds, 1, "run:command", ref svCmdHandler);
            }
            else if (cmds[0] == "cfg")
               cli.PrintConfig();
            else if (cmds[0] == "set")
               cli.SetRequest(cmds, 1);
            else if (cmds[0] == "send")
               cli.SendRequest(cmds, 1);
            else if (cmds[0] == "?" || cmds[0] == "help")
               Console.WriteLine(@"Commands:
? or help
   This info.

quit
   Quit program.

wc secs
   wait connect.

sleep secs

cfg
   List configs.

lf LogFlags(hex) [LogFileFmt]
      
q treePath key tabName

set ReqId(or ReqName) FieldId(or FieldName)=value|fld2=val2|fld3=val3

send ReqId(or ReqName) times
");
            else
               Console.WriteLine($"Unknown command: {cmdln}");
         }

         // ------------------------------------------------------------------
         // 關閉連線, 並等候資源釋放完畢後返回.
         cli.Dispose();
         // 系統結束, 清理 fon9 函式庫所用到的資源.
         fon9.Api.Finalize();
         // 避免因最佳化造成 rcArgs 被回收, 使得 rcArgs.FnOnLinkEv_ 無效, 可能造成 crash.
         // 所以必須讓 rcArgs 保持存活!
         GC.KeepAlive(rcArgs);
         GC.KeepAlive(svArgs);
         GC.KeepAlive(omsRcParams);
         GC.KeepAlive(ctrlHandler);
         if (!string.IsNullOrEmpty(fon9.Tools.CtrlMessage_))
            Console.WriteLine($"Quit: {fon9.Tools.CtrlMessage_}");
         f9oms.Api.FreeOmsErrMsgTx(omsRcParams.ErrCodeTx_);
      }
      static void Sleep(string psec, ClientHandler waitcli, double secsDefault)
      {
         double secs = 0;
         double.TryParse(psec, out secs);
         if (secs <= 0)
            secs = secsDefault;
         while (waitcli == null || !waitcli.IsConnected)
         {
            if (secs >= 1)
               System.Threading.Thread.Sleep(1000);
            else
            {
               if (secs > 0)
                  System.Threading.Thread.Sleep((int)(secs * 1000));
               break;
            }
            Console.Write($"\r{--secs} ");
            Console.Out.Flush();
         }
         Console.Write("\r");
      }
   }
}
