#!/bin/sh
#set -x; trap read debug ####toggle debug

SOURCE='ffmpeg-3.2.2'

function fail {
echo "##################### $1 ####################" && exit 1
}


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

BASEDIR=$(dirname $(realpath $0))

SRCDIR="${BASEDIR}/${SOURCE}"

if [ ! `which gas-preprocessor.pl` ]
then
    echo 'gas-preprocessor.pl not found. Trying to install...'
    (curl -L https://github.com/libav/gas-preprocessor/raw/master/gas-preprocessor.pl -o /usr/local/bin/gas-preprocessor.pl \
    && chmod +x /usr/local/bin/gas-preprocessor.pl) || fail 'installing preprocessor failed'
fi

if [ ! -r $SRCDIR ]
then
    echo 'FFmpeg source not found. Trying to download...'
    curl http://www.ffmpeg.org/releases/$SOURCE.tar.bz2 | tar xj || fail 'downloading/extracting failed'
fi


CFLAGS="-arch arm64 -mios-version-min=10.0 -fembed-bitcode"

pushd $SRCDIR
make clean
echo "############### INIT ffmpeg: please wait #################"

./configure --target-os=darwin --arch=arm64 --cc="xcrun -sdk iphoneos clang" --enable-cross-compile \
    --enable-pic --enable-pthreads --disable-debug  --disable-avdevice --disable-avfilter \
    --enable-hardcoded-tables --disable-doc --enable-static --disable-shared --disable-programs --enable-yasm \
    --disable-encoders --disable-muxers --disable-hwaccels --disable-bsfs \
    --disable-parsers  --enable-parser=aac  --enable-parser=hevc  --enable-parser=h264 \
    --disable-demuxers --enable-demuxer=aac --enable-demuxer=hevc --enable-demuxer=h264 --enable-demuxer=rtsp \
    --disable-decoders --enable-decoder=aac --enable-decoder=hevc --enable-decoder=h264 \
    --disable-protocols --enable-protocol=rtp --enable-protocol=udp --enable-protocol=tcp --enable-protocol=rtmp --enable-protocol=srtp \
    --prefix="${BASEDIR}/ffmpeg-ios" --extra-cflags="$CFLAGS" --extra-ldflags="$CFLAGS" || fail 'configure failed'

make -j${N_CORES} install GASPP_FIX_XCODE5=1 || fail 'make failed'
popd

echo '############ DONE! #############'
