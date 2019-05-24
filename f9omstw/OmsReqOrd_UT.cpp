// \file f9omstw/OmsReqOrd_UT.cpp
//
// 測試 OmsRequest/OmsOrder 的設計.
//
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9omstw/OmsCore.hpp"
#include "f9omstw/OmsRequestFactory.hpp"
#include "f9omstw/OmsRequestPolicy.hpp"
#include "f9omstw/OmsIvSymb.hpp"
#include "f9omstw/OmsOrderTws.hpp"

#include "fon9/ObjSupplier.hpp"
#include "fon9/CmdArgs.hpp"
#include "fon9/Log.hpp"
#include "fon9/seed/RawWr.hpp"

#include "fon9/TestTools.hpp"
//--------------------------------------------------------------------------//
#ifdef _DEBUG
const unsigned    kPoolObjCount = 1000;
#else
const unsigned    kPoolObjCount = 1000 * 100;
#endif

enum class AllocFrom {
   Supplier,
   Memory,
};
static AllocFrom  gAllocFrom{AllocFrom::Supplier};
//--------------------------------------------------------------------------//
class OmsOrderTwsFactory : public f9omstw::OmsOrderFactory {
   fon9_NON_COPY_NON_MOVE(OmsOrderTwsFactory);
   using base = f9omstw::OmsOrderFactory;

   using OmsOrderTwsRaw = f9omstw::OmsOrderTwsRaw;
   using RawSupplier = fon9::ObjSupplier<OmsOrderTwsRaw, kPoolObjCount>;
   RawSupplier::ThisSP RawSupplier_{RawSupplier::Make()};
   OmsOrderTwsRaw* MakeOrderRawImpl() override {
      if (gAllocFrom == AllocFrom::Supplier)
         return this->RawSupplier_->Alloc();
      return new OmsOrderTwsRaw{};
   }

   using OmsOrderTws = f9omstw::OmsOrder;
   using OrderSupplier = fon9::ObjSupplier<OmsOrderTws, kPoolObjCount>;
   OrderSupplier::ThisSP OrderSupplier_{OrderSupplier::Make()};
   OmsOrderTws* MakeOrderImpl() override {
      if (gAllocFrom == AllocFrom::Supplier)
         return this->OrderSupplier_->Alloc();
      return new OmsOrderTws{};
   }
public:
   fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list
   OmsOrderTwsFactory()
      : base(fon9::Named{"TwsOrd"}, f9omstw::MakeFieldsT<OmsOrderTwsRaw>()) {
   }
   fon9_MSC_WARN_POP;

   ~OmsOrderTwsFactory() {
   }
};
//--------------------------------------------------------------------------//
template <class OmsRequestBaseT>
class OmsRequestFactoryT : public f9omstw::OmsRequestFactory {
   fon9_NON_COPY_NON_MOVE(OmsRequestFactoryT);
   using base = f9omstw::OmsRequestFactory;

   struct OmsRequestT : public OmsRequestBaseT {
      fon9_NON_COPY_NON_MOVE(OmsRequestT);
      using base = OmsRequestBaseT;
      using base::base;
      OmsRequestT() = default;
      void NoReadyLineReject(fon9::StrView) override {
      }
   };
   using RequestSupplier = fon9::ObjSupplier<OmsRequestT, kPoolObjCount>;
   typename RequestSupplier::ThisSP RequestTape_{RequestSupplier::Make()};
   f9omstw::OmsRequestSP MakeRequestImpl() override {
      if (gAllocFrom == AllocFrom::Supplier)
         return this->RequestTape_->Alloc();
      return new OmsRequestT{};
   }
public:
   fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list
   template <class... ArgsT>
   OmsRequestFactoryT(ArgsT&&... args)
      : base(std::forward<ArgsT>(args)..., f9omstw::MakeFieldsT<OmsRequestT>()) {
   }
   fon9_MSC_WARN_POP;

