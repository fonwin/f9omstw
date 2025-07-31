// \file f9omsrc/OmsRcClient_UT.c
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9omsrc/OmsRc.h"
#include "f9omstw/OmsToolsC.h" // f9omstw_IncStrAlpha();
#include "f9omstw/OmsMakeErrMsg.h"
#include "fon9/ConsoleIO.h"
#include "fon9/CTools.h"
#include "fon9/CtrlBreakHandler.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#ifdef fon9_WINDOWS
#include <crtdbg.h>
#else
#define strncpy_s(dstbuf,dstsz,srcbuf,count)    strncpy(dstbuf,srcbuf,count)
#endif
//--------------------------------------------------------------------------//
#define f9rc_ClientLogFlag_ut    ((f9rc_ClientLogFlag)0x80000000)
//--------------------------------------------------------------------------//
#define kMaxFieldCount  64
#define kMaxValueSize   1024
typedef struct {
   // 為了簡化測試, 每個下單要求最多支援 kMaxFieldCount 個欄位, 每個欄位最多 kMaxValueSize bytes(包含EOS).
   char  Fields_[kMaxFieldCount][kMaxValueSize];
   char  RequestStr_[kMaxFieldCount * kMaxValueSize];
} RequestRec;

fon9_WARN_DISABLE_PADDING;
typedef struct {
   f9rc_ClientSession*           Session_;
   const f9OmsRc_ClientConfig*   Config_;
   f9OmsRc_CoreTDay              CoreTDay_;
   f9OmsRc_SNO                   LastSNO_;
   // 為了簡化測試, 最多支援 n 種下單表格(下單要求).
   RequestRec  RequestRecs_[16];

   /// SvConfig_->RightsTables_ 只能在 OnSvConfig() 事件裡面安全的使用,
   /// 其他地方使用 SvConfig_->RightsTables_ 都不安全.
   const f9sv_ClientConfig*   SvConfig_;
   const void*                LastQueryUserData_;
   const void*                LastGridViewUserData_;
   const void*                LastPutUserData_;
   const void*                LastCommandUserData_;
} UserDefine;
fon9_WARN_POP;

static f9OmsRc_RptFilter   gRptFilter = f9OmsRc_RptFilter_RptAll;
//--------------------------------------------------------------------------//
void PrintEvSplit(const char* evName) {
   printf("========== %s: ", evName);
}
void fon9_CAPI_CALL OnClientLinkEv(f9rc_ClientSession* ses, f9io_State st, fon9_CStrView info) {
   (void)st; (void)info;
   UserDefine* ud = ses->UserData_;
   ud->Config_ = NULL;
   if (st == f9io_State_Listening /* = Listening for Config = Client ApReady */) {
      // info = "...|ExpDays=N|ExpDate=YYYY/MM/DD"
      for (;;) {
         size_t slen = (size_t)(info.End_ - info.Begin_);
         if (slen <= 0)
            break;
         info.Begin_ = memchr(info.Begin_, '|', slen);
         if (info.Begin_ == NULL)
            break;
         if (memcmp(info.Begin_, "|ExpDays=", 9) == 0) {
            // 密碼還有幾天到期? = 在幾天內必須改密碼?
            printf("Password.ExpDays=%lu\n", strtoul(info.Begin_ + 9, NULL, 10));
         }
         else if (memcmp(info.Begin_, "|ExpDate=", 9) == 0) {
            // 密碼到期日.
            char  strDate[16];
            memcpy(strDate, info.Begin_ + 9, 10); // "YYYY/MM/DD"
            strDate[10] = '\0';
            printf("Password.ExpDate=%s\n", strDate);
         }
         ++info.Begin_;
      }
   }
}
void fon9_CAPI_CALL OnClientConfig(f9rc_ClientSession* ses, const f9OmsRc_ClientConfig* cfg) {
   UserDefine* ud = ses->UserData_;
   ud->Config_ = cfg;
   if (f9OmsRc_IsCoreTDayChanged(&ud->CoreTDay_, &cfg->CoreTDay_)) {
      ud->LastSNO_ = 0;
      ud->CoreTDay_ = cfg->CoreTDay_;
   }
   f9OmsRc_SubscribeReport(ses, cfg, ud->LastSNO_ + 1, gRptFilter);
}
void fon9_CAPI_CALL OnClientReport(f9rc_ClientSession* ses, const f9OmsRc_ClientReport* rpt) {
   UserDefine* ud = ses->UserData_;
   if (fon9_LIKELY(rpt->Layout_)) {
      if (rpt->ReportSNO_ == 0) {
         // 尚未進入 OmsCore 之前就被拒絕, OmsCore log 不會記錄此筆訊息.
         return;
      }
   }
   else { // if (rpt->Layout_ == NULL) // 回補結束.
      PrintEvSplit("OnClientReport.RecoverEnd");
      printf("LastSNO=%" PRIu64 "\n", rpt->ReportSNO_);
   }
   ud->LastSNO_ = rpt->ReportSNO_;

   if (rpt->Layout_ != NULL &&
      ((ses->LogFlags_ & f9oms_ClientLogFlag_All) == f9oms_ClientLogFlag_All
      || (ses->LogFlags_ & f9rc_ClientLogFlag_ut))) {
      char  msgbuf[1024*4];
      char* const pend = msgbuf + sizeof(msgbuf);
      char* pmsg = msgbuf;
      if (rpt->ReportSNO_)
         pmsg += snprintf(pmsg, (size_t)(pend - pmsg), "[SNO=%" PRIu64 "]", rpt->ReportSNO_);
      if (rpt->ReferenceSNO_)
         pmsg += snprintf(pmsg, (size_t)(pend - pmsg), "[REF=%" PRIu64 "]", rpt->ReferenceSNO_);
      pmsg += snprintf(pmsg, (size_t)(pend - pmsg),
                       "%2u:%s(%u|%s)", rpt->Layout_->LayoutId_, rpt->Layout_->LayoutName_.Begin_,
                       rpt->Layout_->LayoutKind_, rpt->Layout_->ExParam_.Begin_);
      for (unsigned L = 0; L < rpt->Layout_->FieldCount_; ++L) {
         pmsg += snprintf(pmsg, (size_t)(pend - pmsg), "|%s=%s",
                         rpt->Layout_->FieldArray_[L].Named_.Name_.Begin_,
                         rpt->FieldArray_[L].Begin_);
      }
      PrintEvSplit("OnClientReport");
      puts(msgbuf);
   }
}
void fon9_CAPI_CALL OnClientFcReq(f9rc_ClientSession* ses, unsigned usWait) {
   // 也可不提供此 function: f9OmsRc_ClientHandler.FnOnFlowControl_ = NULL;
   // 則由 API 判斷超過流量時: 等候解除流量管制 => 送出下單要求 => 返回下單要求呼叫端.
   PrintEvSplit("OnClientFcReq");
   printf("ses=%p|wait=%u us\n", ses, usWait);
   fon9_SleepMS((usWait + 999) / 1000);
}
//--------------------------------------------------------------------------//
void PrintRequest(const f9OmsRc_Layout* pReqLayout, const RequestRec* req) {
   printf("[%u] %s\n", pReqLayout->LayoutId_, pReqLayout->LayoutName_.Begin_);
   for (unsigned iFld = 0; iFld < pReqLayout->FieldCount_; ++iFld) {
      const f9OmsRc_LayoutField* fld = &pReqLayout->FieldArray_[iFld];
      printf("\t[%2u] %-6s %-10s = '%s'\n", iFld, fld->TypeId_, fld->Named_.Name_.Begin_, req->Fields_[iFld]);
   }
}
void PrintConfig(const UserDefine* ud) {
   if (!ud->Config_) {
      puts("Config not ready.");
      return;
   }
   // TODO: 直接在此輸出並不安全, 要先 lock ud, 然後才能安全的取得並使用 ud->Config_.
   const f9OmsRc_ClientConfig* cfg = ud->Config_;
   printf("HostId = %u\n", cfg->CoreTDay_.HostId_);
   printf("TDay   = %u/%u\n", cfg->CoreTDay_.YYYYMMDD_, cfg->CoreTDay_.UpdatedCount_);
   printf("Tables =\n%s\n", cfg->RightsTables_.OrigStrView_.Begin_);

   for (unsigned iReqLayout = 0; iReqLayout < cfg->RequestLayoutCount_; ++iReqLayout)
      PrintRequest(cfg->RequestLayoutArray_[iReqLayout], &ud->RequestRecs_[iReqLayout]);
}

