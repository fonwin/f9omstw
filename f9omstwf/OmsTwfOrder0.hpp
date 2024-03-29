﻿// \file f9omstwf/OmsTwfOrder0.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfOrder0_hpp__
#define __f9omstwf_OmsTwfOrder0_hpp__
#include "f9omstw/OmsOrder.hpp"
#include "f9twf/ExgTmpTypes.hpp"

namespace f9omstw {

class OmsTwfOrder0 : public OmsOrder {
   fon9_NON_COPY_NON_MOVE(OmsTwfOrder0);
   using base = OmsOrder;

protected:
   OmsSymbSP FindSymb(OmsResource& res, const fon9::StrView& symbid) override;
   ~OmsTwfOrder0();

public:
   OmsTwfOrder0() = default;
};

/// 期權報價/詢價/一般委託基底.
struct OmsTwfOrderRawDat0 {
   /// 最後交易所異動時間.
   fon9::TimeStamp         LastExgTime_{fon9::TimeStamp::Null()};
   /// 為了避免與 TradingSessionId 名稱上混淆,
   /// 所以即使期交所「線路代號」的名稱為 SessionId,
   /// 這裡也選擇使用與 OmsTws 相同的命名.
   f9twf::TmpSessionId_t   OutPvcId_{};
   char                    Padding___[6];

   void ContinuePrevUpdate(const OmsTwfOrderRawDat0& prev) {
      memcpy(this, &prev, sizeof(*this));
   }
};

class OmsTwfOrderRaw0 : public OmsOrderRaw, public OmsTwfOrderRawDat0 {
   fon9_NON_COPY_NON_MOVE(OmsTwfOrderRaw0);
   using base = OmsOrderRaw;
public:
   using base::base;
   OmsTwfOrderRaw0() = default;
   ~OmsTwfOrderRaw0();

   void ContinuePrevUpdate(const OmsOrderRaw& prev) override;
   static void MakeFields(fon9::seed::Fields& flds);
};

} // namespaces
#endif//__f9omstwf_OmsTwfOrder0_hpp__
