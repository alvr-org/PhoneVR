cmake_minimum_required(VERSION 3.4.3)

#use rtti, exceptions for asio library ## -std=c++17
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -frtti -fexceptions -Wc++11-narrowing -Wreorder")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O3 -std=c++17 -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -std=c++17 -g")

set(ANDROID_STL c++_static)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)


file(GLOB_RECURSE LIB_SRC
    ../../../../../../common/libs/ifaddrs/*.c
    ../../../../../../common/libs/json/single_include/*.hpp
)
file(GLOB_RECURSE GVR_SRC
    ../../../../../../common/src/*.cpp
    ../../../../../mobile-common/*.cpp
)
IF (NOT "${ANDROID_ABI}" STREQUAL "x86_64")
    add_library(native-lib-gvr SHARED
        cpp/native-lib.cpp
        ${LIB_SRC}
        ${GVR_SRC}
    )

    target_include_directories(native-lib-gvr
        PUBLIC ../../libraries/headers
        PUBLIC ../../../../../../common/libs/asio/asio/include
        PUBLIC ../../../../../../common/libs/eigen/Eigen
        PUBLIC ../../../../../../common/libs/ifaddrs
        PUBLIC ../../../../../../common/libs/json/single_include/

        PUBLIC ../../../../../../common/src
        PUBLIC ../../../../../mobile-common
    )

    target_link_libraries(native-lib-gvr
        log android EGL GLESv3 mediandk
        ${libs_dir}/jni/${ANDROID_ABI}/libgvr.so
        ${libs_dir}/jni/${ANDROID_ABI}/libgvr_audio.so
    )
ENDIF ()
