set(f9omsrc_src
 OmsRcServer.cpp
 OmsRcServerFunc.cpp
 OmsRcClient.cpp
 OmsRcClientApi.cpp
 OmsGvTable.cpp
)
add_library(f9omsrc_s STATIC ${f9omsrc_src})
target_link_libraries(f9omsrc_s f9omstw_s fon9_s pthread dl)

add_executable(OmsRcAgents_UT OmsRcAgents_UT.cpp)
target_link_libraries(OmsRcAgents_UT f9omstw_s f9utws_s f9omsrc_s)

add_executable(OmsRcFramework_UT OmsRcFramework_UT.cpp OmsRcFramework_UT_C.c)
target_link_libraries(OmsRcFramework_UT f9omstw_s f9utws_s f9omsrc_s)

# ---
add_executable(OmsRcClient_UT OmsRcClient_UT.c)
target_link_libraries(OmsRcClient_UT f9omsrc_s f9omstw_s)
