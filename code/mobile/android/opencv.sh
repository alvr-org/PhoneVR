export ANDROID_NDK=/mnt/c/android-ndk-r13b
OCVPATH=/mnt/c/opencv/sources

cd ${OCVPATH}/platforms/android
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=${OCVPATH}/platforms/android/android.toolchain.cmake ..
make -j8