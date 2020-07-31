cmake_minimum_required(VERSION 3.8.3)

#use rtti, exceptions for asio library
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++1z -frtti -fexceptions -Wc++11-narrowing -Wreorder")

# find all src files
file(GLOB_RECURSE  MY_SRC
     ../../../../common/src/*.cpp
     ../../../mobile-common/*.cpp
     )

# add my library
add_library(native-lib  SHARED
            src/main/cpp/native-lib.cpp
            ${MY_SRC}
            )

target_include_directories(native-lib
                           PUBLIC ../../libraries/headers
                           PUBLIC ../../../../common/libs/asio
                           PUBLIC ../../../../common/libs/Eigen

                           # my headers
                           PUBLIC ../../../../common/src
                           PUBLIC ../../../mobile-common
                           )

target_link_libraries(native-lib
                      log android EGL GLESv3 mediandk OpenMAXAL #todo remove OpenMaxal?
                      ${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/jni/${ANDROID_ABI}/libgvr.so
                      )
