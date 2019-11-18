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
)
include_directories(BEFORE .)
add_executable(f9utw ${f9utw_src})
target_link_libraries(f9utw fon9_s f9tws_s f9twf_s f9omstw_s f9omstws_s f9omstwf_s f9omsrc_s)
