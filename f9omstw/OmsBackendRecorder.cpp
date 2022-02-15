// \file f9omstw/OmsBackendRecorder.cpp
//
// OMS 啟動時載入.
// OMS 異動寫檔.
//
// \author fonwinz@gmail.com
#include "f9omstw/OmsBackend.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9omstw/OmsOrder.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsTools.hpp"
#include "f9omstw/OmsIvSymb.hpp" // for OmsScResource{}

#include "fon9/FileReadAll.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/Log.hpp"
#include "fon9/FilePath.hpp"

namespace f9omstw {

#define f9omstw_kCSTR_RequestAbandonHeader   "E:"

void OmsRequestBase::RevPrint(fon9::RevBuffer& rbuf) const {
   if (fon9_UNLIKELY(this->IsAbandoned())) {
      if (this->AbandonReason_)
         fon9::RevPrint(rbuf, ':', *this->AbandonReason_);
      fon9::RevPrint(rbuf, fon9_kCSTR_CELLSPL f9omstw_kCSTR_RequestAbandonHeader, this->ErrCode_);
   }
   RevPrintFields(rbuf, *this->Creator_, fon9::seed::SimpleRawRd{*this});
}
void OmsOrderRaw::RevPrint(fon9::RevBuffer& rbuf) const {
   fon9::RevPrint(rbuf, *fon9_kCSTR_CELLSPL, this->Request_->RxSNO());
#ifdef _DEBUG
   fon9::RevPrint(rbuf, *fon9_kCSTR_CELLSPL, this->UpdSeq());
#endif
   RevPrintFields(rbuf, this->Order_->Creator(), fon9::seed::SimpleRawRd{*this});
}
//--------------------------------------------------------------------------//
void OmsBackend::SaveQuItems(QuItems& quItems) {
   fon9::DcQueueList dcq;
   for (QuItem& qi : quItems) {
      if (fon9_UNLIKELY(qi.Item_ && qi.Item_->RxSNO() == 0)) {
         qi.Item_->RevPrint(qi.ExLog_);
         fon9::RevPrint(qi.ExLog_, "NoRecover:");
      }
      if (auto exsz = fon9::CalcDataSize(qi.ExLog_.cfront()))
         fon9::RevPrint(qi.ExLog_, *fon9_kCSTR_CELLSPL, exsz + 1, *fon9_kCSTR_CELLSPL);
      if (qi.Item_ && qi.Item_->RxSNO() != 0) {
         fon9::RevPrint(qi.ExLog_, *fon9_kCSTR_ROWSPL);
         qi.Item_->RevPrint(qi.ExLog_);
         fon9::RevPrint(qi.ExLog_, qi.Item_->RxSNO(), *fon9_kCSTR_CELLSPL);
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
      if (req.get() == nullptr) {
         req = reqfac->MakeReportIn(f9fmkt_RxKind_Unknown, fon9::TimeStamp{});
         if (req.get() == nullptr)
            return "MakeRequest:Cannot make request or report.";
      }
      req->RxSNO_ = this->LastSNO_;
      StrToFields(flds.Fields_, fon9::seed::SimpleRawWr{*req}, ln);
      if (ln.size() >= sizeof(f9omstw_kCSTR_RequestAbandonHeader)
      && memcmp(ln.begin(), f9omstw_kCSTR_RequestAbandonHeader, sizeof(f9omstw_kCSTR_RequestAbandonHeader) - 1) == 0) {
         ln.SetBegin(ln.begin() + sizeof(f9omstw_kCSTR_RequestAbandonHeader) - 1);
         OmsErrCode ec = static_cast<OmsErrCode>(fon9::StrTo(&ln, fon9::underlying_type_t<OmsErrCode>{0}));
         if (ln.size() <= 1)
            req->Abandon(ec);
         else
            req->Abandon(ec, std::string(ln.begin() + 1, ln.end()));
      }
      this->Items_.AppendHistory(*req);
      return nullptr;
   }
   const char* MakeOrder(fon9::StrView ln, const FieldsRec& flds) {
      using namespace fon9; // for memrchr(); 取出最後一個 value = 委託異動來源的 RxSNO(ReqFrom = Request).
      const char* pReqFrom = reinterpret_cast<const char*>(memrchr(ln.begin(), *fon9_kCSTR_CELLSPL, ln.size()));
      if (pReqFrom == nullptr)
         return "Loader.MakeOrder: ReqFrom tag not found.";
      OmsRxSNO snoReqFrom = fon9::StrTo(fon9::StrView{pReqFrom + 1, ln.end()}, OmsRxSNO{0});
      auto*    reqFrom = const_cast<OmsRequestBase*>(this->Items_.GetRequest(snoReqFrom));
      if (reqFrom == nullptr)
         return "Loader.MakeOrder: Request not found.";
      // ln 讀入的可能是 reqFrom 的首次或後續異動.
      OmsOrderRaw*       ordraw = nullptr;
      OmsOrder*          order;
      const OmsOrderRaw* lastupd = reqFrom->LastUpdated();
      if (lastupd)
         order = &lastupd->Order();
      else { // ln = reqFrom 的首次異動 => order 的首次異動? 或後續異動?
         if (const auto* fldIniSNO = reqFrom->Creator_->Fields_.Get("IniSNO")) {
            auto iniSNO = static_cast<OmsRxSNO>(fldIniSNO->GetNumber(fon9::seed::SimpleRawRd{*reqFrom}, 0, 0));
            if (iniSNO == 0)
               // 有 IniSNO 欄位, 但 IniSNO=0:
               // 可能因回報亂序, 尚未收到 Initiator 之前的刪改查成交.
               goto __GET_ORDER_BY_ORDKEY;
            const auto* reqIni = this->Items_.GetRequest(iniSNO);
            if (reqIni == nullptr)
               return "Loader.MakeOrder: Ini request not found.";
            if ((lastupd = reqIni->LastUpdated()) == nullptr)
               return "Loader.MakeOrder: Ini request no updated.";
            order = &lastupd->Order();
         }
         else {
      __GET_ORDER_BY_ORDKEY:
            if ((order = reqFrom->SearchOrderByOrdKey(this->Resource_)) == nullptr) {
               assert(lastupd == nullptr);
               ordraw = static_cast<OmsOrderFactory*>(flds.Factory_)->MakeOrder(*reqFrom, nullptr);
               order = &ordraw->Order();
            }
         }
      }
      if (ordraw == nullptr) {
         assert(order != nullptr);
         lastupd = order->Tail();
         ordraw = order->BeginUpdate(*reqFrom);
      }
      ordraw->SetRxSNO(this->LastSNO_);
      reqFrom->SetLastUpdated(*ordraw);
      StrToFields(flds.Fields_, fon9::seed::SimpleRawWr{*ordraw}, ln);
      this->Items_.AppendHistory(*ordraw);
      if (!OmsIsOrdNoEmpty(ordraw->OrdNo_) && (order->Head() == ordraw || lastupd->OrdNo_ != ordraw->OrdNo_)) {
         if (OmsBrk* brk = order->GetBrk(this->Resource_))
            if (OmsOrdNoMap* ordNoMap = brk->GetOrdNoMap(*reqFrom))
               if (!ordNoMap->EmplaceOrder(*ordraw)) {
                  return "Loader.MakeOrder: OrdNo exists.";
               }
      }
      order->EndUpdate(*ordraw, nullptr);
      return nullptr;
   }
   const char* MakeEvent(fon9::StrView ln, const FieldsRec& flds) {
      auto  evfac = static_cast<OmsEventFactory*>(flds.Factory_);
      auto  omsEvent = evfac->MakeReloadEvent();
      if (omsEvent.get() == nullptr)
         return "MakeEvent:Cannot make OmsEvent.";
      omsEvent->SetRxSNO(this->LastSNO_);
      StrToFields(flds.Fields_, fon9::seed::SimpleRawWr{*omsEvent}, ln);
      this->Items_.AppendHistory(*omsEvent);
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
         if (auto* ordfac = this->Resource_.Core_.Owner_->OrderFactoryPark().GetFactory(tag)) {
            flds.Factory_ = ordfac;
            flds.FnMaker_ = &Loader::MakeOrder;
         }
         else if (auto* reqfac = this->Resource_.Core_.Owner_->RequestFactoryPark().GetFactory(tag)) {
            flds.Factory_ = reqfac;
            flds.FnMaker_ = &Loader::MakeRequest;
         }
         else if (auto* evfac = this->Resource_.Core_.Owner_->EventFactoryPark().GetFactory(tag)) {
            flds.Factory_ = evfac;
            flds.FnMaker_ = &Loader::MakeEvent;
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
   fon9::File::Result ReloadAll(fon9::File& fd, const fon9::File::SizeType fsize) {
      fon9::LinePeeker     lnPeeker;
      fon9::File::SizeType fpos = 0, lnpos = 0;
      fon9::File::Result   res = fon9::FileReadAll(fd, fpos, [&](fon9::DcQueue& rxbuf, fon9::File::Result& rout){
         size_t exsz = 0;
         while (const char* pln = lnPeeker.PeekUntil(rxbuf, *fon9_kCSTR_ROWSPL)) {
            fon9::StrView ln{pln, lnPeeker.LineSize_ - 1}; // -1 移除 *fon9_kCSTR_ROWSPL;
            if (*pln != *fon9_kCSTR_CELLSPL) {
               this->FeedLine(ln, lnpos);
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
         if ((fpos += exsz) < fsize)
            return true;
         rout = fon9::File::Result{fsize};
         return false;
      });
      if (res.IsError())
         fon9_LOG_FATAL("OmsBackend.OpenReload.Read|pos=", fpos, '|', res);
      return res;
   }
   void MakeLayout(fon9::RevBufferList& rbuf, const fon9::seed::Layout& layout) {
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

OmsBackend::OpenResult OmsBackend::OpenReload(std::string logFileName, OmsResource& resource, fon9::FileMode fmode) {
   assert(!this->RecorderFd_.IsOpened());
   if (this->RecorderFd_.IsOpened())
      return OpenResult{0};
   this->LogPath_ = fon9::FilePath::ExtractPathName(&logFileName);
   auto res = this->RecorderFd_.Open(logFileName, fmode);
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
      res = loader.ReloadAll(this->RecorderFd_, fsize);
      if (res.IsError())
         goto __OPEN_ERROR;

      this->LastSNO_ = this->PublishedSNO_ = loader.LastSNO_;
      if (items->RxHistory_.size() <= loader.LastSNO_) // 可能資料檔有不認識的 factory name 造成的.
         items->RxHistory_.resize(loader.LastSNO_ + 1);// 因 RxHistory_ 使用陣列, 所以需要強制調整儲存空間, 才能直接用序號對應.
      // 依序從第1筆開始, 通知 core 重算風控 RecalcSc.
      // 因為重算過程可能與「下單要求、委託異動、事件」的發生順序有關, 所以需要「從頭、依序」檢查.
      for (OmsRxSNO sno = 1; sno <= loader.LastSNO_; ++sno) {
         auto* const icur = items->RxHistory_[sno];
         if (icur == nullptr)
            continue;
         OmsOrder*          order;
         const OmsOrderRaw* ordraw = static_cast<const OmsOrderRaw*>(icur->CastToOrder());
         if (ordraw) {
            order = &ordraw->Order();
            if (ordraw == order->Tail()) {
               // 遇到 Order 的最後一筆 OrderRaw 時, 重算風控資料.
               resource.Core_.Owner_->RecalcSc(resource, *order);
               FetchScResourceIvr(resource, *order);
            }
            if (ordraw->RequestSt_ == f9fmkt_TradingRequestSt_Filled) {
               // IOC 部分成交, 會有2筆 ordraw:
               // 第1筆為 f9fmkt_TradingRequestSt_Filled
               // 第2筆為 f9fmkt_TradingRequestSt_ExchangeCanceled
               // 必須在第一筆 ordraw 處理 InsertFilled();
               if (auto filled = dynamic_cast<const OmsReportFilled*>(&ordraw->Request())) {
                  if (fon9_LIKELY(order->InsertFilled(filled) == nullptr))
                     continue;
                  fon9_LOG_ERROR("OmsBackend.InsertFilled|filled.SNO=", sno);
               }
               fon9_LOG_ERROR("OmsBackend.NotFilled|SNO=", sno);
               continue;
            }
         }
         const OmsRequestBase* req = static_cast<const OmsRequestBase*>(icur->CastToRequest());
         if (req == nullptr) {
            if (const OmsEvent* ev = static_cast<const OmsEvent*>(icur->CastToEvent()))
               resource.Core_.Owner_->ReloadEvent(resource, *ev, items);
            continue;
         }
         if ((ordraw = req->LastUpdated()) == nullptr)
            continue;
         order = &ordraw->Order();
         if (fon9_UNLIKELY(ordraw->RequestSt_ == f9fmkt_TradingRequestSt_Queuing
                           && order->LastOrderSt() != f9fmkt_OrderSt_NewQueuingCanceled)) {
            // 若 req 最後狀態為 Queueing, 則應改成 f9fmkt_TradingRequestSt_QueuingCanceled.
            // - 發生原因: 可能因為程式沒有正常結束: crash? kill -9?
            // - 不能強迫設定 const_cast<OmsOrderRaw*>(ordraw)->RequestSt_ = f9fmkt_TradingRequestSt_QueuingCanceled;
            //   應使用正常更新方式處理, 否則訂閱端(如果有保留最後 RxSNO), 可能會不知道該筆要求變成 QueuingCanceled.
            items.unlock();
            if (OmsOrderRaw* ordupd = order->BeginUpdate(*req)) {
               OmsRequestRunnerInCore runner{resource, *ordupd, 0u};
               runner.Reject(f9fmkt_TradingRequestSt_QueuingCanceled, OmsErrCode_NoReadyLine, "System restart.");
            }
            items.lock();
         }
      }
   }
   fon9::RevBufferList rbuf{128};
   loader.MakeLayout(rbuf, resource.Core_.Owner_->RequestFactoryPark());
   loader.MakeLayout(rbuf, resource.Core_.Owner_->OrderFactoryPark());
   loader.MakeLayout(rbuf, resource.Core_.Owner_->EventFactoryPark());
   fon9::RevPrint(rbuf, "===== OMS start @ ", fon9::UtcNow(), " | ", logFileName, " =====\n");
   if (fsize > 0)
      fon9::RevPrint(rbuf, '\n');
   fon9::DcQueueList dcq{rbuf.MoveOut()};
   this->RecorderFd_.Append(dcq);
   return res;
}

} // namespaces
