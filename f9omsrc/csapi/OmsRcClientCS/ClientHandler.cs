using System;
using System.Text;

namespace OmsRcClientCS
{
   class ClientHandler : f9rc.ClientSession
   {
      UInt64 LastSNO_;
      f9oms.CoreTDay CoreTDay_;
      unsafe f9oms.ClientConfig* Config_;
      public IntPtr LastQueryReportedUserData_;

      class RequestRec
      {
         string RequestStr_;
         byte[] RequestBytes_;
         fon9.CStrView RequestPacket_;
         public string[] Fields_;
         public string RequestStr
         {
            get { return this.RequestStr_; }
            set
            {
               // 在設定 RequestStr 時, 就先將要送出的字串打包好.
               // 等到要送出時, 就不用再 Encoding.
               this.RequestStr_ = value;
               this.RequestBytes_ = System.Text.Encoding.UTF8.GetBytes(value);
               unsafe
               {
                  fixed (byte* p = this.RequestBytes_)
                  {
                     this.RequestPacket_.Begin_ = new IntPtr(p);
                     this.RequestPacket_.End_ = new IntPtr(p + this.RequestBytes_.Length);
                  }
               }
            }
         }
         public fon9.CStrView RequestPacket
         {
            get { return this.RequestPacket_; }
         }
      }
      RequestRec[] RequestRecs_;

      // ---------------------------------------------------------------------
      public bool IsConnected { get { unsafe { return this.Config_ != null; } } }

      // ---------------------------------------------------------------------
      static void PrintEvSplit(string evName)
      {
         Console.Write($"========== {evName}: ");
      }

      // ---------------------------------------------------------------------
      internal unsafe void OnRcLinkEv(ref f9rc.RcClientSession ses, f9io.State st, fon9.CStrView info)
      {
         Console.WriteLine($"OnRcLinkEv|st={st}|info={info}");
         this.Config_ = null;
      }

      // ---------------------------------------------------------------------
      static string SvResultCodeStr(f9sv.ResultCode res)
      {
         return $"{(int)res}:{res}:{f9sv.Api.GetSvResultMessage(res)}";
      }
      static unsafe void PrintSvField(f9sv.Field* fld, ref f9sv.ClientReport rpt)
      {
         byte[] buf = new byte[1024];
         uint sz = (uint)buf.Length;
         f9sv.Api.GetField_Str(rpt.Seed_, ref *fld, ref buf[0], ref sz);
         string val = Encoding.UTF8.GetString(buf, 0, (int)sz - 1/*EOS*/);
         Console.Write($"{fld->Named_.Name_}=[{val}]");
         {  // 直接取出欄位的數值.
            if (fld->TypeId_[0] == 'U')
            {
               UInt64 uval = f9sv.Api.ReadUnsigned(rpt.Seed_, ref *fld);
               if (uval != 0 && uval == fld->NullValue_.Unsigned_)
                  Console.Write("()");
               else
                  Console.Write($"[{uval}]");
            }
            else if (fld->TypeId_[0] == 'S')
            {
               Int64 sval = f9sv.Api.ReadSigned(rpt.Seed_, ref *fld);
               if (sval != 0 && sval == fld->NullValue_.Signed_)
                  Console.Write("()");
               else
                  Console.Write($"[{sval}]");
            }
         }
      }
      static unsafe void PrintSeedValues(ref f9sv.ClientReport rpt)
      {
         if (rpt.Seed_ == IntPtr.Zero)
         {
            if (rpt.ResultCode_ == f9sv.ResultCode.NoError)
            {  // 「訂閱/查詢/取消訂閱」成功通知.
               // Console.WriteLine("layout=");
               // PrintSvLayout(ref rpt);
            }
            else
            { // 「訂閱/查詢/取消訂閱」失敗通知.
            }
            return;
         }
         uint fldidx = rpt.Tab_->FieldCount_;
         if (fldidx <= 0)
            return;
         f9sv.Field* fld = rpt.Tab_->FieldArray_;
         for (;;)
         {
            PrintSvField(fld, ref rpt);
            if (--fldidx <= 0)
               break;
            Console.Write('|');
            ++fld;
         }
         Console.WriteLine();
      }
      static unsafe void PrintSvReportResult(string evName, ref f9sv.ClientReport rpt)
      {
         PrintEvSplit(evName);
         Console.WriteLine($"UserData={rpt.UserData_}, result={SvResultCodeStr(rpt.ResultCode_)}");
         Console.WriteLine($"treePath=[{rpt.TreePath_}], seedKey=[{rpt.SeedKey_}], tab=[{rpt.Tab_->Named_.Name_}|{rpt.Tab_->Named_.Index_}]");
         if (rpt.ExResult_.Length > 0)
            Console.WriteLine($"exResult=[{rpt.ExResult_}]");
      }
      static unsafe void PrintSvReportSeed(string evName, ref f9sv.ClientReport rpt)
      {
         PrintSvReportResult(evName, ref rpt);
         PrintSeedValues(ref rpt);
         Console.WriteLine("====================");
      }

