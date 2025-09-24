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

# 链接顺序: 先写依赖其它库的(whip -> metartc7 -> metartccore7), 然后系统/第三方库
target_link_libraries(testRTM
  yangwhip7
  metartc7
  metartccore7
  ssl crypto
  srtp2 usrsctp
  opus speexdsp
  pthread m dl
)

message(STATUS "Using METARTC_LIB_DIR=${METARTC_LIB_DIR}")
