set(f9omstw_src
 IvacNo.cpp
 OmsCore.cpp
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
 OmsRequestMatch.cpp
 OmsRequestFactory.cpp
 OmsRequestRunner.cpp
 OmsOrder.cpp

 OmsRequestPolicy.cpp
 OmsPoIvList.cpp
 OmsPoIvListAgent.cpp
 OmsPoUserRights.cpp

 OmsRequestTws.cpp
 OmsOrderTws.cpp
)
add_library(f9omstw_s STATIC ${f9omstw_src})
target_link_libraries(f9omstw_s fon9_s pthread dl)

set (Utws_src
 ../apps/f9omstw/utws/UtwsBrk.cpp
 ../apps/f9omstw/utws/UtwsIvac.cpp
 ../apps/f9omstw/utws/UtwsSubac.cpp
 ../apps/f9omstw/utws/UtwsIvSc.cpp
 ../apps/f9omstw/utws/UtwsIvSymb.cpp
 ../apps/f9omstw/utws/UtwsSymb.cpp
)

add_executable(OmsRequestPolicy_UT OmsRequestPolicy_UT.cpp ${Utws_src})
target_link_libraries(OmsRequestPolicy_UT f9omstw_s)
target_include_directories(OmsRequestPolicy_UT PUBLIC ../apps/f9omstw)

add_executable(OmsOrdNoMap_UT OmsOrdNoMap_UT.cpp)
target_link_libraries(OmsOrdNoMap_UT f9omstw_s)

add_executable(OmsRequestTrade_UT OmsRequestTrade_UT.cpp ${Utws_src})
target_link_libraries(OmsRequestTrade_UT f9omstw_s)
target_include_directories(OmsRequestTrade_UT PUBLIC ../apps/f9omstw)

add_executable(OmsReqOrd_UT OmsReqOrd_UT.cpp ${Utws_src})
target_link_libraries(OmsReqOrd_UT f9omstw_s)
target_include_directories(OmsReqOrd_UT PUBLIC ../apps/f9omstw)
