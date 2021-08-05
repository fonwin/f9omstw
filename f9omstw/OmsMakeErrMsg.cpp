// \file f9omstw/OmsMakeErrMsg.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsMakeErrMsg.h"
#include "fon9/LevelArray.hpp"
#include "fon9/ConfigLoader.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/Raw.hpp"
#include "fon9/Log.hpp"

#ifdef __cplusplus
extern "C" {
#endif

struct f9omsTxMsgWithFields : public fon9::seed::Tab {
   fon9_NON_COPY_NON_MOVE(f9omsTxMsgWithFields);
   using base = fon9::seed::Tab;
   using base::base;
   std::unique_ptr<fon9::seed::DyRaw> Raw_;
};
using f9omsTxMsgWithFieldsSP = fon9::intrusive_ptr<f9omsTxMsgWithFields>;

struct f9omsTxMsg : public fon9::CharVector {
   f9omsTxMsgWithFieldsSP  Tab_;
};
using OmsErrCodeTxMsg = fon9::LevelArray<OmsErrCode, f9omsTxMsg>;

struct f9omstw_ErrCodeTx {
   fon9_NON_COPY_NON_MOVE(f9omstw_ErrCodeTx);
   f9omstw_ErrCodeTx() = default;
   OmsErrCodeTxMsg   OmsErrCodeTxMsg_;
};

static const f9omstw_ErrCodeTx* LoadOmsErrMsgTx(fon9::StrView cfgstr, const fon9::StrView lang) {
   f9omstw_ErrCodeTx* retval = new f9omstw_ErrCodeTx;
   fon9::ConfigLoader cfgldr{std::string{}};
   try {
      cfgldr.CheckLoad(cfgstr);
   }
   catch (fon9::ConfigLoader::Err& e) {
      fon9_LOG_ERROR("LoadOmsErrMsgTx|", e);
      return nullptr;
   }
   cfgstr = &cfgldr.GetCfgStr();
   fon9::StrView msg1st;
   std::string   flddesc;
   while (!fon9::StrTrimHead(&cfgstr).empty()) {
      fon9::StrView ecstr = fon9::StrFetchTrim(cfgstr, '=');
      OmsErrCode    ec = static_cast<OmsErrCode>(fon9::StrTo(ecstr, 0u));
      if (fon9::StrTrimHead(&cfgstr).Get1st() == '{') {
         cfgstr.SetBegin(cfgstr.begin() + 1);
         ecstr = fon9::SbrFetchNoTrim(cfgstr, '}');
         while (!fon9::StrTrimHead(&ecstr).empty()) { // en: ...\n
            fon9::StrView lnstr = fon9::StrFetchTrim(ecstr, '\n');
            fon9::StrView lnhdr = fon9::StrFetchTrim(lnstr, ':');
            if (lnstr.empty()) {
               lnstr = lnhdr;
               lnhdr.Reset(nullptr);
            }
            else
               fon9::StrTrim(&lnstr);
            if (lang.empty() || lang == lnhdr) {
               ecstr = lnstr;
               goto __PARSE_MSG;
            }
            if (msg1st.empty())
               msg1st = lnstr;
         }
         ecstr = msg1st;
      }
      else { // en | zh
         ecstr = fon9::StrFetchTrim(cfgstr, '\n');
         msg1st = fon9::SbrFetchNoTrim(ecstr, '|');
         if (lang.empty() || lang == "en" || fon9::StrTrimHead(&ecstr).empty())
            ecstr = msg1st;
         fon9::StrTrim(&ecstr);
      }
   __PARSE_MSG:;
      if (ec == OmsErrCode_NoError || ecstr.empty())
         continue;
      // 解析 ecstr 是否需要格式化輸出:
      // - 無格式 {:FieldName:} 建立訊息時, 直接複製 OMS 提供的字串.
      // - 型別及輸出格式 {:FieldName:Type:Format:}
      f9omsTxMsg& tx = retval->OmsErrCodeTxMsg_[ec];
      char*       pdst = static_cast<char*>(tx.alloc(ecstr.size()));
      const char* psrc = ecstr.begin();

      using namespace fon9::seed;
      Fields      flds;
      const char* pFldName = nullptr;
      for (;;) {
         *pdst++ = *psrc++;
         if (fon9_UNLIKELY(psrc == ecstr.end()))
            break;
         if (*psrc != ':')
            continue;
         if (pFldName == nullptr) {
            if (*(psrc - 1) == '{') // "{:fieldName:"
               pFldName = psrc + 1; //    ^ pFldName;
            continue;
         }
         if (psrc + 1 >= ecstr.end()) // "{:fieldName:" 不正確的格式.
            continue;                 //             ^ psrc;
         fon9::StrView fldName{pFldName, psrc};
         if (fon9::StrTrim(&fldName).empty()) {
            pFldName = nullptr;
            continue;
         }
         const auto  dstRewind = (psrc - pFldName + 1);
         FieldSP     fld;
         if (psrc[1] == '}') {
            // "{:fieldName:}" 無格式(只簡單複製原始訊息的 value), 所以只需要欄位名稱.
            // 因為在建立輸出訊息時, 是用 fld.Description.empty(); 來決定: 是否直接取原始訊息的 value;
            // 所以這裡任意型別皆可.
            fld.reset(new FieldChar1(fon9::Named{fldName.ToString()}, 0));
         }
         else { // {:FieldName:...} 是否為 {:FieldName:Type:Format:} ?
            flddesc.clear();
            const char* pfmt = psrc + 1;
            const char* pend = ecstr.end();
            for (; pfmt != pend; ++pfmt) {
               if (*pfmt != ':') {
                  flddesc.push_back(*pfmt);
                  continue;
               }
               if (!fldName.empty()) {
                  // {:FieldName:Type:Format:}
                  //                 ^ pfmt;
                  // 此時: flddesc = "Type";
                  // 底下: flddesc = "Type Name||";
                  // 之後: pfmt    = Format(當成 field Description); 填入 flddesc 尾端;
                  flddesc.push_back(' ');
                  fldName.AppendTo(flddesc);
                  flddesc.push_back('|');
                  flddesc.push_back('|');
                  fldName.Reset(nullptr);
                  // 如果 Description 有括號, 則使用括號內容.
                  fon9::StrView tstr{pfmt + 1, pend};
                  fon9::StrView indesc = fon9::SbrTrimHeadFetchInside(tstr);
                  if (indesc.IsNull()) // Description 開頭沒有括號, 則剩餘的內容為「: Description :}」.
                     continue;
                  // {indesc} ...dont care... :}
                  //         ^ tstr.begin();
                  pfmt = static_cast<const char*>(memchr(tstr.begin(), ':', pend - tstr.begin()));
                  if (pfmt == nullptr || pfmt[1] != '}')
                     break;
                  // 已找到格式的結尾, 可直接 MakeField();
                  flddesc.append(indesc.begin() - 1, tstr.begin());
                  goto __MAKE_FIELD;
               }
               // "Format:}"
               //        ^ pfmt;
               if (pfmt + 1 >= pend)
                  break;
               if (pfmt[1] == '}') {
               __MAKE_FIELD:;
                  fldName = &flddesc;
                  if ((fld = MakeField(fldName, '|', '\0')).get() != nullptr)
                     psrc = pfmt;
                  break;
               }
               flddesc.push_back(*pfmt);
            }
         }
         if (fld) {
            flds.Add(std::move(fld));
            *((pdst -= dstRewind) - 1) = '\0';
            psrc += 2; // Skip ":}"
         }
         if (fon9_UNLIKELY(psrc == ecstr.end()))
            break;
         pFldName = nullptr;
      }
      tx.resize(static_cast<size_t>(pdst - tx.begin()));
      if (flds.size() <= 0)
         tx.Tab_.reset();
      else {
         tx.Tab_.reset(new f9omsTxMsgWithFields(fon9::Named{"ec"}, std::move(flds)));
         tx.Tab_->Raw_.reset(MakeDyMemRaw<DyRaw>(*tx.Tab_));
      }
   }
   return retval;
}
//--------------------------------------------------------------------------//
const f9omstw_ErrCodeTx* f9omstw_LoadOmsErrMsgTx(const char* cfgfn, const char* language) {
   return LoadOmsErrMsgTx(fon9::StrView_cstr(cfgfn),
                          language ? fon9::StrView_cstr(language) : fon9::StrView{nullptr});
}
const f9omstw_ErrCodeTx* f9omstw_LoadOmsErrMsgTx1(const char* cfg) {
   fon9::StrView lang{fon9::StrView_cstr(cfg)};
   fon9::StrView cfgstr = fon9::StrFetchTrim(lang, ':');
   return LoadOmsErrMsgTx(cfgstr, fon9::StrTrim(&lang));
}
void f9omstw_FreeOmsErrMsgTx(const f9omstw_ErrCodeTx* tx) {
   delete tx;
}
//--------------------------------------------------------------------------//
fon9_CStrView f9omstw_MakeErrMsg(const f9omstw_ErrCodeTx* txRes,
                                 char* outbuf, unsigned buflen,
                                 OmsErrCode ec, fon9_CStrView orgmsg) {
   if (!txRes)
      return orgmsg;
   const auto* ptx = txRes->OmsErrCodeTxMsg_.Get(ec);
   if (!ptx || ptx->empty() || (ptx->size() == 1 && *ptx->begin() == '-'))
      return orgmsg;
   if (fon9_LIKELY(!ptx->Tab_ || buflen <= ptx->size()))
      return ToStrView(*ptx).ToCStrView();

   // 解析 OMS的訊息: 將欄位資料存放於 values;
   using TagValues = fon9::SortedVector<fon9::StrView, fon9::StrView>;
   TagValues     values;
   fon9::StrView orig{orgmsg};
   fon9::StrView tag, val;
   while(fon9::StrFetchTagValue(orig, tag, val))
      values.kfetch(tag).second = val;

   fon9::RevBufferFixedMem rbuf{outbuf, buflen};
   char* pout = rbuf.AllocPrefix(1);
   *--pout = '\0'; // outbuf 必須包含 EOS.
   try {
      const char* txbeg = ptx->begin();
      const char* txend = ptx->end();
      size_t      fldn = ptx->Tab_->Fields_.size();
      while (txbeg != txend) {
         if ((*--pout = *--txend) != '\0')
            continue;
         const auto* fld = ptx->Tab_->Fields_.Get(--fldn);
         if (fld == nullptr) {
            *pout = '*';
            continue;
         }
         rbuf.SetPrefixUsed(pout + 1);
         auto ival = values.find(&fld->Name_);
         if (ival == values.end()) // OMS的訊息字串裡面, 沒有提供此欄位.
            fon9::RevPrint(rbuf, "{:", fld->Name_, ":}");
         else if (fld->GetDescription().empty()) // 沒有格式設定.
            fon9::RevPrint(rbuf, ival->second);
         else {
            fon9::seed::SimpleRawWr wr{*ptx->Tab_->Raw_};
            fld->StrToCell(wr, ival->second);
            fld->CellRevPrint(wr, &fld->GetDescription(), rbuf);
         }
         pout = rbuf.AllocPrefix(static_cast<size_t>(txend - txbeg));
      }
   }
   catch (fon9::BufferOverflow&) {
      return orgmsg;
   }
   // outbuf 包含 EOS, 但返回值的 End_ 不包含.
   return fon9::StrView{pout, rbuf.GetMemEnd() - 1}.ToCStrView();
}

#ifdef __cplusplus
}
#endif