const f9OmsRc_Layout* GetRequestLayout(UserDefine* ud, char** cmd) {
   if (!ud->Config_) {
      puts("Config not ready.");
      return NULL;
   }
   // Req(Id or Name)
   const char* reqName = (*cmd ? fon9_StrCutSpace(*cmd, cmd) : "");
   if (isdigit(*reqName)) {
      unsigned id = (unsigned)strtoul(reqName, NULL, 10);
      if (id <= 0 || ud->Config_->RequestLayoutCount_ < id) {
         printf("Unknown request id = %s\n", reqName);
         return NULL;
      }
      return ud->Config_->RequestLayoutArray_[id - 1];
   }
   const f9OmsRc_Layout* pReqLayout = f9OmsRc_GetRequestLayout(ud->Session_, ud->Config_, reqName);
   if (pReqLayout)
      return pReqLayout;
   printf("Unknown request name = %s\n", reqName);
   return NULL;
}
void MakeRequestStr(const f9OmsRc_Layout* pReqLayout, RequestRec* req) {
   char* reqstr = req->RequestStr_;
   for (unsigned iFld = 0; iFld < pReqLayout->FieldCount_; ++iFld) {
      const char* str = req->Fields_[iFld];
      size_t      len = strlen(str);
      memcpy(reqstr, str, len);
      *(reqstr += len) = '\x01';
      ++reqstr;
   }
   *reqstr = '\0';
}

