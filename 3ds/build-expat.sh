#!/usr/bin/env bash

set -ex

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR=$SCRIPT_DIR/..
EXPAT_DIR=$ROOT_DIR/third_party/libexpat

cd $EXPAT_DIR/expat

export CFLAGS="-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -mword-relocations -Wno-psabi -fomit-frame-pointer -ffunction-sections"
export CXXFLAGS="${CFLAGS}"
export CPPFLAGS="-D__3DS__ -I${DEVKITPRO}/libctru/include"
export LDFLAGS="-L${DEVKITPRO}/libctru/lib"
export LIBS="-lctru -lm"

./buildconf.sh
./configure \
    --prefix=$DEVKITPRO/portlibs/3ds/ \
    --host=arm-none-eabi \
    --enable-static \
    --without-examples \
    --without-tests \
    --without-docbook \
    CC=$DEVKITARM/bin/arm-none-eabi-gcc \
    CXX=$DEVKITARM/bin/arm-none-eabi-g++ \
    AR=$DEVKITARM/bin/arm-none-eabi-ar \
    RANLIB=$DEVKITARM/bin/arm-none-eabi-ranlib \
    PKG_CONFIG=$DEVKITPRO/portlibs/3ds/bin/arm-none-eabi-pkg-config

make -j$(nproc)
make install

cd -
