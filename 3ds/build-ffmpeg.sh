#!/usr/bin/env bash

set -ex

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR=$SCRIPT_DIR/..
FFMPEG_DIR=$ROOT_DIR/third_party/FFmpeg_for_3DS

cd $FFMPEG_DIR

./configure --enable-cross-compile --cross-prefix=/opt/devkitpro/devkitARM/bin/arm-none-eabi- --prefix=/opt/devkitpro/extra_lib \
            --cpu=armv6k --arch=arm --target-os=linux \
            --extra-cflags="-mfloat-abi=hard -mtune=mpcore -mtp=cp15 -D_POSIX_THREADS -I/opt/devkitpro/extra_lib/include" \
            --extra-ldflags="-mfloat-abi=hard -L/opt/devkitpro/extra_lib/lib"\
             --disable-filters --disable-devices --disable-bsfs --disable-parsers \
             --disable-hwaccels --disable-debug --disable-stripping --disable-programs \
             --disable-avdevice --disable-postproc --disable-avfilter --disable-decoders \
             --disable-demuxers --disable-encoders --disable-muxers --disable-asm \
             --disable-protocols --disable-txtpages --disable-podpages --disable-manpages \
             --disable-htmlpages --disable-doc \
             --enable-inline-asm --enable-vfp --enable-armv5te --enable-armv6 \
             --enable-decoder="h264,hevc,av1" \
             --enable-demuxer="h264,hevc,av1" \
             --enable-muxer="mp4,mp3,mp2,ac3" \
             --enable-protocol="file" \
             --enable-libx264 --enable-libdav1d --enable-gpl --enable-pthreads

make -j$(nproc)
make install

cd -
