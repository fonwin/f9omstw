set(f9omstw_src
 IvacNo.cpp
 OmsCore.cpp
 OmsCoreMgr.cpp
 OmsCoreMgrSeed.cpp
 OmsCoreByThread.cpp
 OmsResource.cpp
 OmsTools.cpp
 OmsToolsC.c
 OmsMakeErrMsg.cpp
 OmsBackend.cpp
 OmsBackendRecorder.cpp
 OmsOrdTeam.cpp
 OmsOrdNoMap.cpp
 OmsErrCodeAct.cpp
 OmsScBase.cpp
 OmsTradingLineMgr.cpp
 OmsTradingLineMgrCfg.cpp
 OmsImporter.cpp

 OmsTree.cpp
 OmsIvBase.cpp
 OmsBrk.cpp
 OmsBrkTree.cpp
 OmsIvac.cpp
 OmsIvacTree.cpp
 OmsSubac.cpp
 OmsIvSymb.cpp
 OmsSymb.cpp
 OmsSymbTree.cpp
 OmsErrCodeActTree.cpp

 OmsRequestId.cpp
 OmsRequestBase.cpp
 OmsRequestTrade.cpp
 OmsRequestFactory.cpp
 OmsRequestRunner.cpp

 OmsOrder.cpp
 OmsOrderFactory.cpp
 OmsEventFactory.cpp
 OmsEventSessionSt.cpp
 OmsReportFilled.cpp
 OmsReportRunner.cpp
 OmsReportTools.cpp

 OmsRequestPolicy.cpp
 OmsPoIvList.cpp
 OmsPoIvListAgent.cpp
 OmsPoUserRights.cpp
 OmsPoUserRightsAgent.cpp
)
add_library(f9omstw_s STATIC ${f9omstw_src})
target_link_libraries(f9omstw_s fon9_s pthread dl)

# 使用 f9utw 的測試環境, 進行 omstw 的 unit test.
# 因 f9utw 提供全部可用的下單要求及委託, 所以也可提供給一般用途的程式使用, 例如: OmsLogAnalyser;
set (f9utw_src
 ../f9omstws/OmsTwsRequest.cpp
 ../f9omstws/OmsTwsOrder.cpp
 ../f9utw/UtwOmsCore.cpp
 ../f9utw/UtwsBrk.cpp
 ../f9utw/UtwsIvac.cpp
 ../f9utw/UtwsSubac.cpp
 ../f9utw/UtwsIvSc.cpp
 ../f9utw/UtwsIvSymb.cpp
 ../f9utw/UtwsSymb.cpp
)
add_library(f9utw_s STATIC ${f9utw_src})
target_link_libraries(f9utw_s f9omstws_s f9omstwf_s f9omstw_s fon9_s pthread dl)

add_executable(OmsLogAnalyser ../f9omstw/OmsLogAnalyser.cpp)
target_link_libraries(OmsLogAnalyser f9omstw_s f9utw_s)

#########################################################################
############################### Unit Test ###############################
if(CMAKE_F9OMSTW_BUILD_UNIT_TEST)
   add_executable(OmsRequestPolicy_UT ../f9omstw/OmsRequestPolicy_UT.cpp)
   target_link_libraries(OmsRequestPolicy_UT f9omstw_s f9utw_s)

   add_executable(OmsOrdNoMap_UT ../f9omstw/OmsOrdNoMap_UT.cpp)
   target_link_libraries(OmsOrdNoMap_UT f9omstw_s f9utw_s)

   add_executable(OmsRequestTrade_UT ../f9omstw/OmsRequestTrade_UT.cpp)
   target_link_libraries(OmsRequestTrade_UT f9omstw_s f9utw_s)

   add_executable(OmsReqOrd_UT ../f9omstw/OmsReqOrd_UT.cpp)
   target_link_libraries(OmsReqOrd_UT f9omstw_s f9utw_s)

   add_executable(OmsReport_UT ../f9omstw/OmsReport_UT.cpp)
   target_link_libraries(OmsReport_UT f9omstw_s f9utw_s)

   add_executable(OmsTwfReport_UT ../f9omstwf/OmsTwfReport_UT.cpp)
   target_link_libraries(OmsTwfReport_UT f9omstw_s f9utw_s)

   add_executable(OmsTwfReportQuote_UT ../f9omstwf/OmsTwfReportQuote_UT.cpp)
   target_link_libraries(OmsTwfReportQuote_UT f9omstw_s f9utw_s)
endif()
############################## Unit Test END ############################
#########################################################################
