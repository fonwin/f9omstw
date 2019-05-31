// \file f9omstw/OmsBackendRecorder.cpp
//
// OMS 啟動時載入.
// OMS 異動寫檔.
//
// \author fonwinz@gmail.com
#include "f9omstw/OmsBackend.hpp"
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsTools.hpp"
#include "f9omstw/OmsIvSymb.hpp" // for OmsScResource{}

#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/buffer/FwdBufferList.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/Log.hpp"

namespace f9omstw {

#define f9omstw_kCSTR_RequestAbandonHeader   "E:"

void OmsRequestBase::RevPrint(fon9::RevBuffer& rbuf) const {
   if (auto* abres = this->AbandonReason())
      fon9::RevPrint(rbuf, fon9_kCSTR_CELLSPL f9omstw_kCSTR_RequestAbandonHeader, *abres);
   RevPrintFields(rbuf, *this->Creator_, fon9::seed::SimpleRawRd{*this});
}
void OmsOrderRaw::RevPrint(fon9::RevBuffer& rbuf) const {
   fon9::RevPrint(rbuf, *fon9_kCSTR_CELLSPL, this->Request_->RxSNO());
   RevPrintFields(rbuf, *this->Order_->Creator_, fon9::seed::SimpleRawRd{*this});
}
//--------------------------------------------------------------------------//
void OmsBackend::SaveQuItems(QuItems& quItems) {
   fon9::DcQueueList dcq;
   for (QuItem& qi : quItems) {
      if (auto exsz = fon9::CalcDataSize(qi.ExLog_.cfront()))
         fon9::RevPrint(qi.ExLog_, *fon9_kCSTR_CELLSPL, exsz + 1, *fon9_kCSTR_CELLSPL);
      if (qi.Item_) {
         fon9::RevPrint(qi.ExLog_, *fon9_kCSTR_ROWSPL);
         qi.Item_->RevPrint(qi.ExLog_);
         fon9::RevPrint(qi.ExLog_, qi.Item_->RxSNO_, *fon9_kCSTR_CELLSPL);
      }
      dcq.push_back(qi.ExLog_.MoveOut());
   }
   if (!dcq.empty())
      this->RecorderFd_.Append(dcq);
}

//--------------------------------------------------------------------------//

struct OmsBackend::Loader {
   fon9_NON_COPY_NON_MOVE(Loader);

   struct FieldsRec;
   using FieldList = std::vector<const fon9::seed::Field*>;
   typedef const char* (Loader::*FnMakeRequest)(fon9::StrView ln, const FieldsRec& flds);
   struct FieldsRec {
      FieldList         Fields_;
      std::string       CfgLine_;
      fon9::seed::Tab*  Factory_{nullptr};
      FnMakeRequest     FnMaker_{nullptr};
   };