      internal void OnSvConfig(ref f9rc.RcClientSession ses, ref f9sv.ClientConfig cfg)
      {
         Console.WriteLine($"OnSvConfig|cfg.FcQry={cfg.FcQryCount_}/{cfg.FcQryMS_}|cfg.MaxSubr={cfg.MaxSubrCount_}");
         Console.WriteLine($"cfg.OrigStrView={cfg.RightsTables_.OrigStrView_}");
      }
      internal void OnSvQueryReport(ref f9rc.RcClientSession ses, ref f9sv.ClientReport rpt)
      {
         PrintSvReportSeed("OnSv.QueryReport", ref rpt);
         this.LastQueryReportedUserData_ = rpt.UserData_;
      }
      internal void OnSvSubscribeReport(ref f9rc.RcClientSession ses, ref f9sv.ClientReport rpt)
      {
         if (rpt.Seed_ == IntPtr.Zero)
         {  // 訂閱結果通知.
            if (rpt.ResultCode_ == f9sv.ResultCode.NoError)
            {
               // 訂閱成功.
            }
            else
            {
               // 訂閱失敗.
            }
         }
         PrintSvReportSeed("OnSv.SubscribeReport", ref rpt);
      }
      internal void OnSvUnsubscribeReport(ref f9rc.RcClientSession ses, ref f9sv.ClientReport rpt)
      {
         PrintSvReportSeed("OnSv.UnsubscribeReport", ref rpt);
      }
      internal void OnSvGridViewReport(ref f9rc.RcClientSession ses, ref f9sv.ClientReport rpt)
      {
         PrintSvReportResult("OnSv.GridViewReport", ref rpt);
         if (rpt.ResultCode_ == f9sv.ResultCode.NoError)
         {
            f9sv.ClientReport nrpt = rpt;
            for (;;)
            {
               nrpt.SeedKey_ = f9sv.Api.GridView_Parser(ref nrpt, ref nrpt.ExResult_);
               if (nrpt.SeedKey_.Begin_ == IntPtr.Zero)
                  break;
               Console.Write($"[{nrpt.SeedKey_}]|");
               PrintSeedValues(ref nrpt);
            }
            Console.WriteLine($"{nrpt.GridViewResultCount_}/{(Int64)nrpt.GridViewTableSize_} [{(Int64)nrpt.GridViewDistanceBegin_}:{(Int64)nrpt.GridViewDistanceEnd_}]");
         }
         Console.WriteLine("====================");
      }
      internal void OnSvRemoveReport(ref f9rc.RcClientSession ses, ref f9sv.ClientReport rpt)
      {
         PrintSvReportSeed("OnSv.RemoveReport", ref rpt);
      }
      internal void OnSvWriteReport(ref f9rc.RcClientSession ses, ref f9sv.ClientReport rpt)
      {
         PrintSvReportSeed("OnSv.WriteReport", ref rpt);
      }
      internal void OnSvCommandReport(ref f9rc.RcClientSession ses, ref f9sv.ClientReport rpt)
      {
         PrintSvReportSeed("OnSv.CommandReport", ref rpt);
      }