int FetchFieldIndex(char** pcmd, const f9OmsRc_Layout* pReqLayout) {
   unsigned iFld;
   if (isdigit((unsigned char)**pcmd)) {
      iFld = (unsigned)strtoul(*pcmd, pcmd, 10);
      if (iFld >= pReqLayout->FieldCount_) {
         printf("Unknwon field index = %u\n", iFld);
         return -1;
      }
   }
   else {
      const char* fldName = fon9_StrFetchNoTrim(*pcmd, (const char**)pcmd, "= \t");
      char ch = **pcmd;
      **pcmd = '\0';
      for (iFld = 0; iFld < pReqLayout->FieldCount_; ++iFld) {
         if (strcmp(pReqLayout->FieldArray_[iFld].Named_.Name_.Begin_, fldName) == 0)
            goto __FOUND_FIELD;
      }
      printf("Field not found: %s\n", fldName);
      return -1;
   __FOUND_FIELD:
      **pcmd = ch;
   }
   *pcmd = fon9_StrTrimHead(*pcmd);
   if (**pcmd != '=') {
      printf("Loss '=' for field.");
      return -1;
   }
   *pcmd = fon9_StrTrimHead(*pcmd + 1);
   return (int)iFld;
}
void SetRequest(UserDefine* ud, char* cmd) {
   // Req(Id or Name) fld(Index or Name)=value|fld2=val2|fld3=val3...
   const f9OmsRc_Layout* pReqLayout = GetRequestLayout(ud, &cmd);
   if (!pReqLayout)
      return;
   RequestRec* req = &ud->RequestRecs_[pReqLayout->LayoutId_ - 1];
   unsigned    iFld;
   if (cmd == NULL)
      goto __BREAK_PUT_FIELDS;
   for (;;) {
      if ((int)(iFld = (unsigned)FetchFieldIndex(&cmd, pReqLayout)) < 0)
         break;
      char* val;
      switch (*cmd) {
      case '\'': case '"':
         cmd = strchr(val = cmd + 1, *cmd);
         if (cmd == NULL) {
            printf("Cannot find matching [%c].\n", *(val - 1));
            goto __BREAK_PUT_FIELDS;
         }
         *cmd = '\0';
         for (char* pn = val; pn != cmd;) {
            if (*pn == '\0')
               break;
            if (*pn != '\\') {
               ++pn;
               continue;
            }
            switch (*++pn) {
            case 'n':   *(pn - 1) = '\n';   break;
            case '!':   *(pn - 1) = '\x01'; break;
            case '\\':  break;
            }
            memmove(pn, pn + 1, (size_t)(cmd - pn));
         }
         cmd = fon9_StrTrimHead(cmd + 1);
         switch (*cmd) {
         case '|':   ++cmd;      break;
         case '\0':  cmd = NULL; break;
         }
         break;
      default:
         val = (char*)fon9_StrFetchNoTrim(cmd, (const char**)&cmd, "|");
         int isEOS = (*cmd == '\0');
         *fon9_StrTrimTail(val, cmd) = '\0';
         if (isEOS)
            cmd = NULL;
         break;
      }
      strncpy_s(req->Fields_[iFld], kMaxValueSize - 1, val, sizeof(req->Fields_[iFld]) - 1);
      req->Fields_[iFld][sizeof(req->Fields_[iFld]) - 1] = '\0';
      if (!cmd)
         break;
      cmd = fon9_StrTrimHead(cmd + 1);
   }
__BREAK_PUT_FIELDS:
   MakeRequestStr(pReqLayout, req);
   PrintRequest(pReqLayout, req);
   printf("RequestStr = [%s]\n", req->RequestStr_);
}
//--------------------------------------------------------------------------//
typedef struct {
   f9rc_ClientSession*     Session_;
   const f9OmsRc_Layout*   ReqLayout_;
   RequestRec*             ReqRec_;
   unsigned long           IntervalMS_;
   unsigned long           Times_;
   int                     IsBatch_;
   char                    Padding____[4];
} SendArgs;

#define kMaxLoopValueCount 100
typedef struct {
   fon9_CStrView  Values_[kMaxLoopValueCount];
   unsigned       ValueCount_;
   unsigned       FieldIndex_;
} LoopField;