   static void StrToFields(const FieldList& flds, const fon9::seed::RawWr& wr, fon9::StrView& values) {
      for (const auto* fld : flds)
         fld->StrToCell(wr, fon9::StrFetchNoTrim(values, *fon9_kCSTR_CELLSPL));
   }
   const char* MakeRequest(fon9::StrView ln, const FieldsRec& flds) {
      OmsRequestFactory* reqfac = static_cast<OmsRequestFactory*>(flds.Factory_);
      auto  req = reqfac->MakeRequest(fon9::TimeStamp{});
      req->RxSNO_ = this->LastSNO_;
      StrToFields(flds.Fields_, fon9::seed::SimpleRawWr{*req}, ln);
      if (ln.size() >= sizeof(f9omstw_kCSTR_RequestAbandonHeader)
      && memcmp(ln.begin(), f9omstw_kCSTR_RequestAbandonHeader, sizeof(f9omstw_kCSTR_RequestAbandonHeader) - 1) == 0) {
         req->Abandon(std::string{ln.begin() + sizeof(f9omstw_kCSTR_RequestAbandonHeader) - 1, ln.end()});
      }
      this->Items_.AppendHistory(*req);
      return nullptr;
   }
   const char* MakeOrder(fon9::StrView ln, const FieldsRec& flds) {
      OmsOrderFactory* ordfac = static_cast<OmsOrderFactory*>(flds.Factory_);
      using namespace fon9; // for memrchr();
      const char* pReqFrom = reinterpret_cast<const char*>(memrchr(ln.begin(), *fon9_kCSTR_CELLSPL, ln.size()));
      if (pReqFrom == nullptr)
         return "MakeOrder:ReqFrom tag not found.";
      OmsRxSNO    snoReqFrom = fon9::StrTo(fon9::StrView{pReqFrom + 1, ln.end()}, OmsRxSNO{0});
      const auto* reqFrom = this->Items_.GetRequest(snoReqFrom);
      if (reqFrom == nullptr)
         return "MakeOrder:Request not found.";
      OmsOrderRaw* ord;
      if (const auto* fldIniSNO = reqFrom->Creator_->Fields_.Get("IniSNO")) {
         auto iniSNO = static_cast<OmsRxSNO>(fldIniSNO->GetNumber(fon9::seed::SimpleRawRd{*reqFrom}, 0, 0));
         const auto* reqIni = this->Items_.GetRequest(iniSNO);
         if (reqIni == nullptr)
            return "MakeOrder:Ini request not found.";
         const auto* lastupd = reqIni->LastUpdated();
         if (lastupd == nullptr)
            return "MakeOrder:Ini request no updated.";;
         ord = ordfac->MakeOrderRaw(*lastupd->Order_, *reqFrom);
      }
      else if (const auto* reqIni = dynamic_cast<const OmsRequestIni*>(reqFrom)) {
         ord = ordfac->MakeOrder(*const_cast<OmsRequestIni*>(reqIni), nullptr);
      }
      else
         return "MakeOrder:ReqFrom is not RequestIni.";
      ord->RxSNO_ = this->LastSNO_;
      reqFrom->LastUpdated_ = ord;
      StrToFields(flds.Fields_, fon9::seed::SimpleRawWr{*ord}, ln);
      this->Items_.AppendHistory(*ord);
      if ((*ord->OrdNo_.begin() != '\0') && (ord->Prev_ == nullptr || ord->Prev_->OrdNo_ != ord->OrdNo_)) {
         if (OmsBrk* brk = ord->Order_->GetBrk(this->Resource_))
            if (OmsOrdNoMap* ordNoMap = brk->GetOrdNoMap(*ord->Order_->Initiator_))
               if (!ordNoMap->EmplaceOrder(*ord)) {
                  return "MakeOrder:OrdNo exists.";
               }
      }
      return nullptr;
   }

   using FieldMap = std::map<fon9::CharVector, FieldsRec>;
   FieldMap       FieldMap_;
   OmsResource&   Resource_;
   ItemsImpl&     Items_;
   OmsRxSNO       LastSNO_{0};
   Loader(OmsResource& resource, ItemsImpl& items)
      : Resource_(resource)
      , Items_(items) {
   }

   void SetFieldMap(fon9::StrView ln) {
      auto  tag = fon9::StrFetchNoTrim(ln, *fon9_kCSTR_CELLSPL);
      auto& flds = this->FieldMap_[fon9::CharVector{tag}];
      flds.CfgLine_.assign(tag.begin(), ln.end());
      flds.Fields_.clear();
      if (flds.Factory_ == nullptr) {
         if (auto* ordfac = this->Resource_.OrderFacPark_->GetFactory(tag)) {
            flds.Factory_ = ordfac;
            flds.FnMaker_ = &Loader::MakeOrder;
         }
         else if (auto* reqfac = this->Resource_.RequestFacPark_->GetFactory(tag)) {
            flds.Factory_ = reqfac;
            flds.FnMaker_ = &Loader::MakeRequest;
         }
         else
            return;
      }
      while (!ln.empty()) {
         tag = fon9::StrFetchNoTrim(ln, *fon9_kCSTR_CELLSPL);
         fon9::StrFetchNoTrim(tag, ' '); // 返回 TypeId, 不理會; tag 剩餘 fieldName;
         if (auto* fld = flds.Factory_->Fields_.Get(tag))
            flds.Fields_.push_back(fld);
      }
   }
   void FeedLine(fon9::StrView ln, const size_t lnpos) {
      const int ch = ln.Get1st();
      if (fon9::isalpha(ch))
         return this->SetFieldMap(ln);
      if (!fon9::isdigit(ch))
         return;
      OmsRxSNO sno = fon9::StrTo(&ln, OmsRxSNO{0});
      if (sno <= this->LastSNO_) {
         fon9_LOG_ERROR("OmsBackend.Load|pos=", lnpos, "|sno=", sno, "|last=", this->LastSNO_, "|err=dup sno?");
         return;
      }
      this->LastSNO_ = sno;
      ln.SetBegin(ln.begin() + 1);

      auto  tag = fon9::StrFetchNoTrim(ln, *fon9_kCSTR_CELLSPL);
      auto  ifind = this->FieldMap_.find(fon9::CharVector::MakeRef(tag));
      if (ifind != this->FieldMap_.end()) {
         const auto& flds = ifind->second;
         if (flds.FnMaker_) {
            if (const char* err = (this->*flds.FnMaker_)(ln, flds))
               fon9_LOG_ERROR("OmsBackend.Load|pos=", lnpos, "|err=", err);
         }
      }
   }