      public delegate f9sv.ResultCode FnSvCmd3(f9rc.ClientSession ses, ref f9sv.SeedName seedName, f9sv.ReportHandler regHandler);
      public bool SvCommand3(FnSvCmd3 fnSvCmd, string[] cmds, uint cmdidx, string prompt, ref f9sv.ReportHandler handler)
      {
         handler.UserData_ = IntPtr.Add(handler.UserData_, 1);
         if (cmdidx >= cmds.Length)
         {
            Console.WriteLine($"{prompt}: require 'treePath'");
            return false;
         }
         f9sv.SeedName seedName = new f9sv.SeedName();
         seedName.TreePath_ = cmds[cmdidx++];
         if (cmdidx >= cmds.Length)
         {
            Console.WriteLine($"{prompt}: require 'key'");
            return false;
         }
         seedName.SeedKey_ = cmds[cmdidx++];
         if (seedName.SeedKey_ == "\\t")
            seedName.SeedKey_ = "\t";
         else if (seedName.SeedKey_ == "\\b")
            seedName.SeedKey_ = "\b";
         else if (seedName.SeedKey_ == "''" || seedName.SeedKey_ == "\"\"")
         {
            seedName.SeedKey_ = string.Empty;
         }
         if (cmdidx < cmds.Length)
         {
            seedName.TabName_ = cmds[cmdidx];
            if (string.IsNullOrEmpty(seedName.TabName_) || Char.IsDigit(seedName.TabName_[0]))
            {
               uint.TryParse(seedName.TabName_, out seedName.TabIndex_);
               seedName.TabName_ = null;
            }
         }
         f9sv.ResultCode res = fnSvCmd(this, ref seedName, handler);
         Console.WriteLine($"---------- {prompt}: UserData={handler.UserData_}, return={SvResultCodeStr(res)}");
         Console.WriteLine($"treePath=[{seedName.TreePath_}], seedKey=[{seedName.SeedKey_}], tab=[{seedName.TabName_}]");
         Console.WriteLine("--------------------");
         return true;
      }

      // ---------------------------------------------------------------------
      internal unsafe void OnOmsClientConfig(ref f9rc.RcClientSession ses, ref f9oms.ClientConfig cfg)
      {
         fixed (void* pcfg = &cfg)
         {
            this.Config_ = (f9oms.ClientConfig*)pcfg;
         }
         if (!this.CoreTDay_.Equals(cfg.CoreTDay_))
         {
            this.LastSNO_ = 0;
            this.CoreTDay_ = cfg.CoreTDay_;
         }
         this.RequestRecs_ = new RequestRec[cfg.RequestLayoutCount_];
         for (uint iReqLayout = 0; iReqLayout < cfg.RequestLayoutCount_; ++iReqLayout)
         {
            this.RequestRecs_[iReqLayout] = new RequestRec();
            this.RequestRecs_[iReqLayout].Fields_ = new string[cfg.RequestLayoutArray_[iReqLayout]->FieldCount_];
         }
         f9oms.Api.SubscribeReport(ref ses, ref cfg, this.LastSNO_ + 1, f9oms.RptFilter.RptAll);
      }
      internal unsafe void OnOmsClientReport(ref f9rc.RcClientSession ses, ref f9oms.ClientReport rpt)
      {
         if (rpt.Layout_ != null)
         {
            if (rpt.ReportSNO_ == 0)
            {
               // [下單要求]尚未進入 OmsCore 之前就被拒絕, OmsCore log 不會記錄此筆訊息.
               return;
            }
         }
         else
         {
            // rpt.Layout_ == null // 回補結束.
         }
         this.LastSNO_ = rpt.ReportSNO_;

         if (rpt.Layout_ != null && (this.LogFlags & f9rc.ClientLogFlag.AllOmsRc) == f9rc.ClientLogFlag.AllOmsRc)
         {
            StringBuilder msg = new StringBuilder();
            msg.Append(rpt.Layout_->LayoutId_);
            msg.Append(':');
            msg.Append(rpt.Layout_->LayoutName_);
            msg.Append('(');
            msg.Append((uint)rpt.Layout_->LayoutKind_);
            msg.Append(':');
            msg.Append(rpt.Layout_->LayoutKind_);
            msg.Append('|');
            msg.Append(rpt.Layout_->ExParam_);
            msg.Append(')');
            for (uint L = 0; L < rpt.Layout_->FieldCount_; ++L)
            {
               msg.Append('|');
               msg.Append(rpt.Layout_->FieldArray_[L].Named_.Name_);
               msg.Append('=');
               msg.Append(rpt.FieldArray_[L].ToString());
            }
            PrintEvSplit("OnOmsClientReport");
            Console.WriteLine(msg);
         }
      }
      internal void OnOmsClientFcReq(ref f9rc.RcClientSession ses, uint usWait)
      {
         // 也可不提供此 function: f9oms.ClientSessionParams.FnOnFlowControl_ = null;
         // 則由 API 判斷超過流量時: 等候解除流量管制 => 送出下單要求 => 返回下單要求呼叫端.
         PrintEvSplit("OnClientFcReq");
         Console.WriteLine($"wait={usWait} us");
         System.Threading.Thread.Sleep((int)(usWait + 999) / 1000);
      }

