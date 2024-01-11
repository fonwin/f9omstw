set(f9omsrc_src
 OmsRcServer.cpp
 OmsRcServerFunc.cpp
 OmsRcClient.cpp
 OmsRcClientApi.cpp
 OmsRcSyn.cpp
)
add_library(f9omsrc_s STATIC ${f9omsrc_src})
target_link_libraries(f9omsrc_s f9omstw_s fon9_s ${f9_s_lib} pthread dl)

#########################################################################
############################### Unit Test ###############################
if(CMAKE_F9OMSTW_BUILD_UNIT_TEST)
   add_executable(OmsRcAgents_UT OmsRcAgents_UT.cpp)
   target_link_libraries(OmsRcAgents_UT f9omstw_s f9utw_s f9omsrc_s)

   add_executable(OmsRcFramework_UT OmsRcFramework_UT.cpp OmsRcFramework_UT_C.c)
   target_link_libraries(OmsRcFramework_UT f9omstw_s f9utw_s f9omsrc_s)
endif()
############################## Unit Test END ############################
#########################################################################

# ---
execute_process(COMMAND "../../fon9/make_proj_verinfo.sh" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                                                          RESULT_VARIABLE ret)
if(NOT ret EQUAL 0)
    message(FATAL_ERROR "Run make_proj_verinfo.sh ERROR: " ${ret})
