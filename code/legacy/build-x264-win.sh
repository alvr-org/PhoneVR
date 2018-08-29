#!/bin/bash
set -x; #trap read debug ####toggle debug

#use win10/ubuntu bash, install mingw-w64 & make

CURRDIR=`pwd`
SDIR=$(dirname "$0")
BUILDDIR='/mnt/c/builds/x264'

cd /mnt/c/sources/x264

for ARCH in i686 x86_64; do
	./configure --enable-static --disable-shared --chroma-format=420 --enable-strip --enaple-pic --enable-win32thread --enable-opencl \
		--disable-avs --disable-swscale --disable-lavf --disable-ffms --disable-gpac --disable-lsmash \
		--host="${ARCH}-w64-mingw32" --cross-prefix="${ARCH}-w64-mingw32-" --prefix="${BUILDDIR}/${ARCH}"
	make -j8
	make install

	mkdir -p "${SDIR}/libs/x264/lib/${ARCH}"
	cp -r -T "${BUILDDIR}/${ARCH}/include" "${SDIR}/libs/x264/include"
	cp "${BUILDDIR}/${ARCH}/lib/libx264.a" "${SDIR}/libs/x264/lib/${ARCH}/libx264.lib"
done


 
cd $CURRDIR