      // ---------------------------------------------------------------------
      static unsafe void PrintRequest(f9oms.Layout* pReqLayout, RequestRec req)
      {
         Console.WriteLine($"[{pReqLayout->LayoutId_}] {pReqLayout->LayoutName_}");
         StringBuilder typeId = new StringBuilder();
         for (uint iFld = 0; iFld < pReqLayout->FieldCount_; ++iFld)
         {
            f9sv.Field* fld = &pReqLayout->FieldArray_[iFld];
            typeId.Clear();
            byte* p = fld->TypeId_;
            while (*p != 0)
               typeId.Append((char)*p++);
            Console.WriteLine($"\t[{iFld,2}] {typeId,-6} {fld->Named_.Name_,-10} = '{req.Fields_[iFld]}'");
         }
      }
      internal unsafe void PrintConfig()
      {
         if (this.Config_ == null)
         {
            Console.WriteLine("Config not ready.");
            return;
         }
         // TODO: 直接在此輸出並不安全, 要先 lock this, 然後才能安全的取得並使用 this.Config_.
         Console.WriteLine($"HostId = {this.Config_->CoreTDay_.HostId_}");
         Console.WriteLine($"TDay   = {this.Config_->CoreTDay_.YYYYMMDD_}/{this.Config_->CoreTDay_.UpdatedCount_}");
         Console.WriteLine($"Tables =\n{this.Config_->RightsTables_.OrigStrView_}");
         for (uint iReqLayout = 0; iReqLayout < this.Config_->RequestLayoutCount_; ++iReqLayout)
            PrintRequest(this.Config_->RequestLayoutArray_[iReqLayout], this.RequestRecs_[iReqLayout]);
      }

      static uint UIntTryParse(string str, ref int ifrom)
      {
         uint retval = 0;
         while (Char.IsDigit(str, ifrom))
         {
            retval = retval * 10 + (uint)(str[ifrom++] - '0');
         }
         return retval;
      }
      static unsafe int GetFieldIndex(string tvstr, ref int ifrom, f9oms.Layout* pReqLayout)
      {
         UInt32 retval;
         if (Char.IsDigit(tvstr, ifrom))
         {
            retval = UIntTryParse(tvstr, ref ifrom);
            if (retval >= pReqLayout->FieldCount_)
            {
               Console.WriteLine($"Unknwon field index = {retval}");
               return -1;
            }
         }
         else
         {
            int ispl = tvstr.IndexOf('=', ifrom);
            string fldName = (ifrom <= ispl ? tvstr.Substring(ifrom, ispl - ifrom) : string.Empty);
            for (retval = 0; retval < pReqLayout->FieldCount_; ++retval)
            {
               if (fldName == pReqLayout->FieldArray_[retval].Named_.Name_.ToString())
                  goto __FOUND_FIELD;
            }
            Console.WriteLine($"Field not found: {fldName}");
            return -1;
            __FOUND_FIELD:;
            ifrom = ispl;
         }
         if (tvstr[ifrom] != '=')
         {
            Console.WriteLine("Loss '=' for field.");
            return -1;
         }
         ++ifrom;
         return (int)retval;
      }
      static unsafe void MakeRequestStr(f9oms.Layout* pReqLayout, RequestRec req)
      {
         StringBuilder res = new StringBuilder();
         for (uint iFld = 0; iFld < pReqLayout->FieldCount_; ++iFld)
         {
            if (iFld != 0)
               res.Append('\x01');
            res.Append(req.Fields_[iFld]);
         }
         req.RequestStr = res.ToString();
      }

