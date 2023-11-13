cmake_minimum_required(VERSION 2.6)
project(f9omstw)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

set(CXX_FLAGS
 -g
 -Wall
 -Wextra
 -Wconversion
 -Wold-style-cast
 -Woverloaded-virtual
 -Wpointer-arith
 -Wshadow
 -Wwrite-strings
 -fexec-charset=UTF-8
 -std=c++11
 ${F9OMS_CXX_FLAGS}
 ${FON9_CXX_FLAGS}
 )
string(REPLACE ";" " " CXX_FLAGS "${CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_FLAGS}")

set(C_FLAGS
 -g
 -Wall
 -Wextra
 -Wconversion
 -Wpointer-arith
 -Wshadow
 -Wwrite-strings
 -fexec-charset=UTF-8
 ${F9OMS_C_FLAGS}
 ${FON9_C_FLAGS}
 )
string(REPLACE ";" " " C_FLAGS "${C_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS}")

include_directories(.)
include_directories(../fon9)
# ---
include(../fon9/AddF9card.cmake)
# ---
if(${CMAKE_BUILD_TYPE} MATCHES "Release|release")
  link_directories(~/devel/output/fon9/release/fon9)
  link_directories(~/devel/output/fon9/release/f9tws)
  link_directories(~/devel/output/fon9/release/f9twf)
else()
  link_directories(~/devel/output/fon9/debug/fon9)
  link_directories(~/devel/output/fon9/debug/f9tws)
  link_directories(~/devel/output/fon9/debug/f9twf)
endif()

add_subdirectory(f9omstw)
add_subdirectory(f9omstws)
add_subdirectory(f9omstwf)
add_subdirectory(f9omsrc)

if(CMAKE_F9OMSTW_BUILD_UNIT_TEST)
   add_subdirectory(f9utw)
endif()
