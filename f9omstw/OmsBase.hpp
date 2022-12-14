// \file f9omstw/OmsBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsBase_hpp__
#define __f9omstw_OmsBase_hpp__
#include "fon9/fmkt/TradingRequest.hpp"
#include "fon9/CharAryL.hpp"
#include <memory>

namespace f9omstw {

class OmsCore;
using OmsCoreSP = fon9::intrusive_ptr<OmsCore>;

class OmsCoreMgr;
using OmsCoreMgrSP = fon9::intrusive_ptr<OmsCoreMgr>;

class OmsTree;
class OmsResource;
class OmsBackend;
using OmsRxItem = fon9::fmkt::TradingRxItem;
using OmsRxSNO = fon9::fmkt::TradingRxSNO;

class OmsOrdNoMap;
using OmsOrdNoMapSP = fon9::intrusive_ptr<OmsOrdNoMap>;
using OmsOrdTeamGroupId = uint32_t;
class OmsOrdTeamGroupCfg;

/// 台灣證券券商代號 4 碼, 台灣期貨商代號 7 碼.
using OmsBrkId = fon9::CharAryL<7>;
inline bool OmsIsBrkIdEmpty(const OmsBrkId& brkId) {
   return brkId.empty();
}

/// 台灣證券、期權, 委託書號 5 碼.
using OmsOrdNo = fon9::CharAryF<5>;
inline bool OmsIsOrdNoEmpty(const OmsOrdNo& ordNo) {
   return ordNo.empty1st();
}

using OmsSubacNo = fon9::CharAry<10>;
using OmsSalesNo = fon9::CharAry<8>;

class OmsIvBase;
using OmsIvBaseSP = fon9::intrusive_ptr<OmsIvBase>;

class OmsBrkTree;
class OmsBrk;
using OmsBrkSP = fon9::intrusive_ptr<OmsBrk>;
class OmsMarketRec;
class OmsSessionRec;

class OmsIvacTree;
class OmsIvac;
using OmsIvacSP = fon9::intrusive_ptr<OmsIvac>;

class OmsSubac;
using OmsSubacSP = fon9::intrusive_ptr<OmsSubac>;

class OmsIvSymb;
using OmsIvSymbSP = fon9::intrusive_ptr<OmsIvSymb>;

class OmsRequestBase;
class OmsRequestTrade;
class OmsRequestIni;
class OmsRequestUpd;
class OmsReportFilled;

using OmsRequestSP = fon9::intrusive_ptr<OmsRequestBase>;
using OmsRequestTradeSP = fon9::intrusive_ptr<OmsRequestTrade>;

class OmsRequestFactory;
using OmsRequestFactorySP = fon9::intrusive_ptr<OmsRequestFactory>;

class OmsRequestRunStep;
using OmsRequestRunStepSP = std::unique_ptr<OmsRequestRunStep>;
class OmsRequestRptStep;
using OmsRequestRptStepSP = std::unique_ptr<OmsRequestRptStep>;

class OmsRequestRunner;
class OmsRequestRunnerInCore;
class OmsReportChecker;
class OmsReportRunnerInCore;

class OmsOrderRaw;
class OmsOrder;
class OmsOrderFactory;
using OmsOrderFactorySP = fon9::intrusive_ptr<OmsOrderFactory>;

struct OmsScResource;
struct OmsErrCodeAct;
using OmsErrCodeActSP = fon9::intrusive_ptr<const OmsErrCodeAct>;

class OmsEventFactory;
using OmsEventFactorySP = fon9::intrusive_ptr<OmsEventFactory>;

class OmsSymb;

using LgOut = fon9::fmkt::LgOut;
class OmsTradingLineMgrBase;

//--------------------------------------------------------------------------//

template <class T>
inline auto OmsIsSymbolEmpty(const T& symbid) -> decltype(symbid.empty()) {
   return symbid.empty();
}

template <size_t arysz, typename CharT, CharT kChFiller>
inline bool OmsIsSymbolEmpty(const fon9::CharAry<arysz, CharT, kChFiller>& symbid) {
   return symbid.empty1st();
}

//--------------------------------------------------------------------------//

/// 交易稅率.
using OmsTaxRate = fon9::Decimal<uint32_t, 9>;
/// 匯率.
using OmsExchangeRate = fon9::Decimal<uint64_t, 9>;

enum CurrencyIndex : uint8_t {
   /// 因 [非境外投資人] 除人民幣商品外, 整戶 [可動用(出金)保證金] 須排除人民幣,
   /// 所以將人民幣的 Index 設為 0, 可讓程式容易處理。
   CurrencyIndex_RMB = 0,
   CurrencyIndex_NTD = 1,
   CurrencyIndex_USD = 2,
   CurrencyIndex_JPY = 3,
   CurrencyIndex_Count,
   /// 雖然 >= CurrencyIndex_Count 就表示不正確,
   /// 但這裡提供一個值, 當確定無法支援, 或不正確的幣別,
   /// 可以直接填入, 讓 UI 查看時可以辨認。
   CurrencyIndex_Unsupport = 99
};
static inline bool IsCurrencyIndexUnsupport(CurrencyIndex i) {
   return i >= CurrencyIndex_Count;
}
static inline bool IsCurrencyIndexSupported(CurrencyIndex i) {
   return i < CurrencyIndex_Count;
}

} // namespaces
#endif//__f9omstw_OmsBase_hpp__