      unsafe f9oms.Layout* GetRequestLayout(string[] cmds, uint cmdidx)
      {
         if (this.Config_ == null)
         {
            Console.WriteLine("Config not ready.");
            return null;
         }
         if (cmdidx >= cmds.Length)
         {
            Console.WriteLine("Require: ReqName");
            return null;
         }
         // Req(Id or Name)
         string reqName = cmds[cmdidx];
         if (!string.IsNullOrEmpty(reqName))
         {
            if (Char.IsDigit(reqName, 0))
            {
               UInt32 id = 0;
               UInt32.TryParse(reqName, out id);
               if (id <= 0 || this.Config_->RequestLayoutCount_ < id)
               {
                  Console.WriteLine($"Unknown request id = {reqName}");
                  return null;
               }
               return this.Config_->RequestLayoutArray_[id - 1];
            }
            f9oms.Layout* pReqLayout = f9oms.Api.GetRequestLayout(this, ref *this.Config_, reqName);
            if (pReqLayout != null)
               return pReqLayout;
         }
         Console.WriteLine($"Unknown request name = {reqName}");
         return null;
      }
      internal unsafe void SetRequest(string[] cmds, uint cmdidx)
      {  // ReqName tag=value...
         f9oms.Layout* pReqLayout = GetRequestLayout(cmds, cmdidx++);
         if (pReqLayout == null)
            return;
         RequestRec req = this.RequestRecs_[pReqLayout->LayoutId_ - 1];
         if (cmdidx < cmds.Length)
         {
            string tvstr = cmds[cmdidx];
            int iend;
            for (int ifrom = 0; 0 <= ifrom && ifrom < tvstr.Length;)
            {
               int iFld = GetFieldIndex(tvstr, ref ifrom, pReqLayout);
               if (iFld < 0)
                  break;
               string val;
               char chbr = (ifrom < tvstr.Length ? tvstr[ifrom] : '\0');
               switch (chbr)
               {
                  case '\'':
                  case '"':
                     iend = tvstr.IndexOf(chbr, ++ifrom);
                     if (iend < ifrom)
                     {
                        Console.WriteLine($"Cannot find matching [{chbr}].\n");
                        goto __BREAK_PUT_FIELDS;
                     }
                     val = tvstr.Substring(ifrom, iend - ifrom);
                     ifrom = tvstr.IndexOf('|', iend + 1);
                     break;
                  default:
                     iend = tvstr.IndexOf('|', ifrom);
                     if (iend < ifrom)
                     {
                        val = tvstr.Substring(ifrom);
                        ifrom = -1;
                     }
                     else
                     {
                        val = tvstr.Substring(ifrom, iend - ifrom);
                        ifrom = iend + 1;
                     }
                     break;
               }
               req.Fields_[iFld] = val;
            }
            __BREAK_PUT_FIELDS:;
            MakeRequestStr(pReqLayout, req);
         }
         PrintRequest(pReqLayout, req);
         Console.WriteLine($"RequestStr = [{req.RequestStr}]");
      }
      internal unsafe void SendRequest(string[] cmds, uint cmdidx)
      {  // ReqName times
         f9oms.Layout* pReqLayout = GetRequestLayout(cmds, cmdidx++);
         if (pReqLayout == null)
            return;
         Int32 times = 0;
         if (cmdidx < cmds.Length)
            Int32.TryParse(cmds[cmdidx], out times);
         RequestRec req = this.RequestRecs_[pReqLayout->LayoutId_ - 1];
         System.Diagnostics.Stopwatch sw = new System.Diagnostics.Stopwatch();
         Int32 count = 0;
         sw.Start();
         do // 最少送一次.
         {
            ++count;
            f9oms.Api.SendRequestString(this, ref *pReqLayout, req.RequestPacket);
         } while (--times > 0);
         sw.Stop();
         double us = 1000L * 1000L * sw.ElapsedTicks / (double)System.Diagnostics.Stopwatch.Frequency;
         Console.WriteLine($"Elapsed={us} us / {count}; Avg={us / count} us");
      }
   }
}
