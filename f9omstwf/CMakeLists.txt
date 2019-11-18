set(f9omstwf_src
 OmsTwfTypes.cpp
 OmsTwfExgMapMgr.cpp

 OmsTwfSenderStepG1.cpp
# OmsTwfSenderStepLg.cpp

 OmsTwfTradingLineMgr.cpp
 OmsTwfRptLineTmp.cpp
 OmsTwfTradingLineTmp.cpp
 OmsTwfLineTmpFactory.cpp

 OmsTwfOrder0.cpp
 OmsTwfRequest0.cpp
 OmsTwfReport3.cpp

 OmsTwfOrder1.cpp
 OmsTwfRequest1.cpp
 OmsTwfReport2.cpp

 OmsTwfOrder7.cpp
 OmsTwfRequest7.cpp
 OmsTwfReport8.cpp

 OmsTwfOrder9.cpp
 OmsTwfRequest9.cpp
 OmsTwfReport9.cpp

 OmsTwfFilled.cpp
)
add_library(f9omstwf_s STATIC ${f9omstwf_src})
target_link_libraries(f9omstwf_s f9omstw_s f9twf_s fon9_s pthread dl)