endif()
# ---
set(f9omsrcapi_src
 ../../fon9/fon9/DecBase.cpp
 ../../fon9/fon9/FmtDef.cpp
 ../../fon9/fon9/RevFormat.cpp
 ../../fon9/fon9/StrTo.cpp
 ../../fon9/fon9/StrTools.cpp
 ../../fon9/fon9/TimeInterval.cpp
 ../../fon9/fon9/TimeStamp.cpp
 ../../fon9/fon9/ToStr.cpp
 ../../fon9/fon9/ToStrFmt.cpp

 ../../fon9/fon9/BitvDecode.cpp
 ../../fon9/fon9/BitvEncode.cpp

 ../../fon9/fon9/Appender.cpp
 ../../fon9/fon9/File.cpp
 ../../fon9/fon9/FileAppender.cpp
 ../../fon9/fon9/FilePath.cpp
 ../../fon9/fon9/LogFile.cpp
 ../../fon9/fon9/TimedFileName.cpp

 ../../fon9/fon9/CountDownLatch.cpp
 ../../fon9/fon9/CyclicBarrier.cpp
 ../../fon9/fon9/DefaultThreadPool.cpp
 ../../fon9/fon9/SchTask.cpp
 ../../fon9/fon9/ThreadId.cpp
 ../../fon9/fon9/Timer.cpp

 ../../fon9/fon9/Assert.c
 ../../fon9/fon9/Base64.cpp
 ../../fon9/fon9/Blob.c
 ../../fon9/fon9/ByteVector.cpp
 ../../fon9/fon9/CmdArgs.cpp
 ../../fon9/fon9/ConfigLoader.cpp
 ../../fon9/fon9/ConfigParser.cpp
 ../../fon9/fon9/ConsoleIO.cpp
 ../../fon9/fon9/CTools.c
 ../../fon9/fon9/CtrlBreakHandler.c
 ../../fon9/fon9/DllHandle.cpp
 ../../fon9/fon9/ErrC.cpp
 ../../fon9/fon9/FlowCounter.cpp
 ../../fon9/fon9/Log.cpp
 ../../fon9/fon9/Named.cpp
 ../../fon9/fon9/Outcome.cpp
 ../../fon9/fon9/Random.cpp
 ../../fon9/fon9/SimpleFactory.cpp
 ../../fon9/fon9/Tools.cpp

 ../../fon9/fon9/buffer/BufferList.cpp
 ../../fon9/fon9/buffer/BufferNode.cpp
 ../../fon9/fon9/buffer/BufferNodeWaiter.cpp
 ../../fon9/fon9/buffer/DcQueue.cpp
 ../../fon9/fon9/buffer/DcQueueList.cpp
 ../../fon9/fon9/buffer/MemBlock.cpp
 ../../fon9/fon9/buffer/RevBuffer.cpp
 ../../fon9/fon9/buffer/RevBufferList.cpp

 ../../fon9/fon9/auth/SaslClient.cpp
 ../../fon9/fon9/auth/SaslScramSha256Client.cpp
 ../../fon9/fon9/crypto/Sha256.cpp

 ../../fon9/fon9/framework/IoFactory.cpp
 ../../fon9/fon9/framework/IoFactoryDgram.cpp
 ../../fon9/fon9/framework/IoFactoryTcpClient.cpp
 ../../fon9/fon9/framework/IoManager.cpp

 ../../fon9/fon9/io/Device.cpp
 ../../fon9/fon9/io/IoBase.cpp
 ../../fon9/fon9/io/IoServiceArgs.cpp
 ../../fon9/fon9/io/Manager.cpp
 ../../fon9/fon9/io/RecvBuffer.cpp
 ../../fon9/fon9/io/SendBuffer.cpp
 ../../fon9/fon9/io/Server.cpp
 ../../fon9/fon9/io/Session.cpp

 ../../fon9/fon9/io/DgramBase.cpp
 ../../fon9/fon9/io/Socket.cpp
 ../../fon9/fon9/io/SocketAddress.cpp
 ../../fon9/fon9/io/SocketAddressDN.cpp
 ../../fon9/fon9/io/SocketClientConfig.cpp
 ../../fon9/fon9/io/SocketClientDevice.cpp
 ../../fon9/fon9/io/SocketConfig.cpp
 ../../fon9/fon9/io/TcpClientBase.cpp

 ../../fon9/fon9/FdrNotify.cpp
 ../../fon9/fon9/io/FdrSocket.cpp
 ../../fon9/fon9/io/FdrSocketClient.cpp
 ../../fon9/fon9/io/FdrTcpClient.cpp
 ../../fon9/fon9/io/FdrDgram.cpp
 ../../fon9/fon9/io/FdrService.cpp
 ../../fon9/fon9/io/FdrServiceEpoll.cpp

 ../../fon9/fon9/rc/Rc.cpp
 ../../fon9/fon9/rc/RcClientApi.cpp
 ../../fon9/fon9/rc/RcClientSession.cpp
 ../../fon9/fon9/rc/RcFuncConn.cpp
 ../../fon9/fon9/rc/RcFuncConnClient.cpp
 ../../fon9/fon9/rc/RcSeedVisitor.cpp
 ../../fon9/fon9/rc/RcSession.cpp

 ../../fon9/fon9/rc/RcSeedVisitorClient.cpp
 ../../fon9/fon9/rc/RcSeedVisitorClientCAPI.cpp
 ../../fon9/fon9/rc/RcSvc.cpp
 ../../fon9/fon9/rc/RcSvcAck.cpp
 ../../fon9/fon9/rc/RcSvcReq.cpp
 ../../fon9/fon9/rc/RcSvcReqSeed.cpp
 ../../fon9/fon9/rc/RcSvcReqSubr.cpp
 ../../fon9/fon9/rc/RcSvStreamDecoder.cpp

 ../../fon9/fon9/seed/Layout.cpp
 ../../fon9/fon9/seed/PodOp.cpp
 ../../fon9/fon9/seed/Raw.cpp
 ../../fon9/fon9/seed/SeedBase.cpp
 ../../fon9/fon9/seed/Tab.cpp
 ../../fon9/fon9/seed/Tree.cpp
 ../../fon9/fon9/seed/TreeOp.cpp

 ../../fon9/fon9/seed/Field.cpp
 ../../fon9/fon9/seed/FieldBytes.cpp
 ../../fon9/fon9/seed/FieldChars.cpp
 ../../fon9/fon9/seed/FieldCharsL.cpp
 ../../fon9/fon9/seed/FieldDyBlob.cpp
 ../../fon9/fon9/seed/FieldMaker.cpp

 ../../fon9/fon9/seed/Plugins.cpp
 ../../fon9/fon9/seed/SeedAcl.cpp

 ../../fon9/fon9/seed/MaTree.cpp
 ../../fon9/fon9/seed/NamedPark.cpp
 ../../fon9/fon9/seed/SysEnv.cpp

 ../f9omstw/OmsToolsC.c
 ../f9omstw/OmsMakeErrMsg.cpp
 OmsRcClient.cpp
 OmsRcClientApi.cpp
 OmsRcClientApiVer.c

 # vc 需要, gcc 不用?
 ../../fon9/fon9/seed/SeedSubr.cpp
 ../../fon9/fon9/seed/FieldSchCfgStr.cpp
 ../../fon9/fon9/fmkt/TradingRequest.cpp
)
# --------------------------------------------------------
set(dst_include ${CMAKE_CURRENT_BINARY_DIR}/include)
set(slnsrc      ${CMAKE_CURRENT_SOURCE_DIR}/../..)
execute_process(COMMAND rm -rf ${dst_include})

