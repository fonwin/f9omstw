// \file f9omsrc/OmsRcClient_UT.c
//
// 測試 OmsRc C API
// - 使用 "TcpClient" 建立 2 個連線: "127.0.0.1:6601"; "dn=localhost:6601";
// - 使用 2 個不同的 user: "admin"; "fonwin"; 下單.
//   - admin:  可用任意帳號, 可用櫃號 "A";
//   - fonwin: 可用帳號 "8610-10"; "8610-21"; 無可用櫃號;
// - 檢查:
//   - 換日事件.
//   - 流量管制.
//   - 是否有針對「可用帳號」回報.
//
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9omsrc/OmsRc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//--------------------------------------------------------------------------//
fon9_BEFORE_INCLUDE_STD;
#if defined(fon9_WINDOWS)
#  define _WINSOCKAPI_ // stops Windows.h including winsock.h
#  define NOMINMAX     // stops Windows.h #define max()
#  include <Windows.h>
inline void SleepMS(DWORD ms) {
   Sleep(ms);
}
#else//#if fon9_WINDOWS #else...
#  include <unistd.h>
inline void SleepMS(useconds_t ms) {
   usleep(ms * 1000);
}
#endif//#if WIN #else #endif
fon9_AFTER_INCLUDE_STD;
//--------------------------------------------------------------------------//
// 一筆成功的下單要求, 會用掉2個序號: req + ord;
// 但在正式環境下, 就不一定了: req + ord(Queuing) + ord(Sending) + ord(Accepted) + ord(Filled)...;
const unsigned kReqUsedSNO = 2;

typedef enum {
   OmsRcCliSt_Creating,
   OmsRcCliSt_LinkError,
   OmsRcCliSt_LinkBroken,
   OmsRcCliSt_LinkReady,
   OmsRcCliSt_Recovering,
   OmsRcCliSt_Recovered,
} OmsRcCliSt;

