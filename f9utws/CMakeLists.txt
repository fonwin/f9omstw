set(f9utws_src
 ../../fon9/fon9/framework/Fon9CoRun.cpp
 f9utws_main.cpp
 UtwsOmsCore.cpp
 UtwsExgSenderStep.cpp
 UtwsSymb.cpp
 UtwsIvSymb.cpp
 UtwsIvSc.cpp
 UtwsSubac.cpp
 UtwsIvac.cpp
 UtwsBrk.cpp
)
include_directories(BEFORE .)
add_executable(f9utws ${f9utws_src})
target_link_libraries(f9utws fon9_s f9tws_s f9omstw_s f9omstws_s f9omsrc_s)