   void MakeLayout(fon9::RevBufferList& rbuf, fon9::seed::Layout& layout) {
      size_t idx = layout.GetTabCount();
      while (idx > 0) {
         fon9::RevBufferList rtmp{256};
         auto* tab = layout.GetTab(--idx);
         RevPrintTabFieldNames(rtmp, *tab);
         std::string cfgstr = fon9::BufferTo<std::string>(rtmp.MoveOut());
         auto ifind = this->FieldMap_.find(fon9::CharVector::MakeRef(tab->Name_));
         if (ifind == this->FieldMap_.end() || ifind->second.CfgLine_ != cfgstr)
            fon9::RevPrint(rbuf, cfgstr, '\n');
      }
   }
};

OmsBackend::OpenResult OmsBackend::OpenReload(std::string logFileName, OmsResource& resource) {
   assert(!this->RecorderFd_.IsOpened());
   if (this->RecorderFd_.IsOpened())
      return OpenResult{0};
   auto res = this->RecorderFd_.Open(logFileName,
                                     fon9::FileMode::CreatePath | fon9::FileMode::Append
                                     | fon9::FileMode::Read | fon9::FileMode::DenyWrite);
   if (res.IsError()) {
   __OPEN_ERROR:
      this->RecorderFd_.Close();
      fon9_LOG_FATAL("OmsBackend.OpenReload|file=", logFileName, '|', res);
      return res;
   }
   res = this->RecorderFd_.GetFileSize();
   if (res.IsError())
      goto __OPEN_ERROR;

   Locker      items{this->Items_};
   Loader      loader{resource, *items};
   const auto  fsize = res.GetResult();
   if (fsize > 0) {
      fon9::LinePeeker     lnPeeker;
      fon9::DcQueueList    rxbuf;
      fon9::File::SizeType fpos = 0, lnpos = 0;

      for (;;) {
         fon9::FwdBufferNode* node = fon9::FwdBufferNode::Alloc(32 * 1024);
         res = this->RecorderFd_.Read(fpos, node->GetDataEnd(), node->GetRemainSize());
         if (res.IsError()) {
            FreeNode(node);
            fon9_LOG_FATAL("OmsBackend.OpenReload.Read|pos=", fpos);
            goto __OPEN_ERROR;
         }
         if (res.GetResult() <= 0)
            break;
         fpos += res.GetResult();
         node->SetDataEnd(node->GetDataEnd() + res.GetResult());
         rxbuf.push_back(node);
         size_t exsz = 0;
         while (const char* pln = lnPeeker.PeekUntil(rxbuf, *fon9_kCSTR_ROWSPL)) {
            fon9::StrView ln{pln, lnPeeker.LineSize_ - 1}; // -1 移除 *fon9_kCSTR_ROWSPL;
            if (*pln != *fon9_kCSTR_CELLSPL) {
               loader.FeedLine(ln, lnpos);
               lnpos += lnPeeker.LineSize_;
               lnPeeker.PopConsumed(rxbuf);
            }
            else {
               ln.SetBegin(pln + 1);
               exsz = fon9::StrTo(&ln, 0u);
               exsz += ln.begin() - pln;
               lnpos += exsz;
               exsz -= lnPeeker.TmpBuf_.size();
               lnPeeker.Clear();
               size_t cursz = rxbuf.CalcSize();
               if (cursz <= exsz) {
                  rxbuf.PopConsumed(cursz);
                  exsz -= cursz;
                  break;
               }
               rxbuf.PopConsumed(exsz);
               exsz = 0;
            }
         } // while lnPeeker
         if ((fpos += exsz) >= fsize)
            break;
      } // for() read block.

      this->LastSNO_ = loader.LastSNO_;
   }
   fon9::RevBufferList rbuf{128};
   loader.MakeLayout(rbuf, *resource.RequestFacPark_);
   loader.MakeLayout(rbuf, *resource.OrderFacPark_);
   fon9::RevPrint(rbuf, "===== OMS start @ ", fon9::UtcNow(), " | ", logFileName, " =====\n");
   if (fsize > 0)
      fon9::RevPrint(rbuf, '\n');
   fon9::DcQueueList dcq{rbuf.MoveOut()};
   this->RecorderFd_.Append(dcq);
   return res;
}

} // namespaces
