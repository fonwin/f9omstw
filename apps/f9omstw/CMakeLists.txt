set(f9omstw_src
 ../../../fon9/fon9/framework/Fon9CoRun.cpp
 f9omstw_main.cpp
 utws/UtwsOmsCore.cpp
 utws/UtwsSymb.cpp
 utws/UtwsIvSymb.cpp
 utws/UtwsIvSc.cpp
 utws/UtwsSubac.cpp
 utws/UtwsIvac.cpp
 utws/UtwsBrk.cpp
)
include_directories(BEFORE .)
add_executable(f9omstw ${f9omstw_src})
target_link_libraries(f9omstw fon9_s f9omstw_s)
