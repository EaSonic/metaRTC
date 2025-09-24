cmake_minimum_required(VERSION 3.5)
project(testRTM C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(METARTC_LIB_DIR "${CMAKE_SOURCE_DIR}/../../bin/lib_release" CACHE PATH "Directory containing libmetartccore7.a")

include_directories(
    ${CMAKE_SOURCE_DIR}/../../include
    ${CMAKE_SOURCE_DIR}/../../thirdparty/include
    ${CMAKE_SOURCE_DIR}/../../thirdparty/user_include
)

link_directories(${METARTC_LIB_DIR})

add_executable(testRTM main.cpp RtpFlvWriter.cpp RtpFlvWriter.h)

target_link_libraries(testRTM metartccore7 yangwhip7 pthread m dl)

message(STATUS "Using METARTC_LIB_DIR=${METARTC_LIB_DIR}")