   ~OmsRequestFactoryT() {
   }
};
using OmsRequestTwsNewFactory = OmsRequestFactoryT<f9omstw::OmsRequestTwsNew>;
using OmsRequestTwsChgFactory = OmsRequestFactoryT<f9omstw::OmsRequestTwsChg>;
using OmsRequestTwsMatchFactory = OmsRequestFactoryT<f9omstw::OmsRequestTwsMatch>;
//--------------------------------------------------------------------------//
f9omstw::OmsRequestRunner MakeOmsTradingRequest(f9omstw::OmsRequestFactoryPark& facPark, fon9::StrView reqstr) {
   f9omstw::OmsRequestRunner retval{reqstr};
   fon9::StrView tag = fon9::StrFetchNoTrim(reqstr, '|');
   if (auto* fac = facPark.GetFactory(tag)) {
      auto req = fac->MakeRequest();
      if (dynamic_cast<f9omstw::OmsTradingRequest*>(req.get()) == nullptr) {
         fon9_LOG_ERROR("MakeOmsTradingRequest|err=It's not a trading request factory|reqfac=", tag);
         return retval;
      }
      fon9::seed::SimpleRawWr wr{*req};
      fon9::StrView value;
      while(fon9::StrFetchTagValue(reqstr, tag, value)) {
         auto* fld = fac->Fields_.Get(tag);
         if (fld == nullptr) {
            fon9_LOG_ERROR("MakeOmsTradingRequest|err=Unknown field|reqfac=", fac->Name_, "|fld=", tag);
            return retval;
         }
         fld->StrToCell(wr, value);
      }
      retval.Request_ = std::move(fon9::static_pointer_cast<f9omstw::OmsTradingRequest>(req));
      return retval;
   }
   fon9_LOG_ERROR("MakeOmsTradingRequest|err=Unknown request factory|reqfac=", tag);
   return retval;
}
void PrintOmsRequest(f9omstw::OmsRequestBase& req) {
   fon9::RevBufferList     rbuf{128};
   fon9::seed::SimpleRawRd rd{req};
   for (size_t fidx = req.Creator_->Fields_.size(); fidx > 0;) {
      auto fld = req.Creator_->Fields_.Get(--fidx);
      fld->CellRevPrint(rd, nullptr, rbuf);
      fon9::RevPrint(rbuf, *fon9_kCSTR_CELLSPL, fld->Name_, '=');
   }
   fon9::RevPrint(rbuf, req.Creator_->Name_);
   puts(fon9::BufferTo<std::string>(rbuf.MoveOut()).c_str());
}
//--------------------------------------------------------------------------//
void TestRequestNewInCore(f9omstw::OmsResource& res, f9omstw::OmsOrderFactory& ordFactory, f9omstw::OmsRequestRunner&& reqNew) {
   // 開始新單流程:
   // 1. 設定 req 序號.
   res.PutRequestId(*reqNew.Request_);

   // 2. 取得相關資源: 商品、投資人帳號、投資人商品資料...
   //    - 檢查:
   //      - 是否為可用帳號?
   //      - 若帳號可用, 但 OmsResource 不存在該帳號?
   //        - 是否允許下單時建立帳號?
   //        - 或是 OmsResource 一律事先建好帳號資料, 若不存在該帳號, 則中斷下單?
   //    - 如果失敗 req.Abandon(), 使用回報機制通知 user(io session), 回報訂閱者需判斷 req.UserId_;
   f9omstw::OmsScResource  scRes;

   static unsigned gTestCount = 0;
   // abandon request:
   if (++gTestCount % 10 == 0) {
      reqNew.Request_->Abandon("Ivac not found.");
      res.Backend_.Append(*reqNew.Request_, std::move(reqNew.ExLog_));
      return;
   }

   // 3. 建立委託書, 開始執行新單步驟.
   //    開始 OmsRequestRunnerInCore 之後, req 就不能再變動了!
   //    接下來 req 將被凍結 , 除了 LastUpdated_ 不會再有異動.
   f9omstw::OmsRequestRunnerInCore  runner{res,
      *ordFactory.MakeOrder(*static_cast<f9omstw::OmsRequestNew*>(reqNew.Request_.get()), std::move(scRes)),
      std::move(reqNew.ExLog_), 256};
   // 4. 風控檢查.
   // 5. 排隊 or 送單.
   runner.OrderRaw_.OrderSt_ = f9fmkt_OrderSt_NewSending;
   runner.OrderRaw_.RequestSt_ = f9fmkt_TradingRequestSt_Sending;
}
void TestRequestChgInCore(f9omstw::OmsResource& res, f9omstw::OmsOrderFactory& ordFactory, f9omstw::OmsRequestRunner&& req) {
   res.PutRequestId(*req.Request_);
   auto* inireq = res.GetRequest(static_cast<f9omstw::OmsRequestUpd*>(req.Request_.get())->IniSNO());
   if (inireq == nullptr || inireq->LastUpdated() == nullptr) {
      req.Request_->Abandon("Order not found.");
      res.Backend_.Append(*req.Request_, std::move(req.ExLog_));
      return;
   }
   f9omstw::OmsRequestRunnerInCore  runner{res,
      *ordFactory.MakeOrderRaw(*inireq->LastUpdated()->Order_, *req.Request_),
      std::move(req.ExLog_), 256};
   runner.OrderRaw_.RequestSt_ = f9fmkt_TradingRequestSt_Sent;
}
//--------------------------------------------------------------------------//
int main(int argc, char* argv[]) {
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif
   fon9::AutoPrintTestInfo utinfo{"OmsRequest/OmsOrder"};

   gAllocFrom = static_cast<AllocFrom>(fon9::StrTo(fon9::GetCmdArg(argc, argv, "f", "allocfrom"), 0u));
   std::cout << "AllocFrom = " << (gAllocFrom == AllocFrom::Supplier ? "Supplier" : "Memory") << std::endl;
   const auto     outfn = fon9::GetCmdArg(argc, argv, "o", "out");
   const auto     isWait = fon9::GetCmdArg(argc, argv, "w", "wait");
   unsigned       testTimes = fon9::StrTo(fon9::GetCmdArg(argc, argv, "c", "count"), 0u);
   if (testTimes <= 0)
      testTimes = kPoolObjCount * 2;

   // 測試用, 不啟動 OmsThread: 把 main thread 當成 OmsThread.
   struct TestCore : public f9omstw::OmsCore {
      fon9_NON_COPY_NON_MOVE(TestCore);
      using base = f9omstw::OmsCore;
      TestCore(const char* outfn) : base{"ut"} {
         this->OrderFacPark_.reset(new f9omstw::OmsOrderFactoryPark(
            new OmsOrderTwsFactory
         ));
         this->RequestFacPark_.reset(new f9omstw::OmsRequestFactoryPark(
            new OmsRequestTwsNewFactory(fon9::Named{"TwsNew"}),
            new OmsRequestTwsChgFactory(fon9::Named{"TwsChg"}),
            new OmsRequestTwsMatchFactory(fon9::Named{"TwsMat"})
         ));

         this->TDay_ = fon9::UtcNow();
         this->Backend_.OpenReload(outfn, this->GetResource());
         this->Backend_.StartThread(this->Name_ + "_Backend");
      }
      ~TestCore() {
         this->Backend_.WaitForEndNow();
      }
      f9omstw::OmsResource& GetResource() {
         return *static_cast<f9omstw::OmsResource*>(this);
      }
   };
   TestCore                      testCore(outfn.empty() ? "OmsReqOrd_UT.log" : outfn.begin());
   f9omstw::OmsThreadTaskHandler omsCoreHandler{testCore};
   auto&                         coreResource = testCore.GetResource();

   // 下單欄位
   // --------
   // Market, SessionId 應可透過下單內容(商品、數量、價格)自動判斷.
   // RequestKind 若為改單, 可透過下單欄位(數量、價格、TIF)自動判斷.
   // ReqUID 由系統自行編製.
   // UserId, FromIp 由 io-session 填入.
   // SalesNo, Src 各券商的處理方式不盡相同, 可能提供者:
   //    - Client 端 Ap 或 user;
   //    - io-session 根據各種情況: UserId 或 FromIp 或...
   // OrdNo 綜合底下原因, OrdNo 會在 OmsRequest, OmsOrderRaw 裡面都有提供.
   //    - 可能允許 Client 自訂委託號: OrdNo=A1234 系統需檢查是否重複.
   //    - 或 Client 自訂委託櫃號: OrdNo=B1 然後由後續的步驟編序號.
   //    - OrdNo 在一個台灣券商交易系統內, 是有限資源,
   //      可能讓某系統專用1碼或2碼櫃號, 因此可用序號範圍: (62^4=14,776,336 或 62^3=238,328).
   //    - 所以券商可能要求: 送單前才編製委託書號.
   // IvacNoFlag 台灣證交所要求提供的欄位, 其實與投資人帳號關連不大, 應屬於下單來源.

   f9omstw::OmsRequestPolicySP   reqPolicy;
   fon9::StopWatch               stopWatch;
   stopWatch.ResetTimer();
   for (unsigned L = 0; L < testTimes; ++L) {
      // Create Request & 填入內容;
      auto reqNew = MakeOmsTradingRequest(*coreResource.RequestFacPark_,
"TwsNew|SesName=UT|Src=A|UsrDef=UD123|ClOrdId=C12|IvacNo=10|SubacNo=sa01|BrkId=8610|Side=B|Symbol=2317|Qty=8000|Pri=84.3|OType=0");
      // user thread 前置檢查.
      if (!reqNew.PreCheck()) {
         fprintf(stderr, "PreCheck|err=%s\n", fon9::BufferTo<std::string>(reqNew.ExLog_.MoveOut()).c_str());
         abort();
      }
      // 模擬已將 reqNew 丟給 OmsCore, 已進入 OmsCore 之後的步驟.
      TestRequestNewInCore(coreResource, *coreResource.OrderFacPark_->GetFactory(0), std::move(reqNew));
   }
   stopWatch.PrintResult("ReqNew ", testTimes);

   stopWatch.ResetTimer();
   for (unsigned L = 0; L < testTimes; ++L) {
      fon9::RevBufferFixedSize<1024> rbuf;
      fon9::RevPrint(rbuf, "TwsChg|IniSNO=", L, "|Market=T|SessionId=N|SesName=UT|Src=B|UsrDef=UD234|ClOrdId=C34|BrkId=8610|OrdNo=|Qty=0");
      auto req = MakeOmsTradingRequest(*coreResource.RequestFacPark_, ToStrView(rbuf));
      if (!req.PreCheck()) {
         fprintf(stderr, "PreCheck|err=%s\n", fon9::BufferTo<std::string>(req.ExLog_.MoveOut()).c_str());
         abort();
      }
      TestRequestChgInCore(coreResource, *coreResource.OrderFacPark_->GetFactory(0), std::move(req));
   }
   stopWatch.PrintResult("ReqChg ", testTimes);
   std::cout << "LastSNO = " << coreResource.Backend_.LastSNO() << std::endl;
   if (isWait.begin() && (isWait.empty() || fon9::toupper(static_cast<unsigned char>(*isWait.begin())) == 'Y')) {
      // 要查看資源用量(時間、記憶體...), 可透過 `/usr/bin/time` 指令:
      //    /usr/bin/time --verbose ~/devel/output/f9omstw/release/f9omstw/OmsReqOrd_UT
      // 或在結束前先暫停, 在透過其他外部工具查看:
      //    ~/devel/output/f9omstw/release/f9omstw/OmsReqOrd_UT -w
      std::cout << "Press <Enter> to quit.";
      getchar();
   }
}