uint64_t SendGroup(char* cmd, const SendArgs args, uint64_t* usEnd) {
   fon9_CStrView  reqFieldArray[kMaxFieldCount];
   for (unsigned L = 0; L < args.ReqLayout_->FieldCount_; ++L) {
      reqFieldArray[L].Begin_ = args.ReqRec_->Fields_[L];
      reqFieldArray[L].End_ = memchr(args.ReqRec_->Fields_[L], '\0', sizeof(args.ReqRec_->Fields_[L]));
   }
   const char* groupName = fon9_StrCutSpace(cmd, &cmd);
   LoopField   loopFields[kMaxFieldCount];
   unsigned    loopFieldCount = 0;
   fon9_CStrView* pAutoOrdNo = NULL; // OrdNo 自動累加.
   if (cmd) {
      if (*cmd == '|') {
         // "|FieldName1=V1,V2,V3...|FieldName2=v1,v2..."
         // value 使用「,」分隔, 不支援特殊字元及引號.
         for (;;) {
            LoopField* fld = loopFields + loopFieldCount;
            ++cmd;
            fld->FieldIndex_ = (unsigned)FetchFieldIndex(&cmd, args.ReqLayout_);
            if ((int)fld->FieldIndex_ < 0)
               return 0;
            fld->ValueCount_ = 0;
            ++loopFieldCount;
            for (;;) {
               fon9_CStrView* val = &fld->Values_[fld->ValueCount_++];
               val->Begin_ = fon9_StrFetchNoTrim(fon9_StrTrimHead(cmd), &val->End_, ",|");
               char chSpl = *(cmd = (char*)val->End_);
               val->End_ = fon9_StrTrimTail((char*)val->Begin_, cmd);
               *((char*)val->End_) = '\0';
               if (chSpl == '\0')
                  goto __CMD_PARSE_END;
               if (chSpl == '|')
                  break;
               ++cmd;
            }
         }
      }
      else if (*cmd == '+') {
         if (args.ReqLayout_->IdxOrdNo_ >= 0)
            pAutoOrdNo = &reqFieldArray[args.ReqLayout_->IdxOrdNo_];
      }
   }
__CMD_PARSE_END:;
   f9OmsRc_RequestBatch* reqb = NULL;
   f9OmsRc_RequestBatch* ptrL;
   if (args.IsBatch_) {
      size_t recSize = args.ReqLayout_->FieldCount_ * (size_t)kMaxValueSize;
      if ((reqb = malloc(sizeof(f9OmsRc_RequestBatch) * args.Times_)) == NULL) {
      __NOT_ENOUGH_MEM:;
         puts("Not enough memory.");
         return 0;
      }
      if ((reqb->ReqStr_.Begin_ = malloc(recSize * args.Times_)) == NULL) {
         free(reqb);
         goto __NOT_ENOUGH_MEM;
      }
      ptrL = reqb;
      for (unsigned long L = 0; L < args.Times_; ++L) {
         ptrL->Layout_ = args.ReqLayout_;
         ptrL->ReqStr_.Begin_ = reqb->ReqStr_.Begin_ + (recSize * L);
         ++ptrL;
      }
   }
   uint64_t usBeg = fon9_GetSystemUS();
   fon9_CStrView* pClOrdId = (args.ReqLayout_->IdxClOrdId_ >= 0) ? &reqFieldArray[args.ReqLayout_->IdxClOrdId_] : NULL;
   fon9_CStrView* pUsrDef = (args.ReqLayout_->IdxUsrDef_ >= 0) ? &reqFieldArray[args.ReqLayout_->IdxUsrDef_] : NULL;
   ptrL = reqb;
   for (unsigned long L = 0; L < args.Times_; ++L) {
      if (loopFieldCount) {
         const LoopField* fld = loopFields;
         for (unsigned lc = 0; lc < loopFieldCount; ++lc) {
            reqFieldArray[fld->FieldIndex_] = fld->Values_[L % fld->ValueCount_];
            ++fld;
         }
      }
      if (pAutoOrdNo)
         f9omstw_IncStrAlpha((char*)pAutoOrdNo->Begin_, (char*)pAutoOrdNo->End_);
      if (pClOrdId)
         pClOrdId->End_ = pClOrdId->Begin_ + snprintf((char*)pClOrdId->Begin_, kMaxValueSize, "%s:%lu", groupName, L + 1);
      if (pUsrDef)
         pUsrDef->End_ = pUsrDef->Begin_ + snprintf((char*)pUsrDef->Begin_, kMaxValueSize, "%" PRIu64, fon9_GetSystemUS());
      if (fon9_UNLIKELY(args.IsBatch_)) {
         char* reqstr = (char*)(ptrL->ReqStr_.Begin_);
         for (unsigned iFld = 0; iFld < ptrL->Layout_->FieldCount_; ++iFld) {
            const fon9_CStrView str = reqFieldArray[iFld];
            const size_t len = (size_t)(str.End_ - str.Begin_);
            memcpy(reqstr, str.Begin_, len);
            reqstr += len;
            *reqstr++ = '\x01';
         }
         ptrL->ReqStr_.End_ = reqstr - 1;
         ++ptrL;
      }
      else {
         f9OmsRc_SendRequestFields(args.Session_, args.ReqLayout_, reqFieldArray);
         if (args.IntervalMS_ > 0)
            fon9_SleepMS((unsigned)args.IntervalMS_);
      }
   }
   if (args.IsBatch_) {
      usBeg = fon9_GetSystemUS();
      f9OmsRc_SendRequestBatch(args.Session_, reqb, (unsigned)args.Times_);
   }
   *usEnd = fon9_GetSystemUS();
   if (pClOrdId)
      *(char*)(pClOrdId->Begin_) = '\0';
   if (pUsrDef)
      *(char*)(pUsrDef->Begin_) = '\0';
   if (reqb) {
      free((void*)(reqb->ReqStr_.Begin_));
      free(reqb);
   }
   return usBeg;
}
void SendRequest(UserDefine* ud, char* cmd) {
   // send Req(Id or Name) times[/msInterval] [GroupId]
   SendArgs args;
   args.ReqLayout_ = GetRequestLayout(ud, &cmd);
   if (!args.ReqLayout_)
      return;
   args.ReqRec_ = &ud->RequestRecs_[args.ReqLayout_->LayoutId_ - 1];
   args.Session_ = ud->Session_;

   args.Times_ = (cmd ? strtoul(cmd, &cmd, 10) : 1u);
   if (args.Times_ == 0)
      args.Times_ = 1;
   args.IntervalMS_ = 0;
   args.IsBatch_ = 0;
   if (cmd) {
      cmd = fon9_StrTrimHead(cmd);
      if (*cmd == '/') { // times/msInterval
         args.IntervalMS_ = strtoul(cmd + 1, &cmd, 10);
         cmd = fon9_StrTrimHead(cmd);
         if (*cmd == 'B') {
            // times/B 例: 15/B
            args.IsBatch_ = 1;
            cmd = fon9_StrTrimHead(cmd + 1);
         }
      }
   }
   fon9_CStrView  reqstr;
   reqstr.Begin_ = args.ReqRec_->RequestStr_;
   reqstr.End_ = memchr(reqstr.Begin_, '\0', sizeof(args.ReqRec_->RequestStr_));
   if (reqstr.End_ - reqstr.Begin_ <= 0) {
      puts("Request message is empty?");
      PrintRequest(args.ReqLayout_, args.ReqRec_);
      return;
   }
   uint64_t usBeg, usEnd;
   if (cmd == NULL || *cmd == '\0') {
      if (args.IsBatch_) {
         f9OmsRc_RequestBatch* const reqb = malloc(sizeof(f9OmsRc_RequestBatch) * args.Times_);
         if (reqb == NULL) {
            puts("Not enough memory.");
            return;
         }
         f9OmsRc_RequestBatch* ptrL = reqb;
         for (unsigned long L = 0; L < args.Times_; ++L) {
            ptrL->Layout_ = args.ReqLayout_;
            ptrL->ReqStr_ = reqstr;
            ++ptrL;
         }
         usBeg = fon9_GetSystemUS();
         f9OmsRc_SendRequestBatch(args.Session_, reqb, (unsigned)args.Times_);
         usEnd = fon9_GetSystemUS();
         free(reqb);
      }
      else {
         usBeg = fon9_GetSystemUS();
         for (unsigned long L = 0; L < args.Times_; ++L) {
            f9OmsRc_SendRequestString(args.Session_, args.ReqLayout_, reqstr);
            if (args.IntervalMS_ > 0)
               fon9_SleepMS((unsigned)args.IntervalMS_);
         }
         usEnd = fon9_GetSystemUS();
      }
   }
   else if ((usBeg = SendGroup(cmd, args, &usEnd)) <= 0)
      return;

   printf("Begin: %" PRIu64 ".%06" PRIu64 "\n", usBeg / 1000000, usBeg % 1000000);
   printf("  End: %" PRIu64 ".%06" PRIu64 "\n", usEnd / 1000000, usEnd % 1000000);
   printf("Spent: %" PRIu64 " us / %lu times = %lf",
          usEnd - usBeg, args.Times_, (double)(usEnd - usBeg) / (double)args.Times_);
   if (args.IsBatch_)
      printf(" / Batch\n");
   else if (args.IntervalMS_)
      printf(" / %lu ms\n", args.IntervalMS_);
   else
      puts("");
}
//--------------------------------------------------------------------------//
void fon9_CAPI_CALL OnSvConfig(f9rc_ClientSession* ses, const f9sv_ClientConfig* cfg) {
   PrintEvSplit("OnSvConfig");
   printf("FcQry=%u/%u|MaxSubrCount=%u\n" "{%s}\n",
          cfg->FcQryCount_, cfg->FcQryMS_, cfg->MaxSubrCount_,
          cfg->RightsTables_.OrigStrView_.Begin_);
   UserDefine* ud = ses->UserData_;
   ud->SvConfig_ = cfg;
   puts("====================");
}

