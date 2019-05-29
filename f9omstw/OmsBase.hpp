// \file f9omstw/OmsBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsBase_hpp__
#define __f9omstw_OmsBase_hpp__
#include "fon9/CharAry.hpp"
#include "fon9/intrusive_ref_counter.hpp"
#include <memory>

namespace f9omstw {

class OmsCore;
class OmsTree;
class OmsResource;
class OmsBackend;
class OmsRxItem;

class OmsOrdNoMap;
using OmsOrdNoMapSP = fon9::intrusive_ptr<OmsOrdNoMap>;
using OmsOrdTeamGroupId = uint32_t;
class OmsOrdTeamGroupCfg;
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
//             OmsTradingRequest   OmsRequestMatch
//                   ↑    ↑                  ↑
//        OmsRequestNew  OmsRequestUpd       ↑
//                   ↑    ↑                  ↑
//     OmsRequestTwsNew  OmsRequestTwsChg  OmsRequestTwsMatch  (類推 Twf)
//
class OmsRequestBase;
class OmsTradingRequest;
class OmsRequestNew;
class OmsRequestUpd;
class OmsRequestMatch;
using OmsRequestSP = fon9::intrusive_ptr<OmsRequestBase>;
using OmsTradingRequestSP = fon9::intrusive_ptr<OmsTradingRequest>;

class OmsRequestFactory;
using OmsRequestFactorySP = fon9::intrusive_ptr<OmsRequestFactory>;

class OmsRequestRunStep;
using OmsRequestRunStepSP = std::unique_ptr<OmsRequestRunStep>;

class OmsRequestRunner;
class OmsRequestRunnerInCore;

class OmsOrderRaw;
class OmsOrder;
class OmsOrderFactory;
using OmsOrderFactorySP = fon9::intrusive_ptr<OmsOrderFactory>;
struct OmsScResource;

} // namespaces
#endif//__f9omstw_OmsBase_hpp__
