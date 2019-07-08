set(f9omstw_src
 IvacNo.cpp
 OmsCore.cpp
 OmsCoreByThread.cpp
 OmsResource.cpp
 OmsTools.cpp
 OmsBackend.cpp
 OmsBackendRecorder.cpp
 OmsOrdTeam.cpp
 OmsOrdNoMap.cpp

 OmsTree.cpp
 OmsIvBase.cpp
 OmsBrk.cpp
 OmsBrkTree.cpp
 OmsIvac.cpp
 OmsIvacTree.cpp
 OmsSubac.cpp
 OmsIvSymb.cpp
 OmsSymbTree.cpp

 OmsRequestId.cpp
 OmsRequestBase.cpp
 OmsRequestTrade.cpp
 OmsRequestFilled.cpp
 OmsRequestFactory.cpp
 OmsRequestRunner.cpp
 OmsOrder.cpp
 OmsOrderFactory.cpp
 OmsEventFactory.cpp

 OmsRequestPolicy.cpp
 OmsPoIvList.cpp
 OmsPoIvListAgent.cpp
 OmsPoUserRights.cpp
 OmsPoUserRightsAgent.cpp
)
add_library(f9omstw_s STATIC ${f9omstw_src})
target_link_libraries(f9omstw_s fon9_s pthread dl)

# ------------------------------------------------
# 使用 f9utws 的測試環境, 進行 omstw 的 unit test.
set (f9utws_src
 ../f9omstws/OmsTwsRequest.cpp
 ../f9omstws/OmsTwsOrder.cpp
 ../f9utws/UtwsOmsCore.cpp
 ../f9utws/UtwsBrk.cpp
 ../f9utws/UtwsIvac.cpp
 ../f9utws/UtwsSubac.cpp
 ../f9utws/UtwsIvSc.cpp
 ../f9utws/UtwsIvSymb.cpp
 ../f9utws/UtwsSymb.cpp
)
add_library(f9utws_s STATIC ${f9utws_src})
target_link_libraries(f9utws_s f9omstws_s f9omstw_s fon9_s pthread dl)

add_executable(OmsRequestPolicy_UT ../f9omstw/OmsRequestPolicy_UT.cpp)
target_link_libraries(OmsRequestPolicy_UT f9omstw_s f9utws_s)

add_executable(OmsOrdNoMap_UT ../f9omstw/OmsOrdNoMap_UT.cpp)
target_link_libraries(OmsOrdNoMap_UT f9omstw_s f9utws_s)

add_executable(OmsRequestTrade_UT ../f9omstw/OmsRequestTrade_UT.cpp)
target_link_libraries(OmsRequestTrade_UT f9omstw_s f9utws_s)

add_executable(OmsReqOrd_UT ../f9omstw/OmsReqOrd_UT.cpp)
target_link_libraries(OmsReqOrd_UT f9omstw_s f9utws_s)
