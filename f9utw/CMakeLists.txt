# ---
execute_process(COMMAND "../../fon9/make_proj_verinfo.sh" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                                                          RESULT_VARIABLE ret)
if(NOT ret EQUAL 0)
    message(FATAL_ERROR "Run make_proj_verinfo.sh ERROR: " ${ret})
endif()
# ---
set(f9utw_src
 ../../fon9/fon9/framework/Fon9CoRun.cpp
 f9utw_main.cpp
 UtwOmsCore.cpp
 UtwsSymb.cpp
 UtwsIvSymb.cpp
 UtwsIvSc.cpp
 UtwsSubac.cpp
 UtwsIvac.cpp
 UtwsBrk.cpp
 UtwSpCmd.cpp
 UtwSpCmdTwf.cpp
 UtwSpCmdTws.cpp
)
include_directories(BEFORE .)
add_executable(f9utw ${f9utw_src})
target_link_libraries(f9utw fon9_s f9tws_s f9twf_s f9omstw_s f9omstws_s f9omstwf_s f9omsrc_s ${f9_s_lib})
