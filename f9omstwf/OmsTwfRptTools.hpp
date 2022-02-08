// \file f9omstwf/OmsTwfRptTools.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstwf_OmsTwfRptTools_hpp__
#define __f9omstwf_OmsTwfRptTools_hpp__
#include "f9twf/ExgMapMgr.hpp"
#include "f9omstw/OmsRequestBase.hpp"
#include "f9omstwf/OmsTwfTypes.hpp"

namespace f9omstw {

using ExgMaps = f9twf::ExgMapMgr::MapsConstLocker;

/// 設定 rpt 的基本資料:
/// - BrkId(根據 ivacFcmId);
/// - Market(Fut/Opt)
/// - TradingSessionId(Normal/AfterHour);
void SetupReport0Base(OmsRequestBase& rpt, f9twf::TmpFcmId ivacFcmId, f9twf::ExgSystemType exgSysType, const ExgMaps& exgMaps);

/// SetupReport0Base() 及 rptSymbol;
void SetupReport0Symbol(OmsRequestBase&             rpt,
                        f9twf::TmpFcmId             ivacFcmId,
                        const f9twf::TmpSymbolType* pkSym,
                        OmsTwfSymbol&               rptSymbol,
                        const ExgMaps&              exgMaps,
                        f9twf::ExgSystemType        exgSysType);

} // namespaces
#endif//__f9omstwf_OmsTwfRptTools_hpp__
