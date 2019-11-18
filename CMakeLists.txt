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
 )
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")
include_directories(.)
include_directories(../fon9)

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
add_subdirectory(f9utw)