fon9_WARN_DISABLE_PADDING;
typedef struct {
   f9OmsRc_ClientSession*        Session_;
   const f9OmsRc_ClientConfig*   Config_;
   f9OmsRc_CoreTDay              CoreTDay_;
   unsigned                      LastClOrdId_;
   f9OmsRc_SNO                   LastSNO_;
   f9OmsRc_SNO                   CheckSum_;
   const f9OmsRc_Layout*         ReqNew_;
   fon9_CStrView*                ReqNewValues_;
   OmsRcCliSt                    State_;
} OmsRcMyself;
fon9_WARN_POP;
//--------------------------------------------------------------------------//
void WaitLastSNO(const volatile OmsRcMyself* ud, f9OmsRc_SNO expectedSNO) {
   unsigned times = 0;
   while (ud->LastSNO_ != expectedSNO) {
      SleepMS(1);
      if (++times % 1000 == 0) {
         printf("Wait ExpectedSNO=%u|LastSNO=%u|err=Timeout.\n",
            (unsigned)expectedSNO, (unsigned)ud->LastSNO_);
         if (times > 5000)
            abort();
      }
   }
}
void WaitLastSNO2(const volatile OmsRcMyself* ud1, const volatile OmsRcMyself* ud2, f9OmsRc_SNO expectedSNO) {
   WaitLastSNO(ud1, expectedSNO);
   WaitLastSNO(ud2, expectedSNO);
   if (ud1->CheckSum_ != ud2->CheckSum_) {
      puts("[ERROR] User report error.");
      abort();
   }
}
//--------------------------------------------------------------------------//
void OnClientLinkEv(f9OmsRc_ClientSession* ses, f9io_State st, fon9_CStrView info) {
   (void)info;
   OmsRcMyself* ud = (OmsRcMyself*)(ses->UserData_);
   if (st == f9io_State_LinkReady)
      ud->State_ = OmsRcCliSt_LinkReady;
   else if (ud->State_ >= OmsRcCliSt_LinkReady)
      ud->State_ = OmsRcCliSt_LinkBroken;
   else if (st == f9io_State_LinkError)
      ud->State_ = OmsRcCliSt_LinkError;
}
void OnClientConfig(f9OmsRc_ClientSession* ses, const f9OmsRc_ClientConfig* cfg) {
   OmsRcMyself* ud = (OmsRcMyself*)(ses->UserData_);
   ud->Config_ = cfg;
   ud->State_ = OmsRcCliSt_Recovering;

   if (f9OmsRc_IsCoreTDayChanged(&ud->CoreTDay_, &cfg->CoreTDay_)) {
      ud->LastSNO_ = 0;
      ud->CoreTDay_ = cfg->CoreTDay_;
      const f9OmsRc_Layout* lyTwsNew = f9OmsRc_GetRequestLayout(ses, cfg, "TwsNew");
      if (ud->ReqNew_ != lyTwsNew) {
         // 只有首次連線, 或斷線後重新連線(且 LayoutsStr 設定不同), 才有可能來到此處.
         ud->ReqNew_ = lyTwsNew;
         free(ud->ReqNewValues_);
         ud->ReqNewValues_ = calloc(lyTwsNew->FieldCount_, sizeof(fon9_CStrView));
      }
   }
   f9OmsRc_SubscribeReport(ses, ud->Config_, ud->LastSNO_ + 1, f9OmsRc_RptFilter_AllPass);

   // 印出解析前的原始字串.
   puts(cfg->TablesStrView_.Begin_);
   // 可用櫃號、下單流量.
   printf("OrdTeams=%s|FcReq=%u/%u\n", cfg->OrdTeams_.Begin_, cfg->FcReqCount_, cfg->FcReqMS_);
   // 印出解析後的資料表.
   for (f9OmsRc_TableIndex iTab = 0; iTab < cfg->TableCount_; ++iTab) {
      const f9oms_GvTable* tab = cfg->TableList_[iTab];
      printf("[%d]%s\n\t", tab->Table_.Index_, tab->Table_.Name_.Begin_);
      f9OmsRc_TableIndex iFld = 0;
      for (; iFld < tab->GvList_.FieldCount_; ++iFld) {
         if (iFld != 0)
            printf("|");
         const f9oms_GvField* fld = &tab->GvList_.FieldArray_[iFld];
         printf("[%2d]%-12s", fld->Index_, fld->Name_.Begin_);
      }
      printf("\n\t");
      for (unsigned iRec = 0; iRec < tab->GvList_.RecordCount_; ++iRec) {
         const fon9_CStrView* rec = tab->GvList_.RecordList_[iRec];
         for (iFld = 0; iFld < tab->GvList_.FieldCount_; ++iFld) {
            if (iFld != 0)
               printf("|");
            printf("%-16s", rec[iFld].Begin_);
         }
         printf("\n\t");
      }
      puts("");
   }
}
void OnClientReport(f9OmsRc_ClientSession* ses, const f9OmsRc_ClientReport* rpt) {
   OmsRcMyself* ud = (OmsRcMyself*)(ses->UserData_);
   ud->CheckSum_ += rpt->ReportSNO_ + rpt->ReferenceSNO_ + 1;
   if (fon9_LIKELY(rpt->Layout_)) {
      if (rpt->Layout_->IdxClOrdId_ >= 0) // 只有下單要求有 ClOrdId 欄位.
         ud->LastClOrdId_ = strtoul(rpt->FieldArray_[rpt->Layout_->IdxClOrdId_].Begin_, NULL, 10);
      if (ud->State_ != OmsRcCliSt_Recovering) {
         if (rpt->ReportSNO_) // 正常回報.
            ud->LastSNO_ = rpt->ReportSNO_;
      }
   }
   else { // if (rpt->Layout_ == NULL) // 回補結束.
      ud->LastSNO_ = rpt->ReportSNO_;
      ud->State_ = OmsRcCliSt_Recovered;
   }
}
f9OmsRc_SNO AddCheckSum_NextReqOrd(f9OmsRc_SNO next) {
   if (next == 0)
      return 1;
   return next /* + ReferenceSNO=0 */ + 1        // Request report.
       + (next + 1) + next/*ReferenceSNO*/ + 1;  // Order report.
}
f9OmsRc_SNO AddCheckSum_NextReqOrd_Times(f9OmsRc_SNO next, unsigned times) {
   f9OmsRc_SNO res = 0;
   for (unsigned L = 0; L < times; ++L) {
      res += AddCheckSum_NextReqOrd(next);
      if (next)
         next += kReqUsedSNO;
   }
   return res;
}

void OnClientFcReq(f9OmsRc_ClientSession* ses, unsigned usWait) {
   printf("OnClientFcReq|ses=%p|wait=%u us\n", ses, usWait);
   SleepMS((usWait + 999) / 1000);
}
//--------------------------------------------------------------------------//
f9OmsRc_ClientHandler cliHandler = {&OnClientLinkEv, &OnClientConfig, &OnClientReport, &OnClientFcReq};
f9OmsRc_API_FN(void) f9OmsRc_FcReqResize(f9OmsRc_ClientSession* ses, unsigned count, unsigned ms);

