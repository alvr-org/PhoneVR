cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -UNDEBUG")

add_library(native-lib   SHARED
            src/main/cpp/src/native-wrap.cpp )

#include_directories(src/main/cpp/vr
#                    ../../../../common/libs/asio
#                    ../../../../common/libs/Eigen
#                    ../../../../common/src
#                    ../../../mobile-common/src)

#source_directories(../../../mobile-common/src
#                    ../../../../common/src src/main/jni/src)

target_link_libraries(native-lib   android log EGL GLESv2 mediandk OpenMAXAL)
