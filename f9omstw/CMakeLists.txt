set(f9omstw_src
 OmsCore.cpp
 OmsResource.cpp
 OmsTools.cpp
 IvacNo.cpp

 OmsTree.cpp
 OmsIvBase.cpp
 OmsBrk.cpp
 OmsBrkTree.cpp
 OmsIvac.cpp
 OmsIvacTree.cpp
 OmsSubac.cpp
 OmsIvSymb.cpp
 OmsSymbTree.cpp

 OmsRequest.cpp
 OmsRequestFactory.cpp
 OmsRequestRunner.cpp
 OmsRequestMgr.cpp
 OmsOrder.cpp

 OmsRequestPolicy.cpp
 OmsPoIvList.cpp
 OmsPoIvListAgent.cpp

 OmsRequestTws.cpp
 OmsOrderTws.cpp
)
add_library(f9omstw_s STATIC ${f9omstw_src})
target_link_libraries(f9omstw_s fon9_s pthread dl)

set (OmsRequestPolicy_UT_src
 OmsRequestPolicy_UT
 ../apps/f9omstw/utws/UtwsBrk.cpp
 ../apps/f9omstw/utws/UtwsIvac.cpp
 ../apps/f9omstw/utws/UtwsSubac.cpp
 ../apps/f9omstw/utws/UtwsIvSc.cpp
 ../apps/f9omstw/utws/UtwsIvSymb.cpp
)
add_executable(OmsRequestPolicy_UT ${OmsRequestPolicy_UT_src})
target_link_libraries(OmsRequestPolicy_UT f9omstw_s)
target_include_directories(OmsRequestPolicy_UT PUBLIC ../apps/f9omstw)

add_executable(OmsReqOrd_UT OmsReqOrd_UT.cpp)
target_link_libraries(OmsReqOrd_UT f9omstw_s)
