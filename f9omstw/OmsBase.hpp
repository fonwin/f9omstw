// \file f9omstw/OmsBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsBase_hpp__
#define __f9omstw_OmsBase_hpp__
#include "fon9/fmkt/Trading.hpp"
#include "fon9/CharAryL.hpp"
#include <memory>

namespace f9omstw {

class OmsCore;
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
/// 台灣證券、期權, 委託書號 5 碼.
using OmsOrdNo = fon9::CharAry<5>;

//
//    +-----------------+
//    |    OmsIvBase    |
//    +-----------------+
//      ↑      ↑      ↑
// OmsBrkc  OmsIvac  OmsSubac
//
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

//
//                        OmsRequestBase
//                           ↑       ↑
//               OmsRequestTrade   OmsRequestFilled
//                   ↑    ↑                  ↑
//        OmsRequestIni  OmsRequestUpd       ↑
//                   ↑    ↑                  ↑
//     OmsRequestTwsIni  OmsRequestTwsChg  OmsRequestTwsFilled  (類推 Twf)
//
class OmsRequestBase;
class OmsRequestTrade;
class OmsRequestIni;
class OmsRequestUpd;
class OmsRequestFilled;
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

class OmsOrderRaw;
class OmsOrder;
class OmsOrderFactory;
using OmsOrderFactorySP = fon9::intrusive_ptr<OmsOrderFactory>;
struct OmsScResource;

} // namespaces
#endif//__f9omstw_OmsBase_hpp__