file(COPY ${slnsrc}/f9omstw/f9omsrc/OmsRc.h             DESTINATION ${dst_include}/f9omsrc)
file(COPY ${slnsrc}/f9omstw/f9omstw/OmsPoIvList.h       DESTINATION ${dst_include}/f9omstw)
file(COPY ${slnsrc}/f9omstw/f9omstw/OmsToolsC.h         DESTINATION ${dst_include}/f9omstw)
file(COPY ${slnsrc}/f9omstw/f9omstw/OmsMakeErrMsg.h     DESTINATION ${dst_include}/f9omstw)
file(COPY ${slnsrc}/f9omstw/f9omstw/OmsErrCode.h        DESTINATION ${dst_include}/f9omstw)
file(COPY ${slnsrc}/fon9/fon9/fmkt/FmktTypes.h          DESTINATION ${dst_include}/fon9/fmkt)
file(COPY ${slnsrc}/fon9/fon9/rc/RcClientApi.h          DESTINATION ${dst_include}/fon9/rc)
file(COPY ${slnsrc}/fon9/fon9/rc/Rc.h                   DESTINATION ${dst_include}/fon9/rc)
file(COPY ${slnsrc}/fon9/fon9/rc/RcSeedVisitor.h        DESTINATION ${dst_include}/fon9/rc)
file(COPY ${slnsrc}/fon9/fon9/io/IoState.h              DESTINATION ${dst_include}/fon9/io)
file(COPY ${slnsrc}/fon9/fon9/seed/FieldType.h          DESTINATION ${dst_include}/fon9/seed)
file(COPY ${slnsrc}/fon9/fon9/sys/Config.h              DESTINATION ${dst_include}/fon9/sys)
file(COPY ${slnsrc}/fon9/fon9/CStrView.h                DESTINATION ${dst_include}/fon9)
file(COPY ${slnsrc}/fon9/fon9/Assert.h                  DESTINATION ${dst_include}/fon9)
file(COPY ${slnsrc}/fon9/fon9/ConsoleIO.h               DESTINATION ${dst_include}/fon9)
file(COPY ${slnsrc}/fon9/fon9/CTools.h                  DESTINATION ${dst_include}/fon9)
file(COPY ${slnsrc}/fon9/fon9/CtrlBreakHandler.h        DESTINATION ${dst_include}/fon9)
file(COPY ${slnsrc}/fon9/f9tws/TwsTypes.h               DESTINATION ${dst_include}/f9tws)

# --------------------------------------------------------
add_library(f9omsrcapi_s STATIC ${f9omsrcapi_src})
target_link_libraries(f9omsrcapi_s pthread dl)
add_custom_command(TARGET f9omsrcapi_s POST_BUILD
                   COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../fon9/make_static_o.sh
                              libf9omsrcapi_s.o
                              libf9omsrcapi_s.a
                              ${CMAKE_CURRENT_SOURCE_DIR}/f9OmsRcApi.ver.txt
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   )

add_executable(OmsRcClient_UT OmsRcClient_UT.c)
target_link_libraries(OmsRcClient_UT f9omsrcapi_s)
# --------------------------------------------------------
add_library(f9omsrcapi SHARED ${f9omsrcapi_src})
target_link_libraries(f9omsrcapi pthread rt dl -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/f9OmsRcApi.ver.txt)

add_executable(OmsRcClient_UTD OmsRcClient_UT.c)
target_link_libraries(OmsRcClient_UTD f9omsrcapi)
# --------------------------------------------------------