void PrintSeedValues(const f9sv_ClientReport* rpt) {
   if (rpt->Seed_ == NULL)
      return;
   const f9sv_Field* fld = rpt->Tab_->FieldArray_;
   unsigned fldidx = rpt->Tab_->FieldCount_;
   if (fldidx <= 0)
      return;
   for (;;) {
      char  buf[1024];
      printf("%s=[%s]", fld->Named_.Name_.Begin_,
             f9sv_GetField_StrN(rpt->Seed_, fld, buf, sizeof(buf)));
      if (--fldidx <= 0)
         break;
      printf("|");
      ++fld;
   }
   printf("\n");
}
void PrintSvEvSplit(const char* evName, const f9sv_ClientReport* rpt) {
   PrintEvSplit(evName);
   printf("UserData=%u, result=%d:%s\n"
          "treePath=[%s], seedKey=[%s], tab=[%s|%u]\n",
          (unsigned)((uintptr_t)rpt->UserData_),
          rpt->ResultCode_, f9sv_GetSvResultMessage(rpt->ResultCode_),
          rpt->TreePath_.Begin_,
          rpt->SeedKey_.Begin_,
          rpt->Tab_->Named_.Name_.Begin_,
          rpt->Tab_->Named_.Index_);
   if (rpt->ExResult_.Begin_ != rpt->ExResult_.End_)
      printf("exResult=[%s]\n", rpt->ExResult_.Begin_);
}
void PrintSvReport(const char* evName, const f9sv_ClientReport* rpt) {
   PrintSvEvSplit(evName, rpt);
   PrintSeedValues(rpt);
   puts("====================");
}
void fon9_CAPI_CALL OnSvQueryReport(f9rc_ClientSession* ses, const f9sv_ClientReport* rpt) {
   PrintSvReport("OnSv.QueryReport", rpt);
   UserDefine* ud = ses->UserData_;
   ud->LastQueryUserData_ = rpt->UserData_;
}
void fon9_CAPI_CALL OnSvGridViewReport(f9rc_ClientSession* ses, const f9sv_ClientReport* rpt) {
   PrintSvEvSplit("OnSv.GridViewReport", rpt);
   if (rpt->ResultCode_ == f9sv_Result_NoError) {
      fon9_CStrView exResult = rpt->ExResult_;
      for (;;) {
         fon9_CStrView key = f9sv_GridView_Parser(rpt, &exResult);
         if (key.Begin_ == NULL)
            break;
         printf("[%s]|", key.Begin_);
         PrintSeedValues(rpt);
      }
      printf("%u/%" PRId64 " [%" PRId64 ":%" PRId64 "]\n",
             (unsigned)rpt->GridViewResultCount_, (int64_t)rpt->GridViewTableSize_,
             (int64_t)rpt->GridViewDistanceBegin_, (int64_t)rpt->GridViewDistanceEnd_);
   }
   puts("====================");
   UserDefine* ud = ses->UserData_;
   ud->LastGridViewUserData_ = rpt->UserData_;
}
void fon9_CAPI_CALL OnSvSubscribeReport(f9rc_ClientSession* ses, const f9sv_ClientReport* rpt) {
   (void)ses;
   if (rpt->Seed_ == NULL) { // 訂閱結果通知.
      if (rpt->ResultCode_ == f9sv_Result_NoError) {
         // 訂閱成功.
      }
      else {
         // 訂閱失敗.
      }
   }
   PrintSvReport("OnSv.SubscribeReport", rpt);
}
void fon9_CAPI_CALL OnSvUnsubscribeReport(f9rc_ClientSession* ses, const f9sv_ClientReport* rpt) {
   (void)ses;
   PrintSvReport("OnSv.UnsubscribeReport", rpt);
}
void fon9_CAPI_CALL OnSvWriteReport(f9rc_ClientSession* ses, const f9sv_ClientReport* rpt) {
   PrintSvReport("OnSv.WriteReport", rpt);
   UserDefine* ud = ses->UserData_;
   ud->LastPutUserData_ = rpt->UserData_;
}
void fon9_CAPI_CALL OnSvRemoveReport(f9rc_ClientSession* ses, const f9sv_ClientReport* rpt) {
   (void)ses;
   // assert(rpt->Seed_ == NULL); // f9sv_Remove() 不論成功或失敗, 都不會提供 rpt->Seed_;
   PrintSvReport("OnSv.RemoveReport", rpt);
}
void fon9_CAPI_CALL OnSvCommandReport(f9rc_ClientSession* ses, const f9sv_ClientReport* rpt) {
   PrintSvReport("OnSv.CommandReport", rpt);
   UserDefine* ud = ses->UserData_;
   ud->LastCommandUserData_ = rpt->UserData_;
}
//--------------------------------------------------------------------------//
const char* ParseSvCommand(f9sv_SeedName* seedName, char* cmdln, const char* svCmdName) {
   memset(seedName, 0, sizeof(*seedName));
   if (cmdln == NULL) {
      printf("%s: require 'treePath'\n", svCmdName);
      return NULL;
   }
   seedName->TreePath_ = fon9_StrCutSpace(cmdln, &cmdln);
   if (cmdln == NULL) {
      printf("%s: require 'key'\n", svCmdName);
      return NULL;
   }
   char* seedKey;
   seedName->SeedKey_ = seedKey = fon9_StrCutSpace(cmdln, &cmdln);
   switch (seedKey[0]) {
   case '\\':
      if (seedKey[2] == '\0') {
         switch (seedKey[1]) {
         case 't': // 輸入2字元 "\\t" => 送出1字元 "\t" => subr tree.
            seedKey[0] = '\t';
            break;
         case 'b':
            seedKey[0] = '\b'; // gv 從尾端往前進.
            break;
         }
         seedKey[1] = '\0';
      }
      break;
   case '\'':
   case '\"':
      // '' or "" => 空字串;
      if (seedKey[1] == seedKey[0] && seedKey[2] == '\0') {
         seedKey[0] = '\0';
      }
      break;
   }
   if (cmdln) {
      if (isdigit((unsigned char)*cmdln))
         seedName->TabIndex_ = (f9sv_TabSize)strtoul(cmdln, &cmdln, 10);
      else
         seedName->TabName_ = fon9_StrCutSpace(cmdln, &cmdln);
   }
   return cmdln ? cmdln : "";
}
void PrintSvCommandResult(const f9sv_SeedName* seedName, const char* svCmdName, f9sv_ReportHandler* handler, f9sv_Result res) {
   printf("---------- %s: UserData=%u, return=%d:%s\n"
          "treePath=[%s], seedKey=[%s], tab=[%s|%u]\n"
          "--------------------\n",
          svCmdName, (unsigned)((uintptr_t)handler->UserData_),
          res, f9sv_GetSvResultMessage(res),
          seedName->TreePath_, seedName->SeedKey_,
          seedName->TabName_, seedName->TabIndex_);
}
// 等候結果 ud->LastQueryUserData_ or ud->LastPutUserData_, 或一小段時間(沒結果則放棄等候).
int WaitSvCommandDone(const void** pUdUserData, const void* waitUserData) {
   for (unsigned L = 0; L < 100; ++L) {
      if (*pUdUserData == waitUserData)
         return 1;
      fon9_SleepMS(10u);
   }
   return 0;
}

