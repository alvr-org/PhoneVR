cmake_minimum_required(VERSION 3.4.3)

#use rtti, exceptions for asio library ## -std=c++17
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -frtti -fexceptions -Wc++11-narrowing -Wreorder")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O3 -std=c++17 -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -std=c++17 -g")

set(ANDROID_STL c++_static)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)

# find all src files
file(GLOB_RECURSE  MY_SRC
    ../../../../common/libs/ifaddrs/*.c
    ../../../../common/libs/json/single_include/*.hpp
    ../../../../common/src/*.cpp
    ../../../mobile-common/*.cpp
)

# add my library
add_library(native-lib SHARED
    src/main/cpp/native-lib.cpp
    src/main/cpp/alvr_main.cpp
    ${MY_SRC}
)

set(libs_dir ${CMAKE_CURRENT_SOURCE_DIR}/libraries)
set(alvr_build_dir ${CMAKE_CURRENT_SOURCE_DIR}/../ALVR/build/alvr_client_core)

target_include_directories(native-lib
    PUBLIC ./libraries/headers
    PUBLIC ../../../../common/libs/asio/asio/include
    PUBLIC ../../../../common/libs/eigen/Eigen
    PUBLIC ../../../../common/libs/ifaddrs
    PUBLIC ../../../../common/libs/json/single_include/

    # my headers
    PUBLIC ../../../../common/src
    PUBLIC ../../../mobile-common

    # ALVR Headers
    PUBLIC ${alvr_build_dir}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../cardboard
)

target_link_libraries(native-lib
    log android EGL GLESv3 mediandk OpenMAXAL #todo remove OpenMaxal?
    # ${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/jni/${ANDROID_ABI}/libgvr.so # TODO: Remove from src tree
    ${alvr_build_dir}/${ANDROID_ABI}/libalvr_client_core.so
    ${libs_dir}/jni/${ANDROID_ABI}/libGfxPluginCardboard.so
    ${libs_dir}/jni/${ANDROID_ABI}/libgvr.so
    ${libs_dir}/jni/${ANDROID_ABI}/libgvr_audio.so
)
