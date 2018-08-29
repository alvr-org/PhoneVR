#!/bin/bash
#set -x; trap read debug ####toggle debug

echo "############ This script only works on 64 bit MacOS or Linux ################"
#TODO: complete all architectures

### Set constants: ###
#Set NDKPATH before executing this script!!!
#export NDKPATH=/path/to/ndk
export NDKPATH=/Volumes/Data/Downloads/android-ndk-r13b
#macOS, linux, cygwin have different path styles!
ARCHS=(arm)                   # arm, arm-neon, arm64, x86, x86_64, mips, mips64
API=19                       # android platform api version






################################################
function fail {
  echo "##################### $1 ####################" && exit 1
}

BASEDIR=$(dirname $(realpath $0))
cd $BASEDIR

### Validate input
if [ ! -d ${NDKPATH}/toolchains ]; then
  fail 'NDKPATH invalid or unset'
fi
if [ ! -d './libav' ]; then
  fail 'libav folder not found'
fi

for ARCH in "${ARCHS[@]}"; do 
  case $ARCH in
    arm | arm-neon | arm64 | x86 | x86_64 | mips | mips64)
    ;;
    *)
      fail "invalid achitecture: $ARCH"
  esac
done

### Init variables ###

### Detect OS:
OS='linux'
if [ "$(uname)" == "Darwin" ]; then
  OS='darwin'
fi
###

if [ $OS == 'linux' ]; then
  N_CORES=$(nproc)
elif [ $OS == 'darwin' ]; then
  N_CORES=$(sysctl -n hw.ncpu)
fi

TEMP_PATH="${BASEDIR}/temp"

export PKG_CONFIG_LIBDIR="${TEMP_PATH}/lib/pkgconfig"

rm -rf $TEMP_PATH
for ARCH in "${ARCHS[@]}"; do

  case $ARCH in
    arm | arm-neon)
      ABI='arm'
      CROSS_PREFIX='arm-linux-androideabi'
    ;;
    arm64)
      ABI='arm'
      CROSS_PREFIX='aarch64-linux-android'
    ;;
    mips)
      ABI='arm'
      CROSS_PREFIX='mipsel-linux-android'
    ;;
    mips64)
      ABI='arm'
      CROSS_PREFIX='mips64el-linux-android'
    ;;
    x86)
      ABI='x86'
      CROSS_PREFIX='i686-linux-android'
    ;;
    x86_64)
      ABI='x86_64'
      CROSS_PREFIX='x86_64-linux-android'
    ;;
  esac
  TOOLCHAIN=$CROSS_PREFIX
  SYSROOT="${NDKPATH}/platforms/android-${API}/arch-${ARCH}"
  case $ARCH in
    x86)
      TOOLCHAIN='x86'
    ;;
    x86_64)
      TOOLCHAIN='x86_64'
    ;;
  esac

  TOOLCHAIN=$(find "${NDKPATH}/toolchains" -maxdepth 1 -type d -name "${TOOLCHAIN}-*")
  CROSS_PREFIX="${TOOLCHAIN}/prebuilt/${OS}-x86_64/bin/${CROSS_PREFIX}-"

############################################################################################################enable pic for x265


  # fix line endings
  #find libav -type f -print0 | xargs -0 -n 1 -P $N_CORES dos2unix

  ################ Make libav #################
  pushd libav
  make clean

  CFLAGS="-I${TEMP_PATH}/include -DANDROID -I${NDKPATH}/sources/cxx-stl/system/include -Wno-multichar -fno-exceptions" # -fno-rtti" #-fno-strict-overflow -fstack-protector-all'  ##### -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
  LDFLAGS="-L${TEMP_PATH}/lib" # -Wl,-z,relro -Wl,-z,now -pie"
  CXXFLAGS=""

  case $ARCH in
    arm | arm-neon)
      CPU='armv7-a'
      CFLAGS="$CFLAGS -march=armv7-a"
      LDFLAGS="$LDFLAGS -Wl,--fix-cortex-a8"
    ;;
    arm-neon)
      CPU='armv7-a' ############ original: cortex-a8
      CFLAGS="$CFLAGS -march=armv7-a -mfloat-abi=softfp -mfpu=neon"
      LDFLAGS="$LDFLAGS -Wl,--fix-cortex-a8"
    ;;
    x86)
      CPU='i686'
      CFLAGS="$CFLAGS -march=i686"
    ;;
  esac


  echo "############### INIT libav ${ARCH}: please wait #################"
  #legacy test support for x264
  ./configure --target-os='linux' --cross-prefix="$CROSS_PREFIX" --arch="$ABI" --cpu="$CPU" --enable-runtime-cpudetect \
      --sysroot="$SYSROOT" --enable-pic --enable-pthreads --disable-debug  --disable-avdevice --disable-avfilter \
      --enable-hardcoded-tables --disable-doc --disable-static --enable-shared --disable-programs  --enable-yasm \
      --disable-encoders --disable-muxers --disable-hwaccels --disable-bsfs \
      --disable-parsers  --enable-parser=aac  --enable-parser=hevc  --enable-parser=h264 \
      --disable-demuxers --enable-demuxer=aac --enable-demuxer=hevc --enable-demuxer=h264 --enable-demuxer=rtsp \
      --disable-decoders --enable-decoder=aac --enable-decoder=hevc --enable-decoder=h264 \
      --disable-protocols --enable-protocol=rtp --enable-protocol=udp --enable-protocol=tcp --enable-protocol=rtmp --enable-protocol=srtp \
      --prefix="${BASEDIR}/libav-build/${ARCH}" --extra-cflags="$CFLAGS" --extra-ldflags="$LDFLAGS" || fail 'configure failed'
  make -j${N_CORES} install || fail 'make failed'

  popd
  ############################################
  rm -rf $TEMP_PATH
done


echo '############ DONE! #############'