typedef f9sv_Result (fon9_CAPI_CALL *FnSvCmd3)(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler);
int RunSvCommandFn3(FnSvCmd3 fnSvCmd3, char* cmdln, const char* svCmdName, UserDefine* ud, f9sv_ReportHandler* handler,
                   const void** udWait) {
   f9sv_SeedName seedName;
   if (!ParseSvCommand(&seedName, cmdln, svCmdName))
      return 0;
   handler->UserData_ = ((char*)handler->UserData_) + 1;
   PrintSvCommandResult(&seedName, svCmdName, handler,
                        fnSvCmd3(ud->Session_, &seedName, *handler));
   if (udWait)
      WaitSvCommandDone(udWait, handler->UserData_);
   return 1;
}

typedef f9sv_Result (fon9_CAPI_CALL *FnSvCmd4)(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler,
                                               const char* strFieldValues);
int RunSvCommandFn4(FnSvCmd4 fnSvCmd4, char* cmdln, const char* svCmdName, UserDefine* ud, f9sv_ReportHandler* handler,
                    const void** udWait) {
   f9sv_SeedName seedName;
   const char*   exArgs = ParseSvCommand(&seedName, cmdln, svCmdName);
   if (exArgs == NULL)
      return 0;
   handler->UserData_ = ((char*)handler->UserData_) + 1;
   PrintSvCommandResult(&seedName, svCmdName, handler,
                        fnSvCmd4(ud->Session_, &seedName, *handler, exArgs));
   if (udWait)
      WaitSvCommandDone(udWait, handler->UserData_);
   return 1;
}
// -----
f9sv_Result fon9_CAPI_CALL Proxy_f9sv_GridView(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler, const char* strReqRowCount) {
   return f9sv_GridView(ses, seedName, handler, (int16_t)strtol(strReqRowCount, NULL, 10));
}
//--------------------------------------------------------------------------//
const char  kCSTR_LogFileFmt[] =
"   LogFileFmt:\n"
"     '' = ./logs/{0:f+'L'}/fon9cli.log\n"
"     time format: {0:?}\n"
"       L = YYYYMMDDHHMMSS\n"
"       f = YYYYMMDD\n"
"       F = YYYY-MM-DD\n"
"       K = YYYY/MM/DD\n"
"       Y = YYYY\n"
"       m = MM = Month\n"
"       d = DD = Day\n"
"       +'L' = to localtime\n";
const char  kCSTR_LogFlags[] =
"   LogFlags(hex):\n"
"       1 = f9oms_ClientLogFlag_Link\n"
"   10000 = f9oms_ClientLogFlag_Request & Config\n"
"   20000 = f9oms_ClientLogFlag_Report  & Config\n"
"   40000 = f9oms_ClientLogFlag_Config\n";
//--------------------------------------------------------------------------//
void PromptSleep(const char* psec, const void** wait, double secsDefault) {
   double secs = psec ? strtod(psec, NULL) : secsDefault;
   if (secs <= 0) {
      secs = secsDefault;
   }
   while (wait == NULL || *wait == NULL) {
      if (secs >= 1)
         fon9_SleepMS(1000u);
      else {
         if (secs > 0)
            fon9_SleepMS((unsigned)(secs * 1000));
         break;
      }
      printf("\r%u ", (unsigned)(--secs));
      fflush(stdout);
   }
   printf("\r");
}
//--------------------------------------------------------------------------//
UserDefine  ud;
//---------------------------------
int main(int argc, char* argv[]) {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   //_CrtSetBreakAlloc(176);
#endif
   fon9_SetConsoleUTF8();
   fon9_SetupCtrlBreakHandler();
   printf("f9OmsRc API library version info: %s\n", f9OmsRc_ApiVersionInfo());
   // ----------------------------

   f9rc_ClientSessionParams  f9rcCliParams;
   memset(&f9rcCliParams, 0, sizeof(f9rcCliParams));
   f9rcCliParams.DevName_ = "TcpClient";
   f9rcCliParams.LogFlags_ = f9rc_ClientLogFlag_All;
   f9rcCliParams.FnOnLinkEv_ = &OnClientLinkEv;

   f9OmsRc_ClientSessionParams   omsRcParams;
   f9OmsRc_InitClientSessionParams(&f9rcCliParams, &omsRcParams);
   omsRcParams.FnOnConfig_ = &OnClientConfig;
   omsRcParams.FnOnReport_ = &OnClientReport;
   omsRcParams.FnOnFlowControl_ = &OnClientFcReq;

   const char*  logFileFmt = NULL;
   const char** pargv = (const char**)argv;
   int          isChgPass = 0;
   for (int L = 1; L < argc;) {
      const char* parg = *++pargv;
      switch (parg[0]) {
      case '-': case '/':  break;
      default:
   __UNKNOWN_ARGUMENT:
         printf("Unknown argument: %s\n", parg);
         goto __USAGE;
      }
      ++L;
      // ----- 無後續參數的 arg.
      switch (parg[1]) {
      case 'c':   isChgPass = 1;    continue;
      }
      // -----
      ++L;
      ++pargv;
      switch (parg[1]) {
      case 'l':   logFileFmt = *pargv;               break;
      case 'f':   f9rcCliParams.LogFlags_ = (f9rc_ClientLogFlag)strtoul(*pargv, NULL, 16);    break;
      case 'u':   f9rcCliParams.UserId_ = *pargv;    break;
      case 'p':   f9rcCliParams.Password_ = *pargv;  break;
      case 'n':   f9rcCliParams.DevName_ = *pargv;   break;
      case 'a':   f9rcCliParams.DevParams_ = *pargv; break;
      case 't':
         f9omstw_FreeOmsErrMsgTx(omsRcParams.ErrCodeTx_);
         omsRcParams.ErrCodeTx_ = f9omstw_LoadOmsErrMsgTx1(*pargv);
         break;
      case 'r':
         if (strcmp(*pargv, "NoCheckSum") == 0)
            f9rcCliParams.RcFlags_ |= f9rc_RcFlag_NoChecksum;
         break;
      case 'D': // DefaultThreadPool
         fon9_PresetDefaultThreadPoolStrArg(*pargv);
         break;
      case 'R': // RptFilter
         gRptFilter = (f9OmsRc_RptFilter)strtoul(*pargv, NULL, 16);
         break;
      case '?':   goto __USAGE;
      default:    goto __UNKNOWN_ARGUMENT;
      }
   }
   if(f9rcCliParams.UserId_ == NULL
   || f9rcCliParams.DevName_ == NULL
   || f9rcCliParams.DevParams_ == NULL) {
__USAGE:
      printf("Usage:\n"
             "-l LogFileFmt\n"
             "   default is log to console\n"
             "%s"
             "-f LogFlags(hex)\n"
             "   default is ffff = all of below.\n"
             "%s"
             "-n DevName\n"
             "   default is TcpClient\n"
             "-a DevArguments\n"
             "   e.g. -a 127.0.0.1:6601\n"
             "   e.g. -a dn=localhost:6601\n"
             "-u UserId\n"
             "-p Password\n"
             "-c\n"
             "   Change password.\n"
             "-r NoCheckSum\n"
             "   Set RcProtocol NoCheckSum flag.\n"
             "-t OmsErrCode.All.cfg:zh\n",
             kCSTR_LogFileFmt, kCSTR_LogFlags);
      return 3;
   }
   char  passwd[1024];
   if (f9rcCliParams.Password_ == NULL) {
      fon9_getpass(stdout, "Password: ", passwd, sizeof(passwd));
      f9rcCliParams.Password_ = passwd;
   }
   if (isChgPass) {
      // 改密碼: f9rcCliParams.Password_ = oldpass + '\r' + newpass
      char  pass1[1024];
      char  pass2[1024];
      fon9_getpass(stdout, "New password: ", pass1, sizeof(pass1));
      fon9_getpass(stdout, "Check new pw: ", pass2, sizeof(pass2));
      if (strcmp(pass1, pass2) != 0) {
         puts("New password isnot match 'Check new pw'.");
         return 3;
      }
      size_t pwlen = strlen(f9rcCliParams.Password_);
      if (passwd != f9rcCliParams.Password_)
         memcpy(passwd, f9rcCliParams.Password_, pwlen);
      passwd[pwlen] = '\r';
      strcpy(passwd + pwlen + 1, pass1);
      f9rcCliParams.Password_ = passwd;
   }
   // ----------------------------
   f9sv_ClientSessionParams   svRcParams;
   f9sv_InitClientSessionParams(&f9rcCliParams, &svRcParams);
   svRcParams.FnOnConfig_ = &OnSvConfig;

   memset(&ud, 0, sizeof(ud));
   f9rcCliParams.UserData_ = &ud;

   const char* iosvCfg = NULL;// "Cpus=0,1|ThreadCount=2|Wait=Block"; // OmsRc 綁核的範例.
   f9OmsRc_Initialize2(logFileFmt, iosvCfg);
   f9sv_Initialize(NULL);
   fon9_Finalize(); // 配對 f9sv_Initialize();
   f9rc_CreateClientSession(&ud.Session_, &f9rcCliParams);
   // ----------------------------
   char cmdbuf[4096];
   while (fon9_AppBreakMsg == NULL) {
      printf("> ");
      if (!fgets(cmdbuf, sizeof(cmdbuf), stdin))
         break;
      char* pend = fon9_StrTrimTail(cmdbuf, memchr(cmdbuf, '\0', sizeof(cmdbuf)));
      if (pend == cmdbuf)
         continue;
      *pend = '\0';
      char* pbeg = fon9_StrCutSpace(cmdbuf, &pend);
      if (strcmp(pbeg, "quit") == 0)
         goto __QUIT;
      else if (strcmp(pbeg, "wc") == 0) { // wait connect. default=5 secs. 0 = 1 secs;
         puts("Waiting for connection...");
         PromptSleep(pend, (const void**)&ud.Config_, 5);
         printf("%s\n", ud.Config_ ? "Connection ready." : "Wait connection timeout.");
      }
      else if (strcmp(pbeg, "sleep") == 0) { // 可輸入 sleep 0.001 表示 1ms;
         PromptSleep(pend, NULL, 1);
      }
      else if (strcmp(pbeg, "cfg") == 0)
         PrintConfig(&ud);
      else if (strcmp(pbeg, "set") == 0)
         SetRequest(&ud, pend);
      else if (strcmp(pbeg, "send") == 0)
         SendRequest(&ud, pend);
      else if (strcmp(pbeg, "lf") == 0) {
         if (pend) {
            ud.Session_->LogFlags_ = (f9rc_ClientLogFlag)strtoul(pend, &pend, 16);
            pend = fon9_StrTrimHead(pend);
         }
         printf("LogFlags = %x\n", ud.Session_->LogFlags_);
         if (pend && *pend) {
            if ((pend[0] == '\'' && pend[1] == '\'')
                || (pend[0] == '"' && pend[1] == '"'))
               *pend = '\0';
            fon9_Initialize(pend);
            fon9_Finalize();
         }
      }
      else if (strcmp(pbeg, "q") == 0) { // query: treePath key tabName
         static f9sv_ReportHandler queryHandler = {&OnSvQueryReport, NULL};
         RunSvCommandFn3(&f9sv_Query, pend, "q:query", &ud, &queryHandler, &ud.LastQueryUserData_);
      }
      else if (strcmp(pbeg, "s") == 0) { // subscribe: treePath key tabName
         static f9sv_ReportHandler  subrHandler = {&OnSvSubscribeReport, NULL};
         RunSvCommandFn3(&f9sv_Subscribe, pend, "s:subr", &ud, &subrHandler, NULL);
      }
      else if (strcmp(pbeg, "u") == 0) { // unsubscribe: treePath key tabName
         static f9sv_ReportHandler  unsubrHandler = {&OnSvUnsubscribeReport, NULL};
         RunSvCommandFn3(&f9sv_Unsubscribe, pend, "u:unsubr", &ud, &unsubrHandler, NULL);
      }
      else if (strcmp(pbeg, "gv") == 0) { // GridView: treePath key tabName   rowCount
         static f9sv_ReportHandler gvHandler = {&OnSvGridViewReport, NULL};
         RunSvCommandFn4(&Proxy_f9sv_GridView, pend, "gv:GridView", &ud, &gvHandler, &ud.LastGridViewUserData_);
      }
      else if (strcmp(pbeg, "rm") == 0) { // remove: treePath key tabName
         static f9sv_ReportHandler  removeHandler = {&OnSvRemoveReport, NULL};
         RunSvCommandFn3(&f9sv_Remove, pend, "rm:remove", &ud, &removeHandler, NULL);
      }
      else if (strcmp(pbeg, "w") == 0) { // write: treePath key tabName    fieldName=value|fieldName2=value2|...
         static f9sv_ReportHandler  svputHandler = {&OnSvWriteReport, NULL};
         RunSvCommandFn4(&f9sv_Write, pend, "w:write", &ud, &svputHandler, &ud.LastPutUserData_);
      }
      else if (strcmp(pbeg, "run") == 0) { // run: treePath key tabName    args
         static f9sv_ReportHandler  runHandler = {&OnSvCommandReport, NULL};
         RunSvCommandFn4(&f9sv_Command, pend, "run:command", &ud, &runHandler, &ud.LastCommandUserData_);
      }
      else if (strcmp(pbeg, "?") == 0 || strcmp(pbeg, "help") == 0)
         printf("quit\n"
                "   Quit program.\n"
                "\n"
                "cfg\n"
                "   List configs.\n"
                "\n"
                "set ReqId(or ReqName) FieldId(or FieldName)=value|fld2=val2|fld3=val3\n"
                "\n"
                "send ReqId(or ReqName) times[/msInterval] [GroupId]\n"
                "\t" "msInterval = B for batch mode."
                "\n"
                "lf LogFlags(hex) [LogFileFmt]\n"
                "%s"
                "%s"
                "\n"
                "q treePath key tabName\n"
                "\n"
                "wc secs"  "\t\t" "wait connect.\n"
                "\n"
                "sleep secs\n"
                "\n"
                "? or help\n"
                "   This info.\n",
                kCSTR_LogFlags, kCSTR_LogFileFmt);
      else
         printf("Unknown command: %s\n", pbeg);
   }
   // ----------------------------
__QUIT:
   f9rc_DestroyClientSession_Wait(ud.Session_);
   fon9_Finalize(); // 配對 f9OmsRc_Initialize();
   f9omstw_FreeOmsErrMsgTx(omsRcParams.ErrCodeTx_);
   printf("OmsRcClient test quit.%s\n", fon9_AppBreakMsg ? fon9_AppBreakMsg : "");
}
