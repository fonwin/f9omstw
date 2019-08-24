set(f9omstws_src
 OmsTwsRequest.cpp
 OmsTwsFilled.cpp
 OmsTwsReport.cpp
 OmsTwsReportTools.cpp
 OmsTwsOrder.cpp
 OmsTwsTradingLineMgr.cpp
 OmsTwsTradingLineMgrCfg.cpp
 OmsTwsTradingLineFix.cpp
 OmsTwsTradingLineFixSend.cpp
)
add_library(f9omstws_s STATIC ${f9omstws_src})
target_link_libraries(f9omstws_s f9omstw_s f9tws_s fon9_s pthread dl)
