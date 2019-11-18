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

/// 線路群組代號.
/// '0'..'9', 'A'..'Z'
enum class LgOut : char {
   /// 應使用 IsValidateLgOut(lgId); 來判斷 lgId 是否有效.
   /// LgOut::Unknown 只是用來初始化無效資料.
   Unknown = 0,
   /// 通用線路.
   Common = '0',
   Count = 36,
};
inline uint8_t LgOutToIndex(LgOut lg) {
   uint8_t idx = static_cast<uint8_t>(fon9::Alpha2Seq(static_cast<char>(lg)));
   return(idx < static_cast<uint8_t>(LgOut::Count) ? idx : static_cast<uint8_t>(0));
}
inline bool IsValidateLgOut(LgOut lgId) {
   return(fon9::isdigit(static_cast<unsigned char>(lgId))
          || fon9::isupper(static_cast<unsigned char>(lgId)));
}

} // namespaces
#endif//__f9omstw_OmsBase_hpp__