int OmsRcFramework_UT_C_main(int argc, char** argv) {
   (void)argc; (void)argv;

   f9OmsRc_Initialize(NULL);

   f9OmsRc_ClientSessionParams   sesParams;
   memset(&sesParams, 0, sizeof(sesParams));
   sesParams.Handler_ = &cliHandler;
   sesParams.DevName_ = "TcpClient";
   sesParams.LogFlags_ = f9OmsRc_ClientLogFlag_Link
      //| f9OmsRc_ClientLogFlag_Config
      //| f9OmsRc_ClientLogFlag_Report | f9OmsRc_ClientLogFlag_Request
      ;

   OmsRcMyself udAdmin;
   memset(&udAdmin, 0, sizeof(udAdmin));
   sesParams.UserId_ = "admin";
   sesParams.Password_ = "AdminPass";
   sesParams.DevParams_ = "dn=localhost:6601";
   sesParams.UserData_ = &udAdmin;
   f9OmsRc_CreateSession(&udAdmin.Session_, &sesParams);

   OmsRcMyself udFonwin;
   memset(&udFonwin, 0, sizeof(udFonwin));
   sesParams.UserId_ = "fonwin";
   sesParams.Password_ = "FonPass";
   sesParams.DevParams_ = "127.0.0.1:6601";
   sesParams.UserData_ = &udFonwin;
   f9OmsRc_CreateSession(&udFonwin.Session_, &sesParams);
   //---------------------------------------------
   // 等候回報回補結束.
   unsigned waitTimes = 0;
   while (udAdmin.State_ <= OmsRcCliSt_Recovering || udFonwin.State_ <= OmsRcCliSt_Recovering) {
      SleepMS(1);
      if (++waitTimes % 1000 == 0)
         puts("Waiting connect or recover.");
   }
   // 因為 Admin, Fonwin 收到的回報內容不同,
   // 所以回補後, 同步兩者之後, 才做後續的測試.
   udAdmin.CheckSum_ = udFonwin.CheckSum_ = 0;
   //------------------------------------------------
   #define PUT_FIELD_VAL(vect, fldIndex, val)       \
      if (fldIndex >= 0) {                          \
         vect[fldIndex].End_ = (vect[fldIndex].Begin_ = val) + sizeof(val) - 1; \
      }
   //------------------------------------------------
   // 填妥大部分的下單欄位.
   const f9OmsRc_Layout* reqNew = udAdmin.ReqNew_;
   fon9_CStrView*        fldValues = udAdmin.ReqNewValues_;
   //PUT_FIELD_VAL(fldValues, reqNew->IdxKind_,       "");
   PUT_FIELD_VAL(fldValues, reqNew->IdxMarket_,     "T");
   PUT_FIELD_VAL(fldValues, reqNew->IdxSessionId_,  "N");
   PUT_FIELD_VAL(fldValues, reqNew->IdxBrkId_,      "8610");
   //PUT_FIELD_VAL(fldValues, reqNew->IdxSubacNo_,    "");
   //PUT_FIELD_VAL(fldValues, reqNew->IdxIvacNoFlag_, "");
   PUT_FIELD_VAL(fldValues, reqNew->IdxUsrDef_,     "UD001");
   PUT_FIELD_VAL(fldValues, reqNew->IdxSide_,       "B");
   PUT_FIELD_VAL(fldValues, reqNew->IdxOType_,      "0");
   PUT_FIELD_VAL(fldValues, reqNew->IdxSymbol_,     "2317");
   PUT_FIELD_VAL(fldValues, reqNew->IdxPriType_,    "L");
   PUT_FIELD_VAL(fldValues, reqNew->IdxPri_,        "200");
   PUT_FIELD_VAL(fldValues, reqNew->IdxQty_,        "8000");
   //PUT_FIELD_VAL(fldValues, reqNew->IdxOrdNo_,      "");
   //---------------------------------------------
   f9OmsRc_SNO    expectedSNO = udAdmin.LastSNO_;
   unsigned       clOrdIdNum = udAdmin.LastClOrdId_;
   fon9_CStrView* clOrdIdStr = &fldValues[reqNew->IdxClOrdId_];
   char           clOrdIdBuf[64];
   clOrdIdStr->Begin_ = clOrdIdBuf;
   // Admin 送一筆 Fonwin 可以收到的回報.
   printf("[TEST ] Admin send, Fonwin recv.");
   clOrdIdStr->End_ = clOrdIdStr->Begin_ + sprintf(clOrdIdBuf, "%u", ++clOrdIdNum);
   PUT_FIELD_VAL(fldValues, reqNew->IdxIvacNo_, "10");
   f9OmsRc_SendRequestFields(udAdmin.Session_, reqNew, fldValues);
   WaitLastSNO2(&udAdmin, &udFonwin, expectedSNO += kReqUsedSNO);
   printf("\r[OK   ]\n");

   // Admin 送一筆 Fonwin 收不到的回報.
   printf("[TEST ] Admin send, Fonwin cannot recv.");
   udFonwin.CheckSum_ += AddCheckSum_NextReqOrd(expectedSNO + 1);
   clOrdIdStr->End_ = clOrdIdStr->Begin_ + sprintf(clOrdIdBuf, "%u", ++clOrdIdNum);
   PUT_FIELD_VAL(fldValues, reqNew->IdxIvacNo_, "32");
   f9OmsRc_SendRequestFields(udAdmin.Session_, reqNew, fldValues);
   WaitLastSNO(&udAdmin, expectedSNO += kReqUsedSNO);
   // if(udAdmin.CheckSum_ != udFonwin.CheckSum_) 等到下一個測試時, 再一並檢查 CheckSum.
   printf("\r[OK   ]\n");

   // Fonwin 送一筆 Admin 沒有的帳號, 但因 Admin 可用帳號有 "*", 所以可以收到.
   printf("[TEST ] Fonwin send, Admin recv.");
   clOrdIdStr->End_ = clOrdIdStr->Begin_ + sprintf(clOrdIdBuf, "%u", ++clOrdIdNum);
   PUT_FIELD_VAL(fldValues, reqNew->IdxIvacNo_, "21");
   f9OmsRc_SendRequestFields(udFonwin.Session_, reqNew, fldValues);
   WaitLastSNO2(&udAdmin, &udFonwin, expectedSNO += kReqUsedSNO);
   printf("\r[OK   ]\n");
   //---------------------------------------------
   const unsigned kFcBuf = (udFonwin.Config_->FcReqCount_ / 10);
   const unsigned kFcCount = (kFcBuf <= 0 ? 1 : kFcBuf) + udFonwin.Config_->FcReqCount_;
   const unsigned kFcOverCount = 3;
   unsigned times;
   // 測試超過流量: API 等候.
   puts("Test FlowControl.");
   for (times = 0; times < kFcCount + kFcOverCount; ++times) {
      clOrdIdStr->End_ = clOrdIdStr->Begin_ + sprintf(clOrdIdBuf, "%u", ++clOrdIdNum);
      f9OmsRc_SendRequestFields(udFonwin.Session_, reqNew, fldValues);
   }
   WaitLastSNO2(&udAdmin, &udFonwin, expectedSNO += times * kReqUsedSNO);

   // 測試強制超過流量.
   puts("Waiting test FlowControl.");
   f9OmsRc_FcReqResize(udFonwin.Session_, 0, 0);
   SleepMS(udFonwin.Config_->FcReqMS_);
   f9OmsRc_SNO chk = udFonwin.CheckSum_
                   + AddCheckSum_NextReqOrd_Times(expectedSNO + 1, kFcCount)
                   + AddCheckSum_NextReqOrd_Times(0, kFcOverCount);
   for (times = 0; times < kFcCount + kFcOverCount; ++times) {
      clOrdIdStr->End_ = clOrdIdStr->Begin_ + sprintf(clOrdIdBuf, "%u", ++clOrdIdNum);
      f9OmsRc_SendRequestFields(udFonwin.Session_, reqNew, fldValues);
   }
   SleepMS(100); // 等候 abandon request 的 log 輸出.

   printf("[TEST ] Fonwin FlowControl.");
   WaitLastSNO(&udFonwin, expectedSNO += kFcCount * kReqUsedSNO);
   while (chk != udFonwin.CheckSum_) {
      SleepMS(1);
      if (++waitTimes % 1000 == 0)
         puts("Waiting report: abandon request.");
   }
   printf("\r[OK   ]\n");

   // 測試超過流量: force logout.
   for (times = 0; times < kFcCount + kFcOverCount; ++times) {
      clOrdIdStr->End_ = clOrdIdStr->Begin_ + sprintf(clOrdIdBuf, "%u", ++clOrdIdNum);
      f9OmsRc_SendRequestFields(udFonwin.Session_, reqNew, fldValues);
   }
   //---------------------------------------------
   // puts("OK, Wait enter..."); getchar();
   f9OmsRc_DestroySession(udAdmin.Session_, 1);
   f9OmsRc_DestroySession(udFonwin.Session_, 1);
   printf("LastSNO: admin=%u, fonwin=%u\n", (unsigned)udAdmin.LastSNO_, (unsigned)udFonwin.LastSNO_);
   // puts("Wait enter again..."); getchar();

   f9OmsRc_Finalize();
   free(udAdmin.ReqNewValues_);
   free(udFonwin.ReqNewValues_);
   return 0;
}
